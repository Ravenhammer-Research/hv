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
 * @file vm_manager.c
 * @brief VM lifecycle management for HVD
 * 
 * This file implements VM lifecycle operations including creation, starting,
 * stopping, and destruction of BHyve virtual machines.
 */

#include "common.h"

/**
 * Create a new VM
 * @param vm_name VM name
 * @param cpu_cores Number of CPU cores
 * @param memory_mb Memory in megabytes
 * @return 0 on success, -1 on failure
 */
int vm_create(const char *vm_name, int cpu_cores, uint64_t memory_mb) {
    // Create VM structure in ZFS
    if (zfs_create_vm_structure(vm_name) != 0) {
        HVD_ERROR("Failed to create VM structure for %s", vm_name);
        return -1;
    }
    
    // Initialize VM configuration
    vm_config_t vm = {0};
    strncpy(vm.name, vm_name, sizeof(vm.name) - 1);
    vm.cpu_cores = cpu_cores;
    vm.memory_mb = memory_mb;
    vm.state = VM_STATE_STOPPED;
    strncpy(vm.boot_device, "disk0", sizeof(vm.boot_device) - 1);
    
    // Save VM configuration
    if (xml_save_vm_config(&vm) != 0) {
        HVD_ERROR("Failed to save VM configuration for %s", vm_name);
        char dataset_path[MAX_PATH_LEN];
        snprintf(dataset_path, sizeof(dataset_path), "%s/%s", VM_BASE_PATH, vm_name);
        zfs_destroy_dataset(dataset_path);
        return -1;
    }
    
    HVD_INFO("Created VM: %s (CPU: %d, Memory: %lu MB)", vm_name, cpu_cores, memory_mb);
    return 0;
}

/**
 * Start a VM
 * @param vm_name VM name
 * @return 0 on success, -1 on failure
 */
int vm_start(const char *vm_name) {
    vm_config_t vm;
    if (xml_load_vm_config(vm_name, &vm) != 0) {
        HVD_ERROR("Failed to load VM configuration for %s", vm_name);
        return -1;
    }
    
    if (vm.state == VM_STATE_RUNNING) {
        HVD_INFO("VM %s is already running", vm_name);
        return 0;
    }
    
    // Build bhyve command
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), 
        "bhyve -c %d -m %luM -H -P -A -l com1,stdio "
        "-s 0:0,hostbridge -s 1:0,lpc -s 2:0,virtio-net,tap0 "
        "-s 3:0,virtio-blk,/dev/zvol/%s/%s/disks/disk0 "
        "-s 4:0,ahci-cd,/path/to/install.iso "
        "%s",
        vm.cpu_cores, vm.memory_mb, HV_ROOT, vm_name, vm_name);
    
    // Start VM in background
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - run bhyve using proper API
        // For now, we'll use a simplified approach without exec
        // In a full implementation, you'd use the BHyve API directly
        HVD_INFO("Starting VM %s with %d CPUs, %lu MB RAM", vm_name, vm.cpu_cores, vm.memory_mb);
        
        // Simulate VM startup (replace with actual BHyve API calls)
        sleep(1);
        exit(0);
    } else if (pid > 0) {
        // Parent process - update VM state
        vm.state = VM_STATE_RUNNING;
        xml_save_vm_config(&vm);
        
        // Save PID to state file
        char pid_file[MAX_PATH_LEN];
        snprintf(pid_file, sizeof(pid_file), "%s/%s/state/pid", VM_BASE_PATH, vm_name);
        FILE *fp = fopen(pid_file, "w");
        if (fp) {
            fprintf(fp, "%d", pid);
            fclose(fp);
        }
        
        HVD_INFO("Started VM: %s (PID: %d)", vm_name, pid);
        return 0;
    } else {
        HVD_ERROR("Failed to fork process for VM %s", vm_name);
        return -1;
    }
}

/**
 * Stop a VM
 * @param vm_name VM name
 * @return 0 on success, -1 on failure
 */
int vm_stop(const char *vm_name) {
    vm_config_t vm;
    if (xml_load_vm_config(vm_name, &vm) != 0) {
        HVD_ERROR("Failed to load VM configuration for %s", vm_name);
        return -1;
    }
    
    if (vm.state != VM_STATE_RUNNING) {
        HVD_INFO("VM %s is not running", vm_name);
        return 0;
    }
    
    // Read PID from state file
    char pid_file[MAX_PATH_LEN];
    snprintf(pid_file, sizeof(pid_file), "%s/%s/state/pid", VM_BASE_PATH, vm_name);
    FILE *fp = fopen(pid_file, "r");
    if (!fp) {
        HVD_ERROR("Failed to read PID file for VM %s", vm_name);
        return -1;
    }
    
    pid_t pid;
    if (fscanf(fp, "%d", &pid) != 1) {
        HVD_ERROR("Failed to read PID for VM %s", vm_name);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    // Send SIGTERM to VM process
    if (kill(pid, SIGTERM) != 0) {
        HVD_ERROR("Failed to send SIGTERM to VM %s (PID: %d)", vm_name, pid);
        return -1;
    }
    
    // Wait for process to terminate
    int status;
    if (waitpid(pid, &status, WNOHANG) == 0) {
        // Process still running, wait a bit more
        sleep(2);
        if (waitpid(pid, &status, WNOHANG) == 0) {
            // Force kill
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
        }
    }
    
    // Update VM state
    vm.state = VM_STATE_STOPPED;
    xml_save_vm_config(&vm);
    
    // Remove PID file
    unlink(pid_file);
    
    HVD_INFO("Stopped VM: %s", vm_name);
    return 0;
}

/**
 * Destroy a VM
 * @param vm_name VM name
 * @return 0 on success, -1 on failure
 */
int vm_destroy(const char *vm_name) {
    // Stop VM if running
    vm_stop(vm_name);
    
    // Destroy VM dataset
    char vm_path[MAX_PATH_LEN];
    snprintf(vm_path, sizeof(vm_path), "%s/%s", VM_BASE_PATH, vm_name);
    
    if (zfs_destroy_dataset(vm_path) != 0) {
        HVD_ERROR("Failed to destroy VM dataset for %s", vm_name);
        return -1;
    }
    
    HVD_INFO("Destroyed VM: %s", vm_name);
    return 0;
}

/**
 * Add a disk to a VM
 * @param vm_name VM name
 * @param disk_name Disk name
 * @param disk_type Disk type (zvol or iscsi)
 * @param size_gb Size in gigabytes (for zvol)
 * @param iscsi_target iSCSI target (for iscsi disks)
 * @return 0 on success, -1 on failure
 */
int vm_add_disk(const char *vm_name, const char *disk_name, disk_type_t disk_type, 
                uint64_t size_gb, const char *iscsi_target) {
    char zvol_path[MAX_PATH_LEN];
    snprintf(zvol_path, sizeof(zvol_path), "%s/%s/disks/%s", VM_BASE_PATH, vm_name, disk_name);
    
    if (disk_type == DISK_TYPE_ZVOL) {
        if (zfs_create_zvol(zvol_path, size_gb) != 0) {
            HVD_ERROR("Failed to create zvol for VM %s disk %s", vm_name, disk_name);
            return -1;
        }
    } else if (disk_type == DISK_TYPE_ISCSI) {
        // For iSCSI, we would configure the connection here
        // This is a placeholder for future iSCSI implementation
        HVD_INFO("iSCSI disk support not yet implemented");
        return -1;
    }
    
    HVD_INFO("Added disk %s to VM %s", disk_name, vm_name);
    return 0;
}

/**
 * Remove a disk from a VM
 * @param vm_name VM name
 * @param disk_name Disk name
 * @return 0 on success, -1 on failure
 */
int vm_remove_disk(const char *vm_name, const char *disk_name) {
    char zvol_path[MAX_PATH_LEN];
    snprintf(zvol_path, sizeof(zvol_path), "%s/%s/disks/%s", VM_BASE_PATH, vm_name, disk_name);
    
    if (zfs_destroy_dataset(zvol_path) != 0) {
        HVD_ERROR("Failed to destroy disk %s for VM %s", disk_name, vm_name);
        return -1;
    }
    
    HVD_INFO("Removed disk %s from VM %s", disk_name, vm_name);
    return 0;
}

/**
 * List all VMs
 * @return 0 on success, -1 on failure
 */
int vm_list(void) {
    DIR *dir = opendir(VM_BASE_PATH);
    if (!dir) {
        HVD_ERROR("Failed to open VM directory: %s", strerror(errno));
        return -1;
    }
    
    printf("Name                CPU    Memory    State\n");
    printf("----                ---    ------    -----\n");
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            vm_config_t vm;
            if (xml_load_vm_config(entry->d_name, &vm) == 0) {
                const char *state_str;
                switch (vm.state) {
                    case VM_STATE_RUNNING: state_str = "running"; break;
                    case VM_STATE_PAUSED: state_str = "paused"; break;
                    case VM_STATE_ERROR: state_str = "error"; break;
                    default: state_str = "stopped"; break;
                }
                printf("%-18s %-6d %-9lu %s\n", vm.name, vm.cpu_cores, vm.memory_mb, state_str);
            }
        }
    }
    
    closedir(dir);
    return 0;
}

/**
 * Show VM details
 * @param vm_name VM name
 * @return 0 on success, -1 on failure
 */
int vm_show(const char *vm_name) {
    vm_config_t vm;
    if (xml_load_vm_config(vm_name, &vm) != 0) {
        HVD_ERROR("Failed to load VM configuration for %s", vm_name);
        return -1;
    }
    
    const char *state_str;
    switch (vm.state) {
        case VM_STATE_RUNNING: state_str = "running"; break;
        case VM_STATE_PAUSED: state_str = "paused"; break;
        case VM_STATE_ERROR: state_str = "error"; break;
        default: state_str = "stopped"; break;
    }
    
    printf("VM: %s\n", vm.name);
    printf("  CPU: %d cores\n", vm.cpu_cores);
    printf("  Memory: %lu MB\n", vm.memory_mb);
    printf("  Boot Device: %s\n", vm.boot_device);
    printf("  State: %s\n", state_str);
    
    return 0;
} 