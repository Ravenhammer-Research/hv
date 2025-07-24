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
 * @file hv_client.c
 * @brief HVD CLI client
 * 
 * This file implements the CLI client for the hypervisor management daemon,
 * providing both interactive and one-shot command modes.
 */

#include "common.h"
#include <readline/readline.h>
#include <readline/history.h>

/**
 * Connect to the HVD server
 * @return Socket file descriptor on success, -1 on failure
 */
static int connect_to_server(void) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

/**
 * Send a command to the server and receive response
 * @param cmd Command string to send
 * @return 0 on success, -1 on failure
 */
static int send_command(const char *cmd) {
    int sock = connect_to_server();
    if (sock < 0) {
        return -1;
    }
    
    // Send command length
    size_t len = strlen(cmd);
    if (send(sock, &len, sizeof(len), 0) < 0) {
        perror("send length");
        close(sock);
        return -1;
    }
    
    // Send command
    if (send(sock, cmd, len, 0) < 0) {
        perror("send command");
        close(sock);
        return -1;
    }
    
    // Receive response length
    if (recv(sock, &len, sizeof(len), 0) < 0) {
        perror("recv length");
        close(sock);
        return -1;
    }
    
    if (len >= MAX_RESPONSE_LEN) {
        fprintf(stderr, "Response too large\n");
        close(sock);
        return -1;
    }
    
    // Receive response
    char response[MAX_RESPONSE_LEN];
    size_t received = 0;
    while (received < len) {
        ssize_t n = recv(sock, response + received, len - received, 0);
        if (n < 0) {
            perror("recv response");
            close(sock);
            return -1;
        }
        received += n;
    }
    
    response[len] = '\0';
    
    // Print response
    printf("%s", response);
    
    close(sock);
    return 0;
}

/**
 * Command generator for readline tab completion
 * @param text Text being completed
 * @param state Completion state (0 for first call, non-zero for subsequent)
 * @return Next possible completion or NULL when done
 */
char* command_generator(const char* text, int state) {
    static int list_index, len;
    static const char* commands[] = {
        "create", "destroy", "start", "stop", "set", "show", "list", "help",
        "vm", "network", "cpu", "memory", "boot-device", "fib", "physical-interface"
    };
    
    if (!state) {
        list_index = 0;
        len = strlen(text);
    }
    
    while (list_index < (int)(sizeof(commands)/sizeof(commands[0]))) {
        const char* name = commands[list_index++];
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }
    
    return NULL;
}

/**
 * Command completion function for readline
 * @param text Text being completed
 * @param start Start position in line (unused)
 * @param end End position in line (unused)
 * @return Array of possible completions
 */
char** command_completion(const char* text, int start __attribute__((unused)), int end __attribute__((unused))) {
    rl_attempted_completion_over = 1;
    return rl_completion_matches(text, command_generator);
}

/**
 * Interactive mode with readline support
 */
static void interactive_mode(void) {
    // Set up readline
    rl_readline_name = "hv";
    rl_attempted_completion_function = command_completion;
    
    char *line;
    while ((line = readline("hv> ")) != NULL) {
        if (strlen(line) == 0) {
            free(line);
            continue;
        }
        
        if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            free(line);
            break;
        }
        
        // Add to history
        add_history(line);
        
        // Send command to server
        send_command(line);
        
        free(line);
    }
}

/**
 * One-shot mode for single command execution
 * @param argc Number of command line arguments
 * @param argv Array of command line arguments
 */
static void one_shot_mode(int argc, char *argv[]) {
    // Build command from arguments
    char cmd[MAX_CMD_LEN] = "";
    for (int i = 1; i < argc; i++) {
        if (i > 1) strcat(cmd, " ");
        strcat(cmd, argv[i]);
    }
    
    if (send_command(cmd) != 0) {
        exit(1);
    }
}

/**
 * Print usage information
 * @param progname Program name
 */
static void print_usage(const char *progname) {
    printf("Usage: %s [command] [args...]\n", progname);
    printf("\n");
    printf("Interactive mode:\n");
    printf("  %s                    Start interactive CLI\n", progname);
    printf("\n");
    printf("One-shot mode:\n");
    printf("  %s <command> [args]   Execute single command\n", progname);
    printf("\n");
    printf("Examples:\n");
    printf("  %s create vm test 2 1024\n", progname);
    printf("  %s list vm\n", progname);
    printf("  %s set vm test cpu 4\n", progname);
    printf("  %s start test\n", progname);
    printf("\n");
    printf("Use 'help' command for detailed command reference\n");
}

/**
 * Main entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status
 */
int main(int argc, char *argv[]) {
    if (argc == 1) {
        // Interactive mode
        interactive_mode();
    } else if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        // Help mode
        print_usage(argv[0]);
    } else {
        // One-shot mode
        one_shot_mode(argc, argv);
    }
    
    return 0;
} 