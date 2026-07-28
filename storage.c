/* $egnet: storage.c,v 1.14 2007/09/23 16:27:22 dive Exp $ */

/*
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007
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

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

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
#include <sys/user.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__OpenBSD__) /* Theo's never gonna thank me for this but whatever */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
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

static inline int
refcount_overhead(Memory_Type type)
{
	/*
	 * These are the only allocation types that are addref()'d. As long
	 * as we're living on the wild side, avoid getting the refcount slot
	 * for allocations that won't need it.
	 */
	switch (type) {
	case M_FLOAT:
		/* for systems with picky double alignment */
		return MAX(sizeof(int), sizeof(double));
	case M_STRING:
		return sizeof(int);
	case M_LIST:
		/* for systems with picky pointer alignment */
		return MAX(sizeof(int), sizeof(Var *));
	default:
		return 0;
	}
}

void           *
mymalloc(unsigned size, Memory_Type type)
{
	char           *memptr;
	char            msg[100];
	int             offs;
	if (size == 0)		/* For queasy systems */
		size = 1;

	offs = refcount_overhead(type);

	memptr = (char *) malloc(size + offs);
	if (!memptr) {
		sprintf(msg, "memory allocation (size %u) failed!", size);
		panic(msg);
	}
	alloc_num[type]++;

	if (offs) {
		memptr += offs;
		((int *) memptr)[-1] = 1;
	}
	return memptr;
}

const char     *
str_ref(const char *s)
{
	addref(s);
	return s;
}

char           *
str_dup(const char *s)
{
	char           *r;

	if (s == 0 || *s == '\0') {
		static char    *emptystring;

		if (!emptystring) {
			emptystring = (char *) mymalloc(1, M_STRING);
			*emptystring = '\0';
		}
		addref(emptystring);
		return emptystring;
	} else {
		r = (char *) mymalloc(strlen(s) + 1, M_STRING);
		strcpy(r, s);
	}
	return r;
}

void           *
myrealloc(void *ptr, unsigned size, Memory_Type type)
{
	int             offs = refcount_overhead(type);
	static char     msg[100];

	ptr = realloc((char *) ptr - offs, size + offs);
	if (!ptr) {
		sprintf(msg, "memory re-allocation (size %u) failed!", size);
		panic(msg);
	}
	return (char *) ptr + offs;
}

void
myfree(void *ptr, Memory_Type type)
{
	alloc_num[type]--;
	free((char *) ptr - refcount_overhead(type));
}

#if defined(__linux__)
static size_t get_server_rss(void) {
    long size_pages = 0, rss_pages = 0;
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return 0;
    if (fscanf(f, "%ld %ld", &size_pages, &rss_pages) != 2) rss_pages = 0;
    fclose(f);
    return (size_t)rss_pages * (size_t)sysconf(_SC_PAGESIZE);
}

#elif defined(__FreeBSD__)
size_t get_server_rss(void) {
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };

    if (sysctl(mib, 4, &kp, &len, NULL, 0) != 0) return 0;
    return (size_t)kp.ki_rssize * (size_t)getpagesize();
}

#elif defined(__NetBSD__)
size_t get_server_rss(void) {
    struct kinfo_proc2 kp;
    size_t len = sizeof(kp);
    int mib[6] = { CTL_KERN, KERN_PROC2, KERN_PROC_PID, getpid(),
                   sizeof(kp), 1 };

    if (sysctl(mib, 6, &kp, &len, NULL, 0) != 0) return 0;
    return (size_t)kp.p_vm_rssize * (size_t)getpagesize();
}

#elif defined(__APPLE__)
size_t get_server_rss(void) {
    struct task_basic_info info;
    mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;

    if (task_info(mach_task_self(), TASK_BASIC_INFO,
                  (task_info_t)&info, &count) != KERN_SUCCESS) {
        return 0;
    }
    return (size_t)info.resident_size;
}

#elif defined(__OpenBSD__)
size_t get_server_rss(void) {
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    int mib[6] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid(),
                   sizeof(kp), 1 };

    if (sysctl(mib, 6, &kp, &len, NULL, 0) != 0) return 0;
    return (size_t)kp.p_vm_rssize * (size_t)getpagesize();
}

#else

#error "get_server_rss: unsupported platform"

#endif

Var
memory_usage(void)
{
	Var     r;
    size_t  rss;
    rss = get_server_rss();
    r.type = TYPE_INT;
    r.v.num = (int)rss;
	return r;
}
