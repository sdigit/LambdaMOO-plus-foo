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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <unistd.h>

#include "config.h"
#include "exceptions.h"
#include "list.h"
#include "options.h"
#include "ref_count.h"
#include "storage.h"
#include "structures.h"
#include "utils.h"

static unsigned alloc_num[Sizeof_Memory_Type];

typedef struct {
    long sec;
    long usec;
} proc_cpu_time_t;

int get_proc_cpu_time(pid_t pid, proc_cpu_time_t *cpu_time) {
    int mib[6];
    size_t size;
    struct kinfo_proc2 p;

    // Setup the sysctl MIB array for a single PID
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC2;
    mib[2] = KERN_PROC_PID;
    mib[3] = (int)pid;
    mib[4] = sizeof(struct kinfo_proc2);
    mib[5] = 1; // Retrieve exactly 1 structure

    size = sizeof(p);

    // Query the kernel
    if (sysctl(mib, 6, &p, &size, NULL, 0) < 0) {
        return -1; // Process not found or permission denied
    }

    // Extract total CPU time directly from the runtime fields
    cpu_time->sec  = p.p_rtime_sec;
    cpu_time->usec = p.p_rtime_usec;

    return 0;
}

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

Var
memory_usage(void)
{
	Var             r;
    pid_t           p;
    proc_cpu_time_t t;
    p = getpid();
    if (get_proc_cpu_time(p, &t) != 0)
    {
    	r = new_list(0);
	    return r;
    } else {
        r = new list(2);
        r.v.list[1] = TYPE_INT;
        r.v.list[2] = TYPE_INT;
        r.v.list[1] = t.sec;
        r.v.list[2] = t.usec;
        return r;
    }
}
