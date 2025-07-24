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
 * @brief Integration with netd for network configuration
 * 
 * This file implements integration with the netd server for configuring
 * bridges and tap interfaces for VM networking.
 */

#include "common.h"

#define NETD_SOCKET_PATH "/var/run/netd.sock"

/**
 * Send command to netd server
 * @param cmd Command to send
 * @param response Buffer to store response
 * @param resp_len Size of response buffer
 * @return 0 on success, -1 on failure
 */
static int netd_send_command(const char *cmd, char *response, size_t resp_len) {
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

    // Send command length
    size_t len = strlen(cmd);
    if (send(sock, &len, sizeof(len), 0) < 0) {
        HVD_ERROR("Failed to send command length to netd: %s", strerror(errno));
        close(sock);
        return -1;
    }
    
    // Send command
    if (send(sock, cmd, len, 0) < 0) {
        HVD_ERROR("Failed to send command to netd: %s", strerror(errno));
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
 * Configure a bridge interface via netd
 * @param bridge_name Bridge interface name
 * @param fib_id FIB ID for the bridge
 * @return 0 on success, -1 on failure
 */
int netd_configure_bridge(const char *bridge_name, uint32_t fib_id) {
    char cmd[512];
    char response[MAX_RESPONSE_LEN];
    
    // Create bridge interface
    snprintf(cmd, sizeof(cmd), "set interface %s type bridge", bridge_name);
    if (netd_send_command(cmd, response, sizeof(response)) != 0) {
        return -1;
    }
    
    // Set FIB ID if specified
    if (fib_id > 0) {
        snprintf(cmd, sizeof(cmd), "set interface %s fib %u", bridge_name, fib_id);
        if (netd_send_command(cmd, response, sizeof(response)) != 0) {
            return -1;
        }
    }
    
    // Enable the bridge
    snprintf(cmd, sizeof(cmd), "set interface %s enabled", bridge_name);
    if (netd_send_command(cmd, response, sizeof(response)) != 0) {
        return -1;
    }
    
    HVD_INFO("Configured bridge interface: %s (FIB: %u)", bridge_name, fib_id);
    return 0;
}

/**
 * Configure a tap interface via netd
 * @param tap_name Tap interface name
 * @param bridge_name Bridge to attach to
 * @param fib_id FIB ID for the tap interface
 * @return 0 on success, -1 on failure
 */
int netd_configure_tap(const char *tap_name, const char *bridge_name, uint32_t fib_id) {
    char cmd[512];
    char response[MAX_RESPONSE_LEN];
    
    // Create tap interface
    snprintf(cmd, sizeof(cmd), "set interface %s type tap", tap_name);
    if (netd_send_command(cmd, response, sizeof(response)) != 0) {
        return -1;
    }
    
    // Set FIB ID if specified
    if (fib_id > 0) {
        snprintf(cmd, sizeof(cmd), "set interface %s fib %u", tap_name, fib_id);
        if (netd_send_command(cmd, response, sizeof(response)) != 0) {
            return -1;
        }
    }
    
    // Add tap to bridge
    snprintf(cmd, sizeof(cmd), "set interface %s bridge %s", tap_name, bridge_name);
    if (netd_send_command(cmd, response, sizeof(response)) != 0) {
        return -1;
    }
    
    // Enable the tap interface
    snprintf(cmd, sizeof(cmd), "set interface %s enabled", tap_name);
    if (netd_send_command(cmd, response, sizeof(response)) != 0) {
        return -1;
    }
    
    HVD_INFO("Configured tap interface: %s (bridge: %s, FIB: %u)", tap_name, bridge_name, fib_id);
    return 0;
}

/**
 * Remove a tap interface via netd
 * @param tap_name Tap interface name
 * @return 0 on success, -1 on failure
 */
int netd_remove_tap(const char *tap_name) {
    char cmd[512];
    char response[MAX_RESPONSE_LEN];
    
    // Disable the tap interface
    snprintf(cmd, sizeof(cmd), "set interface %s disabled", tap_name);
    netd_send_command(cmd, response, sizeof(response));
    
    // Remove the tap interface
    snprintf(cmd, sizeof(cmd), "delete interface %s", tap_name);
    if (netd_send_command(cmd, response, sizeof(response)) != 0) {
        return -1;
    }
    
    HVD_INFO("Removed tap interface: %s", tap_name);
    return 0;
}

/**
 * Remove a bridge interface via netd
 * @param bridge_name Bridge interface name
 * @return 0 on success, -1 on failure
 */
int netd_remove_bridge(const char *bridge_name) {
    char cmd[512];
    char response[MAX_RESPONSE_LEN];
    
    // Disable the bridge interface
    snprintf(cmd, sizeof(cmd), "set interface %s disabled", bridge_name);
    netd_send_command(cmd, response, sizeof(response));
    
    // Remove the bridge interface
    snprintf(cmd, sizeof(cmd), "delete interface %s", bridge_name);
    if (netd_send_command(cmd, response, sizeof(response)) != 0) {
        return -1;
    }
    
    HVD_INFO("Removed bridge interface: %s", bridge_name);
    return 0;
}

/**
 * Check if netd is available
 * @return 0 if available, -1 if not
 */
int netd_check_availability(void) {
    if (access(NETD_SOCKET_PATH, F_OK) != 0) {
        HVD_ERROR("netd socket not found at %s", NETD_SOCKET_PATH);
        return -1;
    }
    
    char response[MAX_RESPONSE_LEN];
    if (netd_send_command("show version", response, sizeof(response)) != 0) {
        HVD_ERROR("Failed to communicate with netd");
        return -1;
    }
    
    HVD_INFO("netd integration available");
    return 0;
} 