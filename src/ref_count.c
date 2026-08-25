/* $egnet: ref_count.c,v 1.0 2026/08/25 00:00:00 dive Exp $ */

/*
 * Copyright (c) 2002-2026
 *               Sean Davis <dive@endersgame.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * I did not write this, ChatGPT did :)
 * -dive @ 20260825
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "exceptions.h"
#include "ref_count.h"

/*
 * The header precedes the object returned to callers.  The union gives the
 * header the alignment required by the objects currently using this API:
 * strings, Var pointers, and doubles, while keeping the header compact on
 * the platforms supported by the server.
 */
struct rc_header {
    union {
        uint32_t refcount;
        double double_alignment;
        void *pointer_alignment;
    } data;
};

static inline struct rc_header *rc_header_from_ptr(const void *ptr) {
    return (struct rc_header *)ptr - 1;
}

void *rc_alloc(size_t size) {
    if (size == 0)
        size = 1;

    struct rc_header *header = malloc(sizeof(*header) + size);
    if (!header) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "reference-counted allocation (size %zu) failed!", size);
        server_panic(msg);
    }

    header->data.refcount = 1;
    return (void *)(header + 1);
}

void *rc_realloc(void *ptr, size_t size) {
    if (ptr == NULL)
        return rc_alloc(size);
    if (size == 0)
        size = 1;

    struct rc_header *header = rc_header_from_ptr(ptr);
    if (header->data.refcount != 1)
        server_panic("attempt to reallocate a shared reference-counted object");

    header = realloc(header, sizeof(*header) + size);
    if (!header) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "reference-counted re-allocation (size %zu) failed!", size);
        server_panic(msg);
    }

    return (void *)(header + 1);
}

void rc_retain(const void *ptr) {
    if (ptr == NULL)
        return;

    struct rc_header *header = rc_header_from_ptr(ptr);
    if (header->data.refcount == UINT32_MAX)
        server_panic("reference count overflow");
    header->data.refcount++;
}

bool rc_release(const void *ptr) {
    if (ptr == NULL)
        return false;

    struct rc_header *header = rc_header_from_ptr(ptr);
    if (header->data.refcount == 0)
        server_panic("reference count underflow");

    return --header->data.refcount == 0;
}

void rc_free(void *ptr) {
    if (ptr == NULL)
        return;
    free(rc_header_from_ptr(ptr));
}

uint32_t rc_count(const void *ptr) {
    if (ptr == NULL)
        return 0;
    return rc_header_from_ptr(ptr)->data.refcount;
}
