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
 * @file hvd.c
 * @brief HVD server daemon
 * 
 * This file implements the main hypervisor management daemon that handles
 * client connections and command execution.
 */

#include "common.h"

static int server_sock = -1;
static volatile int running = 1;

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int sig) {
    (void)sig;
    running = 0;
    HVD_INFO("Received shutdown signal");
}

/**
 * Handle a client connection
 * @param client_sock Client socket file descriptor
 */
static void handle_client(int client_sock) {
    char cmd[MAX_CMD_LEN];
    char response[MAX_RESPONSE_LEN];
    
    while (running) {
        // Receive command length
        size_t len;
        ssize_t n = recv(client_sock, &len, sizeof(len), 0);
        if (n <= 0) {
            break;
        }
        
        if (len >= sizeof(cmd)) {
            HVD_ERROR("Command too large");
            break;
        }
        
        // Receive command
        size_t received = 0;
        while (received < len) {
            n = recv(client_sock, cmd + received, len - received, 0);
            if (n <= 0) {
                goto cleanup;
            }
            received += n;
        }
        cmd[len] = '\0';
        
        // Execute command
        if (execute_command(cmd, response, sizeof(response)) != 0) {
            snprintf(response, sizeof(response), "ERROR: Command execution failed\n");
        }
        
        // Send response length
        len = strlen(response);
        if (send(client_sock, &len, sizeof(len), 0) < 0) {
            break;
        }
        
        // Send response
        if (send(client_sock, response, len, 0) < 0) {
            break;
        }
    }
    
cleanup:
    close(client_sock);
}

/**
 * Initialize the server
 * @return 0 on success, -1 on failure
 */
static int init_server(void) {
    // Initialize ZFS structure
    if (zfs_init_hvd_structure() != 0) {
        HVD_ERROR("Failed to initialize ZFS structure");
        return -1;
    }
    
    // Check netd availability
    if (netd_check_availability() != 0) {
        HVD_ERROR("netd integration not available");
        // Continue anyway, as it's not critical for basic operation
    }
    
    // Create socket
    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_sock < 0) {
        HVD_ERROR("Failed to create socket: %s", strerror(errno));
        return -1;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        HVD_ERROR("Failed to set socket options: %s", strerror(errno));
        close(server_sock);
        return -1;
    }
    
    // Bind socket
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    // Remove existing socket file
    unlink(SOCKET_PATH);
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        HVD_ERROR("Failed to bind socket: %s", strerror(errno));
        close(server_sock);
        return -1;
    }
    
    // Listen for connections
    if (listen(server_sock, 5) < 0) {
        HVD_ERROR("Failed to listen on socket: %s", strerror(errno));
        close(server_sock);
        return -1;
    }
    
    // Set socket permissions
    chmod(SOCKET_PATH, 0666);
    
    HVD_INFO("Server initialized successfully");
    return 0;
}

/**
 * Main server loop
 */
static void server_loop(void) {
    HVD_INFO("Server started, listening on %s", SOCKET_PATH);
    
    while (running) {
        struct sockaddr_un client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            if (running) {
                HVD_ERROR("Failed to accept connection: %s", strerror(errno));
            }
            continue;
        }
        
        HVD_INFO("Client connected");
        
        // Handle client in current process (simple implementation)
        // In a production system, you'd want to fork or use threads
        handle_client(client_sock);
    }
}

/**
 * Cleanup server resources
 */
static void cleanup_server(void) {
    if (server_sock >= 0) {
        close(server_sock);
        server_sock = -1;
    }
    
    unlink(SOCKET_PATH);
    HVD_INFO("Server cleanup completed");
}

/**
 * Main entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize server
    if (init_server() != 0) {
        HVD_ERROR("Failed to initialize server");
        return 1;
    }
    
    // Run server loop
    server_loop();
    
    // Cleanup
    cleanup_server();
    
    HVD_INFO("Server shutdown complete");
    return 0;
} 