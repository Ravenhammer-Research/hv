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
 * @file network_manager.c
 * @brief Network management for HVD
 * 
 * This file implements network management operations including bridge creation,
 * tap interface management, and network configuration.
 */

#include "common.h"

/**
 * Create a new network
 * @param network_name Network name
 * @param fib_id FIB ID for the network
 * @param physical_interface Physical interface to attach (optional)
 * @return 0 on success, -1 on failure
 */
int network_create(const char *network_name, uint32_t fib_id, const char *physical_interface) {
    // Create network structure in ZFS
    if (zfs_create_network_structure(network_name) != 0) {
        HVD_ERROR("Failed to create network structure for %s", network_name);
        return -1;
    }
    
    // Initialize network configuration
    network_def_t network = {0};
    strncpy(network.name, network_name, sizeof(network.name) - 1);
    network.type = NETWORK_TYPE_BRIDGE;
    network.fib_id = fib_id;
    if (physical_interface) {
        strncpy(network.physical_interface, physical_interface, sizeof(network.physical_interface) - 1);
    }
    snprintf(network.bridge_name, sizeof(network.bridge_name), "bridge_%s", network_name);
    
    // Save network configuration
    if (xml_save_network_config(&network) != 0) {
        HVD_ERROR("Failed to save network configuration for %s", network_name);
        char dataset_path[MAX_PATH_LEN];
        snprintf(dataset_path, sizeof(dataset_path), "%s/%s", NETWORK_BASE_PATH, network_name);
        zfs_destroy_dataset(dataset_path);
        return -1;
    }
    
    // Configure bridge via netd
    if (netd_configure_bridge(network.bridge_name, fib_id) != 0) {
        HVD_ERROR("Failed to configure bridge for network %s", network_name);
        char dataset_path[MAX_PATH_LEN];
        snprintf(dataset_path, sizeof(dataset_path), "%s/%s", NETWORK_BASE_PATH, network_name);
        zfs_destroy_dataset(dataset_path);
        return -1;
    }
    
    HVD_INFO("Created network: %s (bridge: %s, FIB: %u)", network_name, network.bridge_name, fib_id);
    return 0;
}

/**
 * Destroy a network
 * @param network_name Network name
 * @return 0 on success, -1 on failure
 */
int network_destroy(const char *network_name) {
    network_def_t network;
    if (xml_load_network_config(network_name, &network) != 0) {
        HVD_ERROR("Failed to load network configuration for %s", network_name);
        return -1;
    }
    
    // Remove bridge via netd
    if (netd_remove_bridge(network.bridge_name) != 0) {
        HVD_ERROR("Failed to remove bridge for network %s", network_name);
        return -1;
    }
    
    // Destroy network dataset
    char network_path[MAX_PATH_LEN];
    snprintf(network_path, sizeof(network_path), "%s/%s", NETWORK_BASE_PATH, network_name);
    
    if (zfs_destroy_dataset(network_path) != 0) {
        HVD_ERROR("Failed to destroy network dataset for %s", network_name);
        return -1;
    }
    
    HVD_INFO("Destroyed network: %s", network_name);
    return 0;
}

/**
 * Create a tap interface for a VM
 * @param vm_name VM name
 * @param network_name Network name
 * @param tap_name Tap interface name
 * @return 0 on success, -1 on failure
 */
int network_create_tap(const char *vm_name, const char *network_name, const char *tap_name) {
    network_def_t network;
    if (xml_load_network_config(network_name, &network) != 0) {
        HVD_ERROR("Failed to load network configuration for %s", network_name);
        return -1;
    }
    
    // Configure tap interface via netd
    if (netd_configure_tap(tap_name, network.bridge_name, network.fib_id) != 0) {
        HVD_ERROR("Failed to configure tap interface %s for VM %s", tap_name, vm_name);
        return -1;
    }
    
    HVD_INFO("Created tap interface %s for VM %s on network %s", tap_name, vm_name, network_name);
    return 0;
}

/**
 * Remove a tap interface
 * @param tap_name Tap interface name
 * @return 0 on success, -1 on failure
 */
int network_remove_tap(const char *tap_name) {
    if (netd_remove_tap(tap_name) != 0) {
        HVD_ERROR("Failed to remove tap interface %s", tap_name);
        return -1;
    }
    
    HVD_INFO("Removed tap interface: %s", tap_name);
    return 0;
}

/**
 * List all networks
 * @return 0 on success, -1 on failure
 */
int network_list(void) {
    DIR *dir = opendir(NETWORK_BASE_PATH);
    if (!dir) {
        HVD_ERROR("Failed to open network directory: %s", strerror(errno));
        return -1;
    }
    
    printf("Name                Type     FIB    Bridge              Physical Interface\n");
    printf("----                ----     ---    ------              ------------------\n");
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            network_def_t network;
            if (xml_load_network_config(entry->d_name, &network) == 0) {
                const char *type_str = "bridge";
                const char *physical = network.physical_interface[0] ? network.physical_interface : "-";
                printf("%-18s %-8s %-6u %-18s %s\n", 
                       network.name, type_str, network.fib_id, network.bridge_name, physical);
            }
        }
    }
    
    closedir(dir);
    return 0;
}

/**
 * Show network details
 * @param network_name Network name
 * @return 0 on success, -1 on failure
 */
int network_show(const char *network_name) {
    network_def_t network;
    if (xml_load_network_config(network_name, &network) != 0) {
        HVD_ERROR("Failed to load network configuration for %s", network_name);
        return -1;
    }
    
    printf("Network: %s\n", network.name);
    printf("  Type: bridge\n");
    printf("  FIB ID: %u\n", network.fib_id);
    printf("  Bridge: %s\n", network.bridge_name);
    printf("  Physical Interface: %s\n", 
           network.physical_interface[0] ? network.physical_interface : "none");
    
    return 0;
}

/**
 * Set network FIB ID
 * @param network_name Network name
 * @param fib_id New FIB ID
 * @return 0 on success, -1 on failure
 */
int network_set_fib(const char *network_name, uint32_t fib_id) {
    network_def_t network;
    if (xml_load_network_config(network_name, &network) != 0) {
        HVD_ERROR("Failed to load network configuration for %s", network_name);
        return -1;
    }
    
    network.fib_id = fib_id;
    
    // Update bridge FIB via netd
    char cmd[512];
    char response[MAX_RESPONSE_LEN];
    snprintf(cmd, sizeof(cmd), "set interface %s fib %u", network.bridge_name, fib_id);
    
    // This would use the netd integration function
    // For now, we'll just update the configuration
    if (xml_save_network_config(&network) != 0) {
        HVD_ERROR("Failed to save network configuration for %s", network_name);
        return -1;
    }
    
    HVD_INFO("Set FIB ID %u for network %s", fib_id, network_name);
    return 0;
}

/**
 * Set network physical interface
 * @param network_name Network name
 * @param physical_interface Physical interface name
 * @return 0 on success, -1 on failure
 */
int network_set_physical_interface(const char *network_name, const char *physical_interface) {
    network_def_t network;
    if (xml_load_network_config(network_name, &network) != 0) {
        HVD_ERROR("Failed to load network configuration for %s", network_name);
        return -1;
    }
    
    strncpy(network.physical_interface, physical_interface, sizeof(network.physical_interface) - 1);
    
    // Add physical interface to bridge via netd
    char cmd[512];
    char response[MAX_RESPONSE_LEN];
    snprintf(cmd, sizeof(cmd), "set interface %s bridge %s", physical_interface, network.bridge_name);
    
    // This would use the netd integration function
    // For now, we'll just update the configuration
    if (xml_save_network_config(&network) != 0) {
        HVD_ERROR("Failed to save network configuration for %s", network_name);
        return -1;
    }
    
    HVD_INFO("Set physical interface %s for network %s", physical_interface, network_name);
    return 0;
} 