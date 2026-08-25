/* $egnet: storage.c,v 1.14 2007/09/23 16:27:22 dive Exp $ */

/*
 * Copyright (c) 2002-2026
 *               Sean Davis <dive@endersgame.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
#include <stdio.h>
#include <unistd.h>
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <unistd.h>
#elif defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#elif defined(                                                                 \
    __OpenBSD__) /* Theo's never gonna thank me for this but whatever */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <unistd.h>
#else
/* No per-platform RSS implementation; get_server_rss() will return 0. */
#endif

#include "config.h"
#include "exceptions.h"
#include "list.h"
#include "options.h"
#include "ref_count.h"
#include "storage.h"
#include "structures.h"
#include "utils.h"

static unsigned alloc_num[Sizeof_Memory_Type];

/*
 * Generic allocations carry no interpreter-specific header.  In particular,
 * Memory_Type is accounting/debugging information only; it does not change
 * the layout of the allocation.
 */
void *mymalloc(size_t size, Memory_Type type) {
    char msg[128];

    if (size == 0)
        size = 1;

    void *ptr = malloc(size);
    if (!ptr) {
        snprintf(msg, sizeof(msg), "memory allocation (size %zu) failed!", size);
        server_panic(msg);
    }

    alloc_num[type]++;
    return ptr;
}

void *myrealloc(void *ptr, size_t size, Memory_Type type) {
    char msg[128];

    if (size == 0)
        size = 1;

    void *newptr = realloc(ptr, size);
    if (!newptr) {
        snprintf(msg, sizeof(msg),
                 "memory re-allocation (size %zu) failed!", size);
        server_panic(msg);
    }

    /* A realloc does not change the number of live allocations. */
    (void)type;
    return newptr;
}

void myfree(void *ptr, Memory_Type type) {
    if (ptr == NULL)
        return;

    alloc_num[type]--;
    free(ptr);
}

char *str_alloc(size_t size) {
    return (char *)rc_alloc(size);
}

const char *str_ref(const char *s) {
    rc_retain(s);
    return s;
}

char *str_dup(const char *s) {
    if (s == NULL || *s == '\0') {
        static char *emptystring;

        if (!emptystring) {
            emptystring = (char *)rc_alloc(1);
            *emptystring = '\0';
        }
        rc_retain(emptystring);
        return emptystring;
    }

    size_t len = strlen(s) + 1;
    char *r = (char *)rc_alloc(len);
    memcpy(r, s, len);
    return r;
}

#if defined(__linux__)
static size_t get_server_rss(void) {
    long size_pages = 0, rss_pages = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f)
        return 0;
    if (fscanf(f, "%ld %ld", &size_pages, &rss_pages) != 2)
        rss_pages = 0;
    long page_size = sysconf(_SC_PAGESIZE);
    fclose(f);
    if (page_size <= 0)
        return 0;
    return (size_t)rss_pages * (size_t)page_size;
}

#elif defined(__FreeBSD__)
static size_t get_server_rss(void) {
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};

    if (sysctl(mib, 4, &kp, &len, NULL, 0) != 0)
        return 0;
    return (size_t)kp.ki_rssize * (size_t)getpagesize();
}

#elif defined(__NetBSD__)
static size_t get_server_rss(void) {
    struct kinfo_proc2 kp;
    size_t len = sizeof(kp);
    int mib[6] = {CTL_KERN, KERN_PROC2, KERN_PROC_PID, getpid(), sizeof(kp), 1};

    if (sysctl(mib, 6, &kp, &len, NULL, 0) != 0)
        return 0;
    return (size_t)kp.p_vm_rssize * (size_t)getpagesize();
}

#elif defined(__APPLE__)
static size_t get_server_rss(void) {
    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info,
                  &count) != KERN_SUCCESS) {
        return 0;
    }
    return (size_t)info.resident_size;
}

#elif defined(__OpenBSD__)
static size_t get_server_rss(void) {
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[6] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid(), sizeof(kp), 1};

    if (sysctl(mib, 6, &kp, &len, NULL, 0) != 0)
        return 0;
    return (size_t)kp.p_vm_rssize * (size_t)getpagesize();
}

#else

#error "get_server_rss: unsupported platform"

#endif

Var memory_usage(void) {
    Var r;
    size_t rss = get_server_rss();
    r.type = TYPE_FLOAT;
    r.v.fnum = rc_alloc(sizeof(double));
    *r.v.fnum = (double)rss;
    return r;
}
