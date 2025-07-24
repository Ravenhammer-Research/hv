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
 * @file commands.c
 * @brief Command parsing and routing for HVD
 * 
 * This file implements command parsing and routing for the hypervisor
 * management daemon CLI interface.
 */

#include "common.h"

/* Forward declarations for static functions */
static int handle_set_command(char *cmd, char *response, size_t resp_len);
static int handle_set_vm_command(char *cmd, char *response, size_t resp_len);
static int handle_set_network_command(char *cmd, char *response, size_t resp_len);
static int handle_show_command(char *cmd, char *response, size_t resp_len);
static int handle_create_command(char *cmd, char *response, size_t resp_len);
static int handle_destroy_command(char *cmd, char *response, size_t resp_len);
static int handle_start_command(char *cmd, char *response, size_t resp_len);
static int handle_stop_command(char *cmd, char *response, size_t resp_len);
static int handle_list_command(char *cmd, char *response, size_t resp_len);
static int handle_help_command(char *response, size_t resp_len);

/**
 * Parse and execute a command
 * @param cmd Command string to execute
 * @param response Buffer to store response
 * @param resp_len Size of response buffer
 * @return 0 on success, -1 on failure
 */
int execute_command(const char *cmd, char *response, size_t resp_len) {
    char *cmd_copy = strdup(cmd);
    if (!cmd_copy) {
        snprintf(response, resp_len, "ERROR: Memory allocation failed\n");
        return -1;
    }
    
    char *token = strtok(cmd_copy, " \t\n");
    if (!token) {
        snprintf(response, resp_len, "ERROR: Empty command\n");
        free(cmd_copy);
        return -1;
    }
    
    if (strcmp(token, "set") == 0) {
        return handle_set_command(cmd_copy, response, resp_len);
    } else if (strcmp(token, "show") == 0) {
        return handle_show_command(cmd_copy, response, resp_len);
    } else if (strcmp(token, "create") == 0) {
        return handle_create_command(cmd_copy, response, resp_len);
    } else if (strcmp(token, "destroy") == 0) {
        return handle_destroy_command(cmd_copy, response, resp_len);
    } else if (strcmp(token, "start") == 0) {
        return handle_start_command(cmd_copy, response, resp_len);
    } else if (strcmp(token, "stop") == 0) {
        return handle_stop_command(cmd_copy, response, resp_len);
    } else if (strcmp(token, "list") == 0) {
        return handle_list_command(cmd_copy, response, resp_len);
    } else if (strcmp(token, "help") == 0) {
        return handle_help_command(response, resp_len);
    } else {
        snprintf(response, resp_len, "ERROR: Unknown command '%s'\n", token);
        free(cmd_copy);
        return -1;
    }
}

/**
 * Handle set commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_set_command(char *cmd, char *response, size_t resp_len) {
    char *token = strtok(NULL, " \t\n");
    if (!token) {
        snprintf(response, resp_len, "ERROR: Missing object type (vm|network)\n");
        return -1;
    }
    
    if (strcmp(token, "vm") == 0) {
        return handle_set_vm_command(cmd, response, resp_len);
    } else if (strcmp(token, "network") == 0) {
        return handle_set_network_command(cmd, response, resp_len);
    } else {
        snprintf(response, resp_len, "ERROR: Unknown object type '%s'\n", token);
        return -1;
    }
}

/**
 * Handle set vm commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_set_vm_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract VM name and parameters
    char *vm_name = strtok(cmd, " \t\n");
    if (!vm_name) {
        snprintf(response, resp_len, "ERROR: Missing VM name\n");
        return -1;
    }
    
    // Skip "set" and "vm" tokens
    vm_name = strtok(NULL, " \t\n"); // "vm"
    vm_name = strtok(NULL, " \t\n"); // actual VM name
    if (!vm_name) {
        snprintf(response, resp_len, "ERROR: Missing VM name\n");
        return -1;
    }
    
    char *property = strtok(NULL, " \t\n");
    if (!property) {
        snprintf(response, resp_len, "ERROR: Missing property\n");
        return -1;
    }
    
    char *value = strtok(NULL, " \t\n");
    if (!value) {
        snprintf(response, resp_len, "ERROR: Missing value\n");
        return -1;
    }
    
    vm_config_t vm;
    if (xml_load_vm_config(vm_name, &vm) != 0) {
        snprintf(response, resp_len, "ERROR: VM '%s' not found\n", vm_name);
        return -1;
    }
    
    if (strcmp(property, "cpu") == 0) {
        int cpu = atoi(value);
        if (cpu <= 0 || cpu > 32) {
            snprintf(response, resp_len, "ERROR: Invalid CPU count (1-32)\n");
            return -1;
        }
        vm.cpu_cores = cpu;
    } else if (strcmp(property, "memory") == 0) {
        uint64_t memory = strtoull(value, NULL, 10);
        if (memory < 64 || memory > 1048576) {
            snprintf(response, resp_len, "ERROR: Invalid memory size (64-1048576 MB)\n");
            return -1;
        }
        vm.memory_mb = memory;
    } else if (strcmp(property, "boot-device") == 0) {
        strncpy(vm.boot_device, value, sizeof(vm.boot_device) - 1);
    } else {
        snprintf(response, resp_len, "ERROR: Unknown property '%s'\n", property);
        return -1;
    }
    
    if (xml_save_vm_config(&vm) != 0) {
        snprintf(response, resp_len, "ERROR: Failed to save VM configuration\n");
        return -1;
    }
    
    snprintf(response, resp_len, "OK: Set %s=%s for VM %s\n", property, value, vm_name);
    return 0;
}

/**
 * Handle set network commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_set_network_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract network name and parameters
    char *network_name = strtok(cmd, " \t\n");
    if (!network_name) {
        snprintf(response, resp_len, "ERROR: Missing network name\n");
        return -1;
    }
    
    // Skip "set" and "network" tokens
    network_name = strtok(NULL, " \t\n"); // "network"
    network_name = strtok(NULL, " \t\n"); // actual network name
    if (!network_name) {
        snprintf(response, resp_len, "ERROR: Missing network name\n");
        return -1;
    }
    
    char *property = strtok(NULL, " \t\n");
    if (!property) {
        snprintf(response, resp_len, "ERROR: Missing property\n");
        return -1;
    }
    
    char *value = strtok(NULL, " \t\n");
    if (!value) {
        snprintf(response, resp_len, "ERROR: Missing value\n");
        return -1;
    }
    
    if (strcmp(property, "fib") == 0) {
        uint32_t fib_id = atoi(value);
        if (fib_id > 255) {
            snprintf(response, resp_len, "ERROR: Invalid FIB ID (0-255)\n");
            return -1;
        }
        if (network_set_fib(network_name, fib_id) != 0) {
            snprintf(response, resp_len, "ERROR: Failed to set FIB ID\n");
            return -1;
        }
    } else if (strcmp(property, "physical-interface") == 0) {
        if (network_set_physical_interface(network_name, value) != 0) {
            snprintf(response, resp_len, "ERROR: Failed to set physical interface\n");
            return -1;
        }
    } else {
        snprintf(response, resp_len, "ERROR: Unknown property '%s'\n", property);
        return -1;
    }
    
    snprintf(response, resp_len, "OK: Set %s=%s for network %s\n", property, value, network_name);
    return 0;
}

/**
 * Handle show commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_show_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract object type and name
    char *object = strtok(cmd, " \t\n");
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type\n");
        return -1;
    }
    
    // Skip "show" token
    object = strtok(NULL, " \t\n"); // actual object type
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type (vm|network)\n");
        return -1;
    }
    
    if (strcmp(object, "vm") == 0) {
        char *vm_name = strtok(NULL, " \t\n");
        if (!vm_name) {
            snprintf(response, resp_len, "ERROR: Missing VM name\n");
            return -1;
        }
        
        // Capture output
        FILE *fp = tmpfile();
        if (!fp) {
            snprintf(response, resp_len, "ERROR: Failed to create temporary file\n");
            return -1;
        }
        
        int old_stdout = dup(STDOUT_FILENO);
        dup2(fileno(fp), STDOUT_FILENO);
        
        int result = vm_show(vm_name);
        
        dup2(old_stdout, STDOUT_FILENO);
        close(old_stdout);
        
        if (result != 0) {
            snprintf(response, resp_len, "ERROR: Failed to show VM details\n");
            fclose(fp);
            return -1;
        }
        
        rewind(fp);
        size_t bytes_read = fread(response, 1, resp_len - 1, fp);
        response[bytes_read] = '\0';
        fclose(fp);
        
        return 0;
    } else if (strcmp(object, "network") == 0) {
        char *network_name = strtok(NULL, " \t\n");
        if (!network_name) {
            snprintf(response, resp_len, "ERROR: Missing network name\n");
            return -1;
        }
        
        // Capture output
        FILE *fp = tmpfile();
        if (!fp) {
            snprintf(response, resp_len, "ERROR: Failed to create temporary file\n");
            return -1;
        }
        
        int old_stdout = dup(STDOUT_FILENO);
        dup2(fileno(fp), STDOUT_FILENO);
        
        int result = network_show(network_name);
        
        dup2(old_stdout, STDOUT_FILENO);
        close(old_stdout);
        
        if (result != 0) {
            snprintf(response, resp_len, "ERROR: Failed to show network details\n");
            fclose(fp);
            return -1;
        }
        
        rewind(fp);
        size_t bytes_read = fread(response, 1, resp_len - 1, fp);
        response[bytes_read] = '\0';
        fclose(fp);
        
        return 0;
    } else {
        snprintf(response, resp_len, "ERROR: Unknown object type '%s'\n", object);
        return -1;
    }
}

/**
 * Handle create commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_create_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract object type and parameters
    char *object = strtok(cmd, " \t\n");
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type\n");
        return -1;
    }
    
    // Skip "create" token
    object = strtok(NULL, " \t\n"); // actual object type
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type (vm|network)\n");
        return -1;
    }
    
    if (strcmp(object, "vm") == 0) {
        char *vm_name = strtok(NULL, " \t\n");
        if (!vm_name) {
            snprintf(response, resp_len, "ERROR: Missing VM name\n");
            return -1;
        }
        
        char *cpu_str = strtok(NULL, " \t\n");
        if (!cpu_str) {
            snprintf(response, resp_len, "ERROR: Missing CPU count\n");
            return -1;
        }
        
        char *memory_str = strtok(NULL, " \t\n");
        if (!memory_str) {
            snprintf(response, resp_len, "ERROR: Missing memory size\n");
            return -1;
        }
        
        int cpu = atoi(cpu_str);
        uint64_t memory = strtoull(memory_str, NULL, 10);
        
        if (vm_create(vm_name, cpu, memory) != 0) {
            snprintf(response, resp_len, "ERROR: Failed to create VM\n");
            return -1;
        }
        
        snprintf(response, resp_len, "OK: Created VM %s\n", vm_name);
        return 0;
    } else if (strcmp(object, "network") == 0) {
        char *network_name = strtok(NULL, " \t\n");
        if (!network_name) {
            snprintf(response, resp_len, "ERROR: Missing network name\n");
            return -1;
        }
        
        char *fib_str = strtok(NULL, " \t\n");
        if (!fib_str) {
            snprintf(response, resp_len, "ERROR: Missing FIB ID\n");
            return -1;
        }
        
        uint32_t fib_id = atoi(fib_str);
        char *physical_interface = strtok(NULL, " \t\n");
        
        if (network_create(network_name, fib_id, physical_interface) != 0) {
            snprintf(response, resp_len, "ERROR: Failed to create network\n");
            return -1;
        }
        
        snprintf(response, resp_len, "OK: Created network %s\n", network_name);
        return 0;
    } else {
        snprintf(response, resp_len, "ERROR: Unknown object type '%s'\n", object);
        return -1;
    }
}

/**
 * Handle destroy commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_destroy_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract object type and name
    char *object = strtok(cmd, " \t\n");
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type\n");
        return -1;
    }
    
    // Skip "destroy" token
    object = strtok(NULL, " \t\n"); // actual object type
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type (vm|network)\n");
        return -1;
    }
    
    char *name = strtok(NULL, " \t\n");
    if (!name) {
        snprintf(response, resp_len, "ERROR: Missing name\n");
        return -1;
    }
    
    if (strcmp(object, "vm") == 0) {
        if (vm_destroy(name) != 0) {
            snprintf(response, resp_len, "ERROR: Failed to destroy VM\n");
            return -1;
        }
        snprintf(response, resp_len, "OK: Destroyed VM %s\n", name);
    } else if (strcmp(object, "network") == 0) {
        if (network_destroy(name) != 0) {
            snprintf(response, resp_len, "ERROR: Failed to destroy network\n");
            return -1;
        }
        snprintf(response, resp_len, "OK: Destroyed network %s\n", name);
    } else {
        snprintf(response, resp_len, "ERROR: Unknown object type '%s'\n", object);
        return -1;
    }
    
    return 0;
}

/**
 * Handle start commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_start_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract VM name
    char *vm_name = strtok(cmd, " \t\n");
    if (!vm_name) {
        snprintf(response, resp_len, "ERROR: Missing VM name\n");
        return -1;
    }
    
    // Skip "start" token
    vm_name = strtok(NULL, " \t\n"); // actual VM name
    if (!vm_name) {
        snprintf(response, resp_len, "ERROR: Missing VM name\n");
        return -1;
    }
    
    if (vm_start(vm_name) != 0) {
        snprintf(response, resp_len, "ERROR: Failed to start VM\n");
        return -1;
    }
    
    snprintf(response, resp_len, "OK: Started VM %s\n", vm_name);
    return 0;
}

/**
 * Handle stop commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_stop_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract VM name
    char *vm_name = strtok(cmd, " \t\n");
    if (!vm_name) {
        snprintf(response, resp_len, "ERROR: Missing VM name\n");
        return -1;
    }
    
    // Skip "stop" token
    vm_name = strtok(NULL, " \t\n"); // actual VM name
    if (!vm_name) {
        snprintf(response, resp_len, "ERROR: Missing VM name\n");
        return -1;
    }
    
    if (vm_stop(vm_name) != 0) {
        snprintf(response, resp_len, "ERROR: Failed to stop VM\n");
        return -1;
    }
    
    snprintf(response, resp_len, "OK: Stopped VM %s\n", vm_name);
    return 0;
}

/**
 * Handle list commands
 * @param cmd Command string
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_list_command(char *cmd, char *response, size_t resp_len) {
    // Parse the command to extract object type
    char *object = strtok(cmd, " \t\n");
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type\n");
        return -1;
    }
    
    // Skip "list" token
    object = strtok(NULL, " \t\n"); // actual object type
    if (!object) {
        snprintf(response, resp_len, "ERROR: Missing object type (vm|network)\n");
        return -1;
    }
    
    // Capture output
    FILE *fp = tmpfile();
    if (!fp) {
        snprintf(response, resp_len, "ERROR: Failed to create temporary file\n");
        return -1;
    }
    
    int old_stdout = dup(STDOUT_FILENO);
    dup2(fileno(fp), STDOUT_FILENO);
    
    int result;
    if (strcmp(object, "vm") == 0) {
        result = vm_list();
    } else if (strcmp(object, "network") == 0) {
        result = network_list();
    } else {
        snprintf(response, resp_len, "ERROR: Unknown object type '%s'\n", object);
        fclose(fp);
        return -1;
    }
    
    dup2(old_stdout, STDOUT_FILENO);
    close(old_stdout);
    
    if (result != 0) {
        snprintf(response, resp_len, "ERROR: Failed to list %s\n", object);
        fclose(fp);
        return -1;
    }
    
    rewind(fp);
    size_t bytes_read = fread(response, 1, resp_len - 1, fp);
    response[bytes_read] = '\0';
    fclose(fp);
    
    return 0;
}

/**
 * Handle help command
 * @param response Response buffer
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on failure
 */
static int handle_help_command(char *response, size_t resp_len) {
    const char *help_text = 
        "HVD Commands:\n"
        "  create vm <name> <cpu> <memory>     Create a new VM\n"
        "  create network <name> <fib> [if]    Create a new network\n"
        "  destroy vm <name>                   Destroy a VM\n"
        "  destroy network <name>              Destroy a network\n"
        "  start <vm_name>                     Start a VM\n"
        "  stop <vm_name>                      Stop a VM\n"
        "  set vm <name> <prop> <value>        Set VM property\n"
        "  set network <name> <prop> <value>   Set network property\n"
        "  show vm <name>                      Show VM details\n"
        "  show network <name>                 Show network details\n"
        "  list vm                             List all VMs\n"
        "  list network                        List all networks\n"
        "  help                                Show this help\n"
        "\n"
        "VM Properties:\n"
        "  cpu <1-32>                          Number of CPU cores\n"
        "  memory <64-1048576>                 Memory in MB\n"
        "  boot-device <name>                  Boot device name\n"
        "\n"
        "Network Properties:\n"
        "  fib <0-255>                         FIB ID\n"
        "  physical-interface <name>           Physical interface\n";
    
    strncpy(response, help_text, resp_len - 1);
    response[resp_len - 1] = '\0';
    return 0;
} 