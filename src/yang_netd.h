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

#ifndef YANG_NETD_H
#define YANG_NETD_H

#include "common.h"

#define NETD_YANG_NAMESPACE "urn:netd:simple"
#define MAX_INTERFACES 50
#define MAX_ROUTES 100
#define MAX_ADDRESSES_PER_INTERFACE 10

/* YANG configuration structures */
typedef struct {
    char name[MAX_NAME_LEN];
    int enabled;
    uint32_t fib;
    char addresses[MAX_ADDRESSES_PER_INTERFACE][64];  /* IP addresses with prefix */
    char families[MAX_ADDRESSES_PER_INTERFACE][8];    /* ipv4/ipv6 */
    int address_count;
} yang_interface_t;

typedef struct {
    char destination[64];
    char gateway[64];
    uint32_t fib;
    char description[MAX_NAME_LEN];
} yang_route_t;

typedef struct {
    yang_interface_t interfaces[MAX_INTERFACES];
    yang_route_t routes[MAX_ROUTES];
    int interface_count;
    int route_count;
} yang_netd_config_t;

/* YANG XML generation functions */
int yang_generate_interface_xml(const yang_interface_t *interface, char *xml_buffer, size_t buffer_size);
int yang_generate_route_xml(const yang_route_t *route, char *xml_buffer, size_t buffer_size);
int yang_generate_config_xml(const yang_netd_config_t *config, char *xml_buffer, size_t buffer_size);

/* YANG configuration helper functions */
int yang_add_interface(yang_netd_config_t *config, const char *name, int enabled, uint32_t fib);
int yang_add_interface_address(yang_netd_config_t *config, int interface_index, const char *ip_address, const char *family);
int yang_add_route(yang_netd_config_t *config, const char *destination, const char *gateway, uint32_t fib, const char *description);

/* YANG validation functions */
int yang_validate_ipv4_address(const char *address);
int yang_validate_ipv6_address(const char *address);
int yang_validate_ip_prefix(const char *prefix);

#endif /* YANG_NETD_H */ 