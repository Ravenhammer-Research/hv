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

#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/sysctl.h>

/* Define missing types that nvpair.h expects */
#ifndef _SYS_NVPAIR_H
typedef unsigned int uint_t;
typedef unsigned char uchar_t;
typedef int boolean_t;
typedef long long hrtime_t;
#define B_TRUE 1
#define B_FALSE 0
#endif

/* Define missing types that avl.h expects */
typedef unsigned long ulong_t;

/* Define missing header that libzfs.h expects */
#ifndef _SYS_MNTTAB_H
#define _SYS_MNTTAB_H
/* Empty stub - we don't need mnttab functionality */
#endif

#include <libzfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <bsdxml.h>

/* Configuration constants */
#define SOCKET_PATH "/var/run/hvd.sock"
#define HV_ROOT "/hv"
#define VM_BASE_PATH HV_ROOT "/vm"
#define NETWORK_BASE_PATH HV_ROOT "/networks"
#define CONFIG_BASE_PATH HV_ROOT "/config"
#define MAX_CMD_LEN 4096
#define MAX_RESPONSE_LEN 8192
#define MAX_PATH_LEN 512
#define MAX_NAME_LEN 64

/* VM states */
typedef enum {
    VM_STATE_STOPPED,
    VM_STATE_RUNNING,
    VM_STATE_PAUSED,
    VM_STATE_ERROR
} vm_state_t;

/* Disk types */
typedef enum {
    DISK_TYPE_ZVOL,
    DISK_TYPE_ISCSI
} disk_type_t;

/* Network types */
typedef enum {
    NETWORK_TYPE_BRIDGE
} network_type_t;

/* VM configuration structure */
typedef struct {
    char name[MAX_NAME_LEN];
    int cpu_cores;
    uint64_t memory_mb;
    char boot_device[MAX_NAME_LEN];
    vm_state_t state;
    char config_path[MAX_PATH_LEN];
} vm_config_t;

/* Disk configuration structure */
typedef struct {
    char name[MAX_NAME_LEN];
    disk_type_t type;
    char path[MAX_PATH_LEN];
    uint64_t size_gb;  /* For zvol creation */
    char iscsi_target[MAX_PATH_LEN];  /* For iSCSI */
} disk_config_t;

/* Network interface configuration */
typedef struct {
    char name[MAX_NAME_LEN];
    char network_name[MAX_NAME_LEN];
    char mac_address[18];
    uint32_t fib_id;
} network_config_t;

/* Network configuration structure */
typedef struct {
    char name[MAX_NAME_LEN];
    network_type_t type;
    uint32_t fib_id;
    char physical_interface[MAX_NAME_LEN];
    char bridge_name[MAX_NAME_LEN];
} network_def_t;

/* Function prototypes */
int zfs_create_dataset(const char *dataset, const char *type);
int zfs_create_zvol(const char *zvol_path, uint64_t size_gb);
int zfs_destroy_dataset(const char *dataset);
int zfs_set_property(const char *dataset, const char *property, const char *value);
int zfs_get_property(const char *dataset, const char *property, char *value, size_t len);
int zfs_create_vm_structure(const char *vm_name);
int zfs_create_network_structure(const char *network_name);
int zfs_init_hvd_structure(void);

int xml_save_vm_config(const vm_config_t *vm);
int xml_load_vm_config(const char *vm_name, vm_config_t *vm);
int xml_save_network_config(const network_def_t *network);
int xml_load_network_config(const char *network_name, network_def_t *network);

/* YANG-aware netd integration functions */
int netd_send_yang_config(const char *xml_config, char *response, size_t resp_len);
int netd_configure_bridge(const char *bridge_name, uint32_t fib_id);
int netd_configure_tap(const char *tap_name, const char *bridge_name, uint32_t fib_id);
int netd_add_interface_address(const char *interface_name, const char *ip_address, const char *family);
int netd_add_static_route(const char *destination, const char *gateway, uint32_t fib_id, const char *description);
int netd_remove_tap(const char *tap_name);
int netd_remove_bridge(const char *bridge_name);
int netd_check_availability(void);

/* Legacy function for backward compatibility */
int netd_send_command(const char *cmd, char *response, size_t resp_len);

int vm_create(const char *vm_name, int cpu_cores, uint64_t memory_mb);
int vm_start(const char *vm_name);
int vm_stop(const char *vm_name);
int vm_destroy(const char *vm_name);
int vm_add_disk(const char *vm_name, const char *disk_name, disk_type_t disk_type, 
                uint64_t size_gb, const char *iscsi_target);
int vm_remove_disk(const char *vm_name, const char *disk_name);
int vm_list(void);
int vm_show(const char *vm_name);

int network_create(const char *network_name, uint32_t fib_id, const char *physical_interface);
int network_destroy(const char *network_name);
int network_create_tap(const char *vm_name, const char *network_name, const char *tap_name);
int network_remove_tap(const char *tap_name);
int network_list(void);
int network_show(const char *network_name);
int network_set_fib(const char *network_name, uint32_t fib_id);
int network_set_physical_interface(const char *network_name, const char *physical_interface);

int execute_command(const char *cmd, char *response, size_t resp_len);

/* Error handling */
#define HVD_ERROR(msg, ...) fprintf(stderr, "HVD ERROR: " msg "\n", ##__VA_ARGS__)
#define HVD_INFO(msg, ...) fprintf(stderr, "HVD INFO: " msg "\n", ##__VA_ARGS__)

#endif /* COMMON_H */ 