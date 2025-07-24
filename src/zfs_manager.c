/*
 * Copyright (c) 2025 Paige Thompson / Raven Hammer Research (paige@paige.bio)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file zfs_manager.c
 * @brief ZFS operations for HVD
 * 
 * This file implements ZFS operations for the hypervisor management daemon,
 * including dataset creation, zvol management, and property handling.
 */

#include "common.h"

/**
 * Create a ZFS dataset
 * @param dataset Dataset name
 * @param type Dataset type (filesystem, volume, etc.)
 * @return 0 on success, -1 on failure
 */
int zfs_create_dataset(const char *dataset, const char *type) {
    // Use the type parameter to determine dataset type
    zfs_type_t zfs_type = ZFS_TYPE_FILESYSTEM;
    if (type && strcmp(type, "volume") == 0) {
        zfs_type = ZFS_TYPE_VOLUME;
    }
    libzfs_handle_t *zfs_hdl = libzfs_init();
    if (!zfs_hdl) {
        HVD_ERROR("Failed to initialize libzfs");
        return -1;
    }
    
    zfs_handle_t *zhp = zfs_open(zfs_hdl, dataset, ZFS_TYPE_FILESYSTEM);
    if (zhp) {
        zfs_close(zhp);
        libzfs_fini(zfs_hdl);
        HVD_INFO("ZFS dataset %s already exists", dataset);
        return 0;
    }
    
    int result = zfs_create(zfs_hdl, dataset, zfs_type, NULL);
    if (result != 0) {
        HVD_ERROR("Failed to create ZFS dataset %s: %s", dataset, libzfs_error_description(zfs_hdl));
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    HVD_INFO("Created ZFS dataset: %s", dataset);
    libzfs_fini(zfs_hdl);
    return 0;
}

/**
 * Create a ZFS zvol
 * @param zvol_path Zvol path (e.g., "tank/vm/disk1")
 * @param size_gb Size in gigabytes
 * @return 0 on success, -1 on failure
 */
int zfs_create_zvol(const char *zvol_path, uint64_t size_gb) {
    libzfs_handle_t *zfs_hdl = libzfs_init();
    if (!zfs_hdl) {
        HVD_ERROR("Failed to initialize libzfs");
        return -1;
    }
    
    zfs_handle_t *zhp = zfs_open(zfs_hdl, zvol_path, ZFS_TYPE_VOLUME);
    if (zhp) {
        zfs_close(zhp);
        libzfs_fini(zfs_hdl);
        HVD_INFO("ZFS zvol %s already exists", zvol_path);
        return 0;
    }
    
    nvlist_t *props = NULL;
    int result = nvlist_alloc(&props, NV_UNIQUE_NAME, 0);
    if (result != 0) {
        HVD_ERROR("Failed to allocate nvlist for zvol properties");
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    char size_str[32];
    snprintf(size_str, sizeof(size_str), "%luG", size_gb);
    nvlist_add_string(props, "volsize", size_str);
    
    result = zfs_create(zfs_hdl, zvol_path, ZFS_TYPE_VOLUME, props);
    nvlist_free(props);
    
    if (result != 0) {
        HVD_ERROR("Failed to create ZFS zvol %s: %s", zvol_path, libzfs_error_description(zfs_hdl));
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    HVD_INFO("Created ZFS zvol: %s (%lu GB)", zvol_path, size_gb);
    libzfs_fini(zfs_hdl);
    return 0;
}

/**
 * Destroy a ZFS dataset
 * @param dataset Dataset name
 * @return 0 on success, -1 on failure
 */
int zfs_destroy_dataset(const char *dataset) {
    libzfs_handle_t *zfs_hdl = libzfs_init();
    if (!zfs_hdl) {
        HVD_ERROR("Failed to initialize libzfs");
        return -1;
    }
    
    zfs_handle_t *zhp = zfs_open(zfs_hdl, dataset, ZFS_TYPE_FILESYSTEM);
    if (!zhp) {
        HVD_INFO("ZFS dataset %s does not exist", dataset);
        libzfs_fini(zfs_hdl);
        return 0;
    }
    
    int result = zfs_destroy(zhp, B_FALSE);
    zfs_close(zhp);
    
    if (result != 0) {
        HVD_ERROR("Failed to destroy ZFS dataset %s: %s", dataset, libzfs_error_description(zfs_hdl));
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    HVD_INFO("Destroyed ZFS dataset: %s", dataset);
    libzfs_fini(zfs_hdl);
    return 0;
}

/**
 * Set a ZFS property
 * @param dataset Dataset name
 * @param property Property name
 * @param value Property value
 * @return 0 on success, -1 on failure
 */
int zfs_set_property(const char *dataset, const char *property, const char *value) {
    libzfs_handle_t *zfs_hdl = libzfs_init();
    if (!zfs_hdl) {
        HVD_ERROR("Failed to initialize libzfs");
        return -1;
    }
    
    zfs_handle_t *zhp = zfs_open(zfs_hdl, dataset, ZFS_TYPE_FILESYSTEM);
    if (!zhp) {
        HVD_ERROR("ZFS dataset %s does not exist", dataset);
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    int result = zfs_prop_set(zhp, property, value);
    zfs_close(zhp);
    
    if (result != 0) {
        HVD_ERROR("Failed to set ZFS property %s=%s on %s: %s", 
                  property, value, dataset, libzfs_error_description(zfs_hdl));
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    HVD_INFO("Set ZFS property %s=%s on %s", property, value, dataset);
    libzfs_fini(zfs_hdl);
    return 0;
}

/**
 * Get a ZFS property
 * @param dataset Dataset name
 * @param property Property name
 * @param value Buffer to store property value
 * @param len Size of value buffer
 * @return 0 on success, -1 on failure
 */
int zfs_get_property(const char *dataset, const char *property, char *value, size_t len) {
    libzfs_handle_t *zfs_hdl = libzfs_init();
    if (!zfs_hdl) {
        HVD_ERROR("Failed to initialize libzfs");
        return -1;
    }
    
    zfs_handle_t *zhp = zfs_open(zfs_hdl, dataset, ZFS_TYPE_FILESYSTEM);
    if (!zhp) {
        HVD_ERROR("ZFS dataset %s does not exist", dataset);
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    char prop_value[ZFS_MAXPROPLEN];
    int result = zfs_prop_get(zhp, zfs_name_to_prop(property), prop_value, sizeof(prop_value), NULL, NULL, 0, B_TRUE);
    zfs_close(zhp);
    
    if (result != 0) {
        HVD_ERROR("Failed to get ZFS property %s from %s: %s", property, dataset, libzfs_error_description(zfs_hdl));
        libzfs_fini(zfs_hdl);
        return -1;
    }
    
    strncpy(value, prop_value, len - 1);
    value[len - 1] = '\0';
    
    libzfs_fini(zfs_hdl);
    return 0;
}

/**
 * Create VM directory structure in ZFS
 * @param vm_name VM name
 * @return 0 on success, -1 on failure
 */
int zfs_create_vm_structure(const char *vm_name) {
    char vm_path[MAX_PATH_LEN];
    char disks_path[MAX_PATH_LEN];
    char state_path[MAX_PATH_LEN];
    
    snprintf(vm_path, sizeof(vm_path), "%s/%s", VM_BASE_PATH, vm_name);
    snprintf(disks_path, sizeof(disks_path), "%s/disks", vm_path);
    snprintf(state_path, sizeof(state_path), "%s/state", vm_path);
    
    // Create VM directory
    if (zfs_create_dataset(vm_path, "filesystem") != 0) {
        return -1;
    }
    
    // Create disks subdirectory
    if (zfs_create_dataset(disks_path, "filesystem") != 0) {
        zfs_destroy_dataset(vm_path);
        return -1;
    }
    
    // Create state subdirectory
    if (zfs_create_dataset(state_path, "filesystem") != 0) {
        zfs_destroy_dataset(disks_path);
        zfs_destroy_dataset(vm_path);
        return -1;
    }
    
    // Set properties for VM dataset
    zfs_set_property(vm_path, "hvd:type", "vm");
    zfs_set_property(vm_path, "hvd:name", vm_name);
    
    return 0;
}

/**
 * Create network directory structure in ZFS
 * @param network_name Network name
 * @return 0 on success, -1 on failure
 */
int zfs_create_network_structure(const char *network_name) {
    char network_path[MAX_PATH_LEN];
    
    snprintf(network_path, sizeof(network_path), "%s/%s", NETWORK_BASE_PATH, network_name);
    
    // Create network directory
    if (zfs_create_dataset(network_path, "filesystem") != 0) {
        return -1;
    }
    
    // Set properties for network dataset
    zfs_set_property(network_path, "hvd:type", "network");
    zfs_set_property(network_path, "hvd:name", network_name);
    
    return 0;
}

/**
 * Initialize the HVD ZFS structure
 * @return 0 on success, -1 on failure
 */
int zfs_init_hvd_structure(void) {
    // Create base directories if they don't exist
    if (zfs_create_dataset(HV_ROOT, "filesystem") != 0) {
        // Dataset might already exist, check if it's accessible
        if (access(HV_ROOT, F_OK) != 0) {
            HVD_ERROR("Failed to create or access HV root directory");
            return -1;
        }
    }
    
    if (zfs_create_dataset(VM_BASE_PATH, "filesystem") != 0) {
        if (access(VM_BASE_PATH, F_OK) != 0) {
            HVD_ERROR("Failed to create or access VM base directory");
            return -1;
        }
    }
    
    if (zfs_create_dataset(NETWORK_BASE_PATH, "filesystem") != 0) {
        if (access(NETWORK_BASE_PATH, F_OK) != 0) {
            HVD_ERROR("Failed to create or access network base directory");
            return -1;
        }
    }
    
    if (zfs_create_dataset(CONFIG_BASE_PATH, "filesystem") != 0) {
        if (access(CONFIG_BASE_PATH, F_OK) != 0) {
            HVD_ERROR("Failed to create or access config base directory");
            return -1;
        }
    }
    
    HVD_INFO("Initialized HVD ZFS structure");
    return 0;
} 