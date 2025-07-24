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
 * @file netd_integration.c
 * @brief YANG-aware integration with netd for network configuration
 * 
 * This file implements integration with the netd server using YANG schemas
 * for configuring bridges and tap interfaces for VM networking.
 */

#include "common.h"
#include "yang_netd.h"

#define NETD_SOCKET_PATH "/var/run/netd.sock"

/**
 * Generate YANG XML for interface configuration
 * @param interface Interface configuration
 * @param xml_buffer Buffer to store XML
 * @param buffer_size Size of buffer
 * @return 0 on success, -1 on failure
 */
int yang_generate_interface_xml(const yang_interface_t *interface, char *xml_buffer, size_t buffer_size) {
    size_t offset = 0;
    
    offset += snprintf(xml_buffer + offset, buffer_size - offset,
        "<interface xmlns=\"%s\">\n"
        "  <name>%s</name>\n"
        "  <enabled>%s</enabled>\n"
        "  <fib>%u</fib>\n",
        NETD_YANG_NAMESPACE, interface->name, 
        interface->enabled ? "true" : "false", interface->fib);
    
    for (int i = 0; i < interface->address_count; i++) {
        offset += snprintf(xml_buffer + offset, buffer_size - offset,
            "  <address>\n"
            "    <ip>%s</ip>\n"
            "    <family>%s</family>\n"
            "  </address>\n",
            interface->addresses[i], interface->families[i]);
    }
    
    offset += snprintf(xml_buffer + offset, buffer_size - offset, "</interface>\n");
    
    return (offset < buffer_size) ? 0 : -1;
}

/**
 * Generate YANG XML for route configuration
 * @param route Route configuration
 * @param xml_buffer Buffer to store XML
 * @param buffer_size Size of buffer
 * @return 0 on success, -1 on failure
 */
int yang_generate_route_xml(const yang_route_t *route, char *xml_buffer, size_t buffer_size) {
    size_t offset = snprintf(xml_buffer, buffer_size,
        "<route xmlns=\"%s\">\n"
        "  <destination>%s</destination>\n"
        "  <gateway>%s</gateway>\n"
        "  <fib>%u</fib>\n"
        "  <description>%s</description>\n"
        "</route>\n",
        NETD_YANG_NAMESPACE, route->destination, route->gateway, 
        route->fib, route->description);
    
    return (offset < buffer_size) ? 0 : -1;
}

/**
 * Generate complete YANG configuration XML
 * @param config Configuration structure
 * @param xml_buffer Buffer to store XML
 * @param buffer_size Size of buffer
 * @return 0 on success, -1 on failure
 */
int yang_generate_config_xml(const yang_netd_config_t *config, char *xml_buffer, size_t buffer_size) {
    size_t offset = snprintf(xml_buffer, buffer_size,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<netd-config xmlns=\"%s\">\n",
        NETD_YANG_NAMESPACE);
    
    if (offset >= buffer_size) return -1;
    
    for (int i = 0; i < config->interface_count; i++) {
        char interface_xml[1024];
        if (yang_generate_interface_xml(&config->interfaces[i], interface_xml, sizeof(interface_xml)) != 0) {
            return -1;
        }
        
        offset += snprintf(xml_buffer + offset, buffer_size - offset, "%s", interface_xml);
        if (offset >= buffer_size) return -1;
    }
    
    for (int i = 0; i < config->route_count; i++) {
        char route_xml[512];
        if (yang_generate_route_xml(&config->routes[i], route_xml, sizeof(route_xml)) != 0) {
            return -1;
        }
        
        offset += snprintf(xml_buffer + offset, buffer_size - offset, "%s", route_xml);
        if (offset >= buffer_size) return -1;
    }
    
    offset += snprintf(xml_buffer + offset, buffer_size - offset, "</netd-config>\n");
    
    return (offset < buffer_size) ? 0 : -1;
}

/**
 * Send YANG configuration to netd server
 * @param xml_config YANG XML configuration
 * @param response Buffer to store response
 * @param resp_len Size of response buffer
 * @return 0 on success, -1 on failure
 */
int netd_send_yang_config(const char *xml_config, char *response, size_t resp_len) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        HVD_ERROR("Failed to create socket for netd communication: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, NETD_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        HVD_ERROR("Failed to connect to netd: %s", strerror(errno));
        close(sock);
        return -1;
    }

    // Send YANG configuration length
    size_t len = strlen(xml_config);
    if (send(sock, &len, sizeof(len), 0) < 0) {
        HVD_ERROR("Failed to send YANG config length to netd: %s", strerror(errno));
        close(sock);
        return -1;
    }
    
    // Send YANG configuration
    if (send(sock, xml_config, len, 0) < 0) {
        HVD_ERROR("Failed to send YANG config to netd: %s", strerror(errno));
        close(sock);
        return -1;
    }
    
    // Receive response length
    if (recv(sock, &len, sizeof(len), 0) < 0) {
        HVD_ERROR("Failed to receive response length from netd: %s", strerror(errno));
        close(sock);
        return -1;
    }
    
    if (len >= resp_len) {
        HVD_ERROR("Response from netd too large");
        close(sock);
        return -1;
    }
    
    // Receive response
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(sock, response + received, len - received, 0);
        if (n < 0) {
            HVD_ERROR("Failed to receive response from netd: %s", strerror(errno));
            close(sock);
            return -1;
        }
        received += n;
    }
    
    response[len] = '\0';
    close(sock);
    return 0;
}

/**
 * Configure a bridge interface via YANG
 * @param bridge_name Bridge interface name
 * @param fib_id FIB ID for the bridge
 * @return 0 on success, -1 on failure
 */
int netd_configure_bridge(const char *bridge_name, uint32_t fib_id) {
    yang_netd_config_t config = {0};
    char xml_buffer[8192];
    char response[MAX_RESPONSE_LEN];
    
    // Configure bridge interface
    yang_interface_t *bridge = &config.interfaces[config.interface_count++];
    strncpy(bridge->name, bridge_name, sizeof(bridge->name) - 1);
    bridge->enabled = 1;
    bridge->fib = fib_id;
    
    // Generate YANG XML
    if (yang_generate_config_xml(&config, xml_buffer, sizeof(xml_buffer)) != 0) {
        HVD_ERROR("Failed to generate YANG XML for bridge configuration");
        return -1;
    }
    
    // Send to netd
    if (netd_send_yang_config(xml_buffer, response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to send bridge configuration to netd");
        return -1;
    }
    
    HVD_INFO("Configured bridge interface via YANG: %s (FIB: %u)", bridge_name, fib_id);
    return 0;
}

/**
 * Configure a tap interface via YANG
 * @param tap_name Tap interface name
 * @param bridge_name Bridge to attach to
 * @param fib_id FIB ID for the tap interface
 * @return 0 on success, -1 on failure
 */
int netd_configure_tap(const char *tap_name, const char *bridge_name, uint32_t fib_id) {
    yang_netd_config_t config = {0};
    char xml_buffer[8192];
    char response[MAX_RESPONSE_LEN];
    
    // Configure tap interface
    yang_interface_t *tap = &config.interfaces[config.interface_count++];
    strncpy(tap->name, tap_name, sizeof(tap->name) - 1);
    tap->enabled = 1;
    tap->fib = fib_id;
    
    // Generate YANG XML
    if (yang_generate_config_xml(&config, xml_buffer, sizeof(xml_buffer)) != 0) {
        HVD_ERROR("Failed to generate YANG XML for tap configuration");
        return -1;
    }
    
    // Send to netd
    if (netd_send_yang_config(xml_buffer, response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to send tap configuration to netd");
        return -1;
    }
    
    HVD_INFO("Configured tap interface via YANG: %s (bridge: %s, FIB: %u)", 
             tap_name, bridge_name, fib_id);
    return 0;
}

/**
 * Add IP address to interface via YANG
 * @param interface_name Interface name
 * @param ip_address IP address with prefix
 * @param family Address family (ipv4/ipv6)
 * @return 0 on success, -1 on failure
 */
int netd_add_interface_address(const char *interface_name, const char *ip_address, const char *family) {
    yang_netd_config_t config = {0};
    char xml_buffer[8192];
    char response[MAX_RESPONSE_LEN];
    
    // Configure interface with IP address
    yang_interface_t *interface = &config.interfaces[config.interface_count++];
    strncpy(interface->name, interface_name, sizeof(interface->name) - 1);
    interface->enabled = 1;
    interface->fib = 0;
    
    // Add IP address
    strncpy(interface->addresses[interface->address_count], ip_address, sizeof(interface->addresses[0]) - 1);
    strncpy(interface->families[interface->address_count], family, sizeof(interface->families[0]) - 1);
    interface->address_count++;
    
    // Generate YANG XML
    if (yang_generate_config_xml(&config, xml_buffer, sizeof(xml_buffer)) != 0) {
        HVD_ERROR("Failed to generate YANG XML for IP address configuration");
        return -1;
    }
    
    // Send to netd
    if (netd_send_yang_config(xml_buffer, response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to send IP address configuration to netd");
        return -1;
    }
    
    HVD_INFO("Added IP address via YANG: %s %s to %s", family, ip_address, interface_name);
    return 0;
}

/**
 * Add static route via YANG
 * @param destination Destination network
 * @param gateway Gateway address
 * @param fib_id FIB ID for the route
 * @param description Route description
 * @return 0 on success, -1 on failure
 */
int netd_add_static_route(const char *destination, const char *gateway, uint32_t fib_id, const char *description) {
    yang_netd_config_t config = {0};
    char xml_buffer[8192];
    char response[MAX_RESPONSE_LEN];
    
    // Configure static route
    yang_route_t *route = &config.routes[config.route_count++];
    strncpy(route->destination, destination, sizeof(route->destination) - 1);
    strncpy(route->gateway, gateway, sizeof(route->gateway) - 1);
    route->fib = fib_id;
    if (description) {
        strncpy(route->description, description, sizeof(route->description) - 1);
    }
    
    // Generate YANG XML
    if (yang_generate_config_xml(&config, xml_buffer, sizeof(xml_buffer)) != 0) {
        HVD_ERROR("Failed to generate YANG XML for route configuration");
        return -1;
    }
    
    // Send to netd
    if (netd_send_yang_config(xml_buffer, response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to send route configuration to netd");
        return -1;
    }
    
    HVD_INFO("Added static route via YANG: %s via %s (FIB: %u)", destination, gateway, fib_id);
    return 0;
}

/**
 * Remove a tap interface via YANG
 * @param tap_name Tap interface name
 * @return 0 on success, -1 on failure
 */
int netd_remove_tap(const char *tap_name) {
    yang_netd_config_t config = {0};
    char xml_buffer[8192];
    char response[MAX_RESPONSE_LEN];
    
    // Configure tap interface as disabled
    yang_interface_t *tap = &config.interfaces[config.interface_count++];
    strncpy(tap->name, tap_name, sizeof(tap->name) - 1);
    tap->enabled = 0;  // Disable the interface
    
    // Generate YANG XML
    if (yang_generate_config_xml(&config, xml_buffer, sizeof(xml_buffer)) != 0) {
        HVD_ERROR("Failed to generate YANG XML for tap removal");
        return -1;
    }
    
    // Send to netd
    if (netd_send_yang_config(xml_buffer, response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to send tap removal to netd");
        return -1;
    }
    
    HVD_INFO("Removed tap interface via YANG: %s", tap_name);
    return 0;
}

/**
 * Remove a bridge interface via YANG
 * @param bridge_name Bridge interface name
 * @return 0 on success, -1 on failure
 */
int netd_remove_bridge(const char *bridge_name) {
    yang_netd_config_t config = {0};
    char xml_buffer[8192];
    char response[MAX_RESPONSE_LEN];
    
    // Configure bridge interface as disabled
    yang_interface_t *bridge = &config.interfaces[config.interface_count++];
    strncpy(bridge->name, bridge_name, sizeof(bridge->name) - 1);
    bridge->enabled = 0;  // Disable the interface
    
    // Generate YANG XML
    if (yang_generate_config_xml(&config, xml_buffer, sizeof(xml_buffer)) != 0) {
        HVD_ERROR("Failed to generate YANG XML for bridge removal");
        return -1;
    }
    
    // Send to netd
    if (netd_send_yang_config(xml_buffer, response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to send bridge removal to netd");
        return -1;
    }
    
    HVD_INFO("Removed bridge interface via YANG: %s", bridge_name);
    return 0;
}

/**
 * Check if netd is available and supports YANG
 * @return 0 if available, -1 if not
 */
int netd_check_availability(void) {
    if (access(NETD_SOCKET_PATH, F_OK) != 0) {
        HVD_ERROR("netd socket not found at %s", NETD_SOCKET_PATH);
        return -1;
    }
    
    // Test YANG configuration with minimal config
    yang_netd_config_t test_config = {0};
    char xml_buffer[1024];
    char response[MAX_RESPONSE_LEN];
    
    if (yang_generate_config_xml(&test_config, xml_buffer, sizeof(xml_buffer)) != 0) {
        HVD_ERROR("Failed to generate test YANG configuration");
        return -1;
    }
    
    if (netd_send_yang_config(xml_buffer, response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to communicate with netd using YANG");
        return -1;
    }
    
    HVD_INFO("netd YANG integration available");
    return 0;
}

/* Legacy function for backward compatibility */
int netd_send_command(const char *cmd, char *response, size_t resp_len) {
    (void)cmd;      /* Suppress unused parameter warning */
    (void)response; /* Suppress unused parameter warning */
    (void)resp_len; /* Suppress unused parameter warning */
    HVD_ERROR("netd_send_command() is deprecated, use YANG-based functions instead");
    return -1;
}

/**
 * Add interface to YANG configuration
 * @param config Configuration structure
 * @param name Interface name
 * @param enabled Whether interface is enabled
 * @param fib FIB ID
 * @return Interface index on success, -1 on failure
 */
int yang_add_interface(yang_netd_config_t *config, const char *name, int enabled, uint32_t fib) {
    if (config->interface_count >= MAX_INTERFACES) {
        HVD_ERROR("Too many interfaces in configuration");
        return -1;
    }
    
    yang_interface_t *interface = &config->interfaces[config->interface_count];
    strncpy(interface->name, name, sizeof(interface->name) - 1);
    interface->enabled = enabled;
    interface->fib = fib;
    interface->address_count = 0;
    
    return config->interface_count++;
}

/**
 * Add IP address to interface in YANG configuration
 * @param config Configuration structure
 * @param interface_index Index of interface
 * @param ip_address IP address with prefix
 * @param family Address family (ipv4/ipv6)
 * @return 0 on success, -1 on failure
 */
int yang_add_interface_address(yang_netd_config_t *config, int interface_index, const char *ip_address, const char *family) {
    if (interface_index < 0 || interface_index >= config->interface_count) {
        HVD_ERROR("Invalid interface index: %d", interface_index);
        return -1;
    }
    
    yang_interface_t *interface = &config->interfaces[interface_index];
    if (interface->address_count >= MAX_ADDRESSES_PER_INTERFACE) {
        HVD_ERROR("Too many addresses for interface %s", interface->name);
        return -1;
    }
    
    strncpy(interface->addresses[interface->address_count], ip_address, sizeof(interface->addresses[0]) - 1);
    strncpy(interface->families[interface->address_count], family, sizeof(interface->families[0]) - 1);
    interface->address_count++;
    
    return 0;
}

/**
 * Add route to YANG configuration
 * @param config Configuration structure
 * @param destination Destination network
 * @param gateway Gateway address
 * @param fib FIB ID
 * @param description Route description
 * @return 0 on success, -1 on failure
 */
int yang_add_route(yang_netd_config_t *config, const char *destination, const char *gateway, uint32_t fib, const char *description) {
    if (config->route_count >= MAX_ROUTES) {
        HVD_ERROR("Too many routes in configuration");
        return -1;
    }
    
    yang_route_t *route = &config->routes[config->route_count];
    strncpy(route->destination, destination, sizeof(route->destination) - 1);
    strncpy(route->gateway, gateway, sizeof(route->gateway) - 1);
    route->fib = fib;
    if (description) {
        strncpy(route->description, description, sizeof(route->description) - 1);
    } else {
        route->description[0] = '\0';
    }
    
    config->route_count++;
    return 0;
}

/**
 * Validate IPv4 address format
 * @param address IPv4 address string
 * @return 0 if valid, -1 if invalid
 */
int yang_validate_ipv4_address(const char *address) {
    if (!address) return -1;
    
    int octets[4];
    int count = sscanf(address, "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]);
    
    if (count != 4) return -1;
    
    for (int i = 0; i < 4; i++) {
        if (octets[i] < 0 || octets[i] > 255) return -1;
    }
    
    return 0;
}

/**
 * Validate IPv6 address format (basic check)
 * @param address IPv6 address string
 * @return 0 if valid, -1 if invalid
 */
int yang_validate_ipv6_address(const char *address) {
    if (!address) return -1;
    
    // Basic IPv6 validation - check for colons
    int colon_count = 0;
    for (const char *p = address; *p; p++) {
        if (*p == ':') colon_count++;
    }
    
    // IPv6 addresses should have at least 2 colons
    return (colon_count >= 2) ? 0 : -1;
}

/**
 * Validate IP prefix format (CIDR notation)
 * @param prefix IP prefix string (e.g., "192.168.1.0/24")
 * @return 0 if valid, -1 if invalid
 */
int yang_validate_ip_prefix(const char *prefix) {
    if (!prefix) return -1;
    
    char *slash = strchr(prefix, '/');
    if (!slash) return -1;
    
    // Split into address and prefix length
    char address[64];
    int prefix_len;
    
    size_t addr_len = slash - prefix;
    if (addr_len >= sizeof(address)) return -1;
    
    strncpy(address, prefix, addr_len);
    address[addr_len] = '\0';
    
    if (sscanf(slash + 1, "%d", &prefix_len) != 1) return -1;
    
    // Validate prefix length
    if (prefix_len < 0 || prefix_len > 128) return -1;
    
    // Validate address part
    if (strchr(address, ':')) {
        return yang_validate_ipv6_address(address);
    } else {
        return yang_validate_ipv4_address(address);
    }
} 