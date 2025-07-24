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
 * @file xml_config.c
 * @brief XML configuration management for HVD
 * 
 * This file implements XML configuration management using BSDXML for
 * storing and loading VM and network configurations.
 */

#include "common.h"

/* XML parsing state for VM configuration */
typedef struct {
    vm_config_t *vm;
    disk_config_t *current_disk;
    network_config_t *current_network;
    char current_element[64];
    char current_text[256];
    int in_disk;
    int in_network;
    int in_name;
    int in_cpu;
    int in_memory;
    int in_boot_device;
    int in_state;
    int in_type;
    int in_path;
    int in_size;
    int in_iscsi_target;
    int in_network_name;
    int in_mac_address;
    int in_fib_id;
} vm_xml_state_t;

/* XML parsing state for network configuration */
typedef struct {
    network_def_t *network;
    char current_element[64];
    char current_text[256];
    int in_name;
    int in_type;
    int in_fib_id;
    int in_physical_interface;
    int in_bridge_name;
} network_xml_state_t;

/**
 * XML start element handler for VM configuration
 */
static void XMLCALL vm_start_element_handler(void *userData, const XML_Char *name, const XML_Char **atts __attribute__((unused))) {
    vm_xml_state_t *state = (vm_xml_state_t *)userData;
    strncpy(state->current_element, name, sizeof(state->current_element) - 1);
    state->current_element[sizeof(state->current_element) - 1] = '\0';
    
    if (strcmp(name, "disk") == 0) {
        state->in_disk = 1;
        state->current_disk = malloc(sizeof(disk_config_t));
        memset(state->current_disk, 0, sizeof(disk_config_t));
    } else if (strcmp(name, "network") == 0) {
        state->in_network = 1;
        state->current_network = malloc(sizeof(network_config_t));
        memset(state->current_network, 0, sizeof(network_config_t));
    } else if (strcmp(name, "name") == 0) {
        state->in_name = 1;
    } else if (strcmp(name, "cpu") == 0) {
        state->in_cpu = 1;
    } else if (strcmp(name, "memory") == 0) {
        state->in_memory = 1;
    } else if (strcmp(name, "boot-device") == 0) {
        state->in_boot_device = 1;
    } else if (strcmp(name, "state") == 0) {
        state->in_state = 1;
    } else if (strcmp(name, "type") == 0) {
        state->in_type = 1;
    } else if (strcmp(name, "path") == 0) {
        state->in_path = 1;
    } else if (strcmp(name, "size") == 0) {
        state->in_size = 1;
    } else if (strcmp(name, "iscsi-target") == 0) {
        state->in_iscsi_target = 1;
    } else if (strcmp(name, "network-name") == 0) {
        state->in_network_name = 1;
    } else if (strcmp(name, "mac-address") == 0) {
        state->in_mac_address = 1;
    } else if (strcmp(name, "fib-id") == 0) {
        state->in_fib_id = 1;
    }
}

/**
 * XML end element handler for VM configuration
 */
static void XMLCALL vm_end_element_handler(void *userData, const XML_Char *name) {
    vm_xml_state_t *state = (vm_xml_state_t *)userData;
    
    if (strcmp(name, "name") == 0 && !state->in_disk && !state->in_network) {
        strncpy(state->vm->name, state->current_text, sizeof(state->vm->name) - 1);
        state->in_name = 0;
    } else if (strcmp(name, "cpu") == 0) {
        state->vm->cpu_cores = atoi(state->current_text);
        state->in_cpu = 0;
    } else if (strcmp(name, "memory") == 0) {
        state->vm->memory_mb = strtoull(state->current_text, NULL, 10);
        state->in_memory = 0;
    } else if (strcmp(name, "boot-device") == 0) {
        strncpy(state->vm->boot_device, state->current_text, sizeof(state->vm->boot_device) - 1);
        state->in_boot_device = 0;
    } else if (strcmp(name, "state") == 0) {
        if (strcmp(state->current_text, "running") == 0) {
            state->vm->state = VM_STATE_RUNNING;
        } else if (strcmp(state->current_text, "paused") == 0) {
            state->vm->state = VM_STATE_PAUSED;
        } else if (strcmp(state->current_text, "error") == 0) {
            state->vm->state = VM_STATE_ERROR;
        } else {
            state->vm->state = VM_STATE_STOPPED;
        }
        state->in_state = 0;
    } else if (strcmp(name, "disk") == 0) {
        state->in_disk = 0;
        // Note: In a full implementation, you'd add the disk to a list
        free(state->current_disk);
        state->current_disk = NULL;
    } else if (strcmp(name, "network") == 0) {
        state->in_network = 0;
        // Note: In a full implementation, you'd add the network to a list
        free(state->current_network);
        state->current_network = NULL;
    }
    
    memset(state->current_text, 0, sizeof(state->current_text));
}

/**
 * XML character data handler for VM configuration
 */
static void XMLCALL vm_character_data_handler(void *userData, const XML_Char *s, int len) {
    vm_xml_state_t *state = (vm_xml_state_t *)userData;
    strncat(state->current_text, s, len);
}

/**
 * Save VM configuration to XML file
 * @param vm VM configuration to save
 * @return 0 on success, -1 on failure
 */
int xml_save_vm_config(const vm_config_t *vm) {
    char config_path[MAX_PATH_LEN];
    snprintf(config_path, sizeof(config_path), "%s/%s/config.xml", VM_BASE_PATH, vm->name);
    
    FILE *fp = fopen(config_path, "w");
    if (!fp) {
        HVD_ERROR("Failed to open VM config file %s: %s", config_path, strerror(errno));
        return -1;
    }
    
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<vm-config xmlns=\"urn:hvd:vm\">\n");
    fprintf(fp, "  <name>%s</name>\n", vm->name);
    fprintf(fp, "  <cpu>%d</cpu>\n", vm->cpu_cores);
    fprintf(fp, "  <memory>%lu</memory>\n", vm->memory_mb);
    fprintf(fp, "  <boot-device>%s</boot-device>\n", vm->boot_device);
    
    const char *state_str;
    switch (vm->state) {
        case VM_STATE_RUNNING: state_str = "running"; break;
        case VM_STATE_PAUSED: state_str = "paused"; break;
        case VM_STATE_ERROR: state_str = "error"; break;
        default: state_str = "stopped"; break;
    }
    fprintf(fp, "  <state>%s</state>\n", state_str);
    
    fprintf(fp, "</vm-config>\n");
    
    fclose(fp);
    HVD_INFO("Saved VM configuration: %s", config_path);
    return 0;
}

/**
 * Load VM configuration from XML file
 * @param vm_name VM name
 * @param vm VM configuration structure to populate
 * @return 0 on success, -1 on failure
 */
int xml_load_vm_config(const char *vm_name, vm_config_t *vm) {
    char config_path[MAX_PATH_LEN];
    snprintf(config_path, sizeof(config_path), "%s/%s/config.xml", VM_BASE_PATH, vm_name);
    
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        HVD_ERROR("Failed to open VM config file %s: %s", config_path, strerror(errno));
        return -1;
    }
    
    // Read file content
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *xml_content = malloc(file_size + 1);
    if (!xml_content) {
        HVD_ERROR("Failed to allocate memory for XML content");
        fclose(fp);
        return -1;
    }
    
    size_t bytes_read = fread(xml_content, 1, file_size, fp);
    xml_content[bytes_read] = '\0';
    fclose(fp);
    
    // Parse XML
    XML_Parser parser = XML_ParserCreate(NULL);
    if (!parser) {
        HVD_ERROR("Failed to create XML parser");
        free(xml_content);
        return -1;
    }
    
    vm_xml_state_t state = {0};
    state.vm = vm;
    
    XML_SetUserData(parser, &state);
    XML_SetElementHandler(parser, vm_start_element_handler, vm_end_element_handler);
    XML_SetCharacterDataHandler(parser, vm_character_data_handler);
    
    if (XML_Parse(parser, xml_content, bytes_read, 1) != XML_STATUS_OK) {
        HVD_ERROR("Failed to parse VM config XML: %s", XML_ErrorString(XML_GetErrorCode(parser)));
        XML_ParserFree(parser);
        free(xml_content);
        return -1;
    }
    
    XML_ParserFree(parser);
    free(xml_content);
    
    HVD_INFO("Loaded VM configuration: %s", vm_name);
    return 0;
}

/**
 * XML start element handler for network configuration
 */
static void XMLCALL network_start_element_handler(void *userData, const XML_Char *name, const XML_Char **atts __attribute__((unused))) {
    network_xml_state_t *state = (network_xml_state_t *)userData;
    strncpy(state->current_element, name, sizeof(state->current_element) - 1);
    state->current_element[sizeof(state->current_element) - 1] = '\0';
    
    if (strcmp(name, "name") == 0) {
        state->in_name = 1;
    } else if (strcmp(name, "type") == 0) {
        state->in_type = 1;
    } else if (strcmp(name, "fib-id") == 0) {
        state->in_fib_id = 1;
    } else if (strcmp(name, "physical-interface") == 0) {
        state->in_physical_interface = 1;
    } else if (strcmp(name, "bridge-name") == 0) {
        state->in_bridge_name = 1;
    }
}

/**
 * XML end element handler for network configuration
 */
static void XMLCALL network_end_element_handler(void *userData, const XML_Char *name) {
    network_xml_state_t *state = (network_xml_state_t *)userData;
    
    if (strcmp(name, "name") == 0) {
        strncpy(state->network->name, state->current_text, sizeof(state->network->name) - 1);
        state->in_name = 0;
    } else if (strcmp(name, "type") == 0) {
        if (strcmp(state->current_text, "bridge") == 0) {
            state->network->type = NETWORK_TYPE_BRIDGE;
        }
        state->in_type = 0;
    } else if (strcmp(name, "fib-id") == 0) {
        state->network->fib_id = atoi(state->current_text);
        state->in_fib_id = 0;
    } else if (strcmp(name, "physical-interface") == 0) {
        strncpy(state->network->physical_interface, state->current_text, sizeof(state->network->physical_interface) - 1);
        state->in_physical_interface = 0;
    } else if (strcmp(name, "bridge-name") == 0) {
        strncpy(state->network->bridge_name, state->current_text, sizeof(state->network->bridge_name) - 1);
        state->in_bridge_name = 0;
    }
    
    memset(state->current_text, 0, sizeof(state->current_text));
}

/**
 * XML character data handler for network configuration
 */
static void XMLCALL network_character_data_handler(void *userData, const XML_Char *s, int len) {
    network_xml_state_t *state = (network_xml_state_t *)userData;
    strncat(state->current_text, s, len);
}

/**
 * Save network configuration to XML file
 * @param network Network configuration to save
 * @return 0 on success, -1 on failure
 */
int xml_save_network_config(const network_def_t *network) {
    char config_path[MAX_PATH_LEN];
    snprintf(config_path, sizeof(config_path), "%s/%s/config.xml", NETWORK_BASE_PATH, network->name);
    
    FILE *fp = fopen(config_path, "w");
    if (!fp) {
        HVD_ERROR("Failed to open network config file %s: %s", config_path, strerror(errno));
        return -1;
    }
    
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<network-config xmlns=\"urn:hvd:network\">\n");
    fprintf(fp, "  <name>%s</name>\n", network->name);
    fprintf(fp, "  <type>bridge</type>\n");
    fprintf(fp, "  <fib-id>%u</fib-id>\n", network->fib_id);
    fprintf(fp, "  <physical-interface>%s</physical-interface>\n", network->physical_interface);
    fprintf(fp, "  <bridge-name>%s</bridge-name>\n", network->bridge_name);
    fprintf(fp, "</network-config>\n");
    
    fclose(fp);
    HVD_INFO("Saved network configuration: %s", config_path);
    return 0;
}

/**
 * Load network configuration from XML file
 * @param network_name Network name
 * @param network Network configuration structure to populate
 * @return 0 on success, -1 on failure
 */
int xml_load_network_config(const char *network_name, network_def_t *network) {
    char config_path[MAX_PATH_LEN];
    snprintf(config_path, sizeof(config_path), "%s/%s/config.xml", NETWORK_BASE_PATH, network_name);
    
    FILE *fp = fopen(config_path, "r");
    if (!fp) {
        HVD_ERROR("Failed to open network config file %s: %s", config_path, strerror(errno));
        return -1;
    }
    
    // Read file content
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *xml_content = malloc(file_size + 1);
    if (!xml_content) {
        HVD_ERROR("Failed to allocate memory for XML content");
        fclose(fp);
        return -1;
    }
    
    size_t bytes_read = fread(xml_content, 1, file_size, fp);
    xml_content[bytes_read] = '\0';
    fclose(fp);
    
    // Parse XML
    XML_Parser parser = XML_ParserCreate(NULL);
    if (!parser) {
        HVD_ERROR("Failed to create XML parser");
        free(xml_content);
        return -1;
    }
    
    network_xml_state_t state = {0};
    state.network = network;
    
    XML_SetUserData(parser, &state);
    XML_SetElementHandler(parser, network_start_element_handler, network_end_element_handler);
    XML_SetCharacterDataHandler(parser, network_character_data_handler);
    
    if (XML_Parse(parser, xml_content, bytes_read, 1) != XML_STATUS_OK) {
        HVD_ERROR("Failed to parse network config XML: %s", XML_ErrorString(XML_GetErrorCode(parser)));
        XML_ParserFree(parser);
        free(xml_content);
        return -1;
    }
    
    XML_ParserFree(parser);
    free(xml_content);
    
    HVD_INFO("Loaded network configuration: %s", network_name);
    return 0;
} 