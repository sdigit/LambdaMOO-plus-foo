/* $egnet: name_lookup.c,v 1.9 2007/09/23 16:27:21 dive Exp $ */

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

/******************************************************************************
  Copyright (c) 1994, 1995, 1996 Xerox Corporation.  All rights reserved.
  Portions of this code were written by Stephen White, aka ghond.
  Use and copying of this software and preparation of derivative works based
  upon this software are permitted.  Any distribution of this software or
  derivative works must comply with all applicable United States export
  control laws.  This software is made available AS IS, and Xerox Corporation
  makes no warranty about the software, its performance or its conformity to
  any specification.  Any person obtaining a copy of this software is requested
  to send their name and post office or electronic mail address to:
    Pavel Curtis
    Xerox PARC
    3333 Coyote Hill Rd.
    Palo Alto, CA 94304
    Pavel@Xerox.Com
 *****************************************************************************/

/*
 * This module provides IP host name lookup with timeouts.
 *
 * Historically this ran the actual gethostbyname()/gethostbyaddr() calls in
 * a forked subprocess, because longjmp'ing out of those calls (which is
 * what happens here if a lookup exceeds its timeout -- see timers.h) can
 * corrupt the static/global state some resolver libraries keep internally.
 * A subprocess made that safe: worst case, you longjmp'd out of a *pipe
 * read*, which is fine, and the possibly-wedged child got SIGKILLed.
 *
 * That hazard is specific to gethostbyname()/gethostbyaddr(), which POSIX
 * explicitly permits to be non-reentrant. Their replacements, getaddrinfo()
 * and getnameinfo(), are required to be reentrant/thread-safe: they return
 * heap-allocated results and keep no shared static state. That means the
 * blocking call itself can safely run on a plain detached pthread instead
 * of in a forked child -- the caller never longjmps out of the call, it
 * just stops waiting on a condition variable, and the thread is free to
 * finish (or not) on its own time.
 *
 * IMPORTANT: because that worker thread runs concurrently with the rest of
 * this single-threaded, cooperatively-scheduled server, it must NEVER call
 * back into any other MOO subsystem. In particular: no mymalloc()/myfree()
 * (storage.c's alloc_num[] counters are plain globals, not atomics), no
 * oklog()/errlog()/log_perror() (log.c is not reentrant either), and no
 * timers.c or exceptions.c. The worker touches only its own stack, the
 * resolver, and plain libc malloc/free/strdup.
 */

#include "options.h"

#include <arpa/inet.h> /* inet_addr() */
#include <errno.h>
#include <netdb.h>      /* struct addrinfo, getaddrinfo(), getnameinfo() */
#include <netinet/in.h> /* struct sockaddr_in, INADDR_ANY, htons(),
			 * htonl(), ntohl(), struct in_addr */
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h> /* AF_INET */
#include <time.h>

#include "config.h"
#include "log.h"
#include "storage.h"

/******************************************************************************
 * Bookkeeping shared between the caller and a lookup's worker thread.
 *
 * A lookup_ctx is refcounted rather than owned outright by either side,
 * because the caller may give up and move on (on timeout) before the
 * worker thread -- which cannot be safely killed mid-resolver-call any
 * more than the old subprocess's child could be, without reintroducing
 * the exact hazard this rewrite exists to avoid -- has actually finished.
 * Whichever side (caller giving up, or worker completing) drops the last
 * reference is the one that frees it.
 *****************************************************************************/

enum request_kind { REQ_NAME_FROM_ADDR, REQ_ADDR_FROM_NAME };

struct lookup_ctx {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int refcount; /* protected by mutex; starts at 2 */
    int done;     /* protected by mutex */

    enum request_kind kind;

    /* Request, for REQ_ADDR_FROM_NAME. */
    char *name; /* strdup()'d */

    /* Request, for REQ_NAME_FROM_ADDR. */
    struct sockaddr_in address;

    /* Result, for REQ_ADDR_FROM_NAME: 0 on failure. */
    unsigned32 addr_result;

    /* Result, for REQ_NAME_FROM_ADDR: strdup()'d, or NULL on failure. */
    char *name_result;
};

/*
 * Bound the number of concurrently outstanding lookup threads. Nothing
 * forcibly reclaims a thread stuck on a hung/blackholed resolver -- same as
 * before, a stuck lookup is abandoned rather than killed -- so without a
 * cap, a flood of connections during a DNS outage could accumulate
 * unbounded threads. When the cap is hit we fail the lookup immediately,
 * same as the old "lookup dead and wouldn't restart" fallback.
 */
#define MAX_OUTSTANDING_LOOKUPS 128
static atomic_int outstanding_lookups = 0;

static struct lookup_ctx *new_ctx(enum request_kind kind) {
    struct lookup_ctx *ctx = malloc(sizeof *ctx);

    if (!ctx)
        return 0;
    pthread_mutex_init(&ctx->mutex, 0);
    pthread_cond_init(&ctx->cond, 0);
    ctx->refcount = 2; /* one for the caller, one for the worker */
    ctx->done = 0;
    ctx->kind = kind;
    ctx->name = 0;
    ctx->addr_result = 0;
    ctx->name_result = 0;
    return ctx;
}

static void free_ctx(struct lookup_ctx *ctx) {
    pthread_mutex_destroy(&ctx->mutex);
    pthread_cond_destroy(&ctx->cond);
    free(ctx->name);
    free(ctx->name_result);
    free(ctx);
}

static void release_ctx(struct lookup_ctx *ctx) {
    /* Caller must hold no lock; this acquires and releases ctx->mutex. */
    int last;

    pthread_mutex_lock(&ctx->mutex);
    last = (--ctx->refcount == 0);
    pthread_mutex_unlock(&ctx->mutex);

    if (last)
        free_ctx(ctx);
}

/******************************************************************************
 * Code that runs on the worker thread. MUST NOT touch anything but its own
 * stack, the resolver, and plain libc malloc/free/strdup -- see the note
 * at the top of this file.
 *****************************************************************************/

static void *lookup_worker(void *arg) {
    struct lookup_ctx *ctx = arg;

    if (ctx->kind == REQ_ADDR_FROM_NAME) {
        struct addrinfo hints, *res = 0;

        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(ctx->name, 0, &hints, &res) == 0 && res) {
            /*
             * Raw network-byte-order bits out of the sockaddr_in, same as
             * the old code's h_addr_list[0]/inet_addr() copy: this is
             * meant to be stored straight into a sockaddr_in.sin_addr,
             * not byte-swapped.
             */
            ctx->addr_result =
                ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
            freeaddrinfo(res);
        }
    } else {
        char host[NI_MAXHOST];

        /*
         * NI_NAMEREQD: fail rather than fall back to a numeric string, so
         * "no PTR record" comes back as failure here (matching the old
         * gethostbyaddr()-returned-NULL case) and the caller's existing
         * dotted-decimal fallback is what actually produces the numeric
         * form.
         */
        if (getnameinfo((struct sockaddr *)&ctx->address, sizeof ctx->address,
                        host, sizeof host, 0, 0, NI_NAMEREQD) == 0)
            ctx->name_result = strdup(host);
    }

    pthread_mutex_lock(&ctx->mutex);
    ctx->done = 1;
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->mutex);

    atomic_fetch_sub(&outstanding_lookups, 1);
    release_ctx(ctx);
    return 0;
}

/*
 * Spawn a detached worker for ctx and wait up to `timeout' seconds for it
 * to finish. Returns true iff it finished in time, in which case the
 * result fields in ctx are valid.
 *
 * Contract: ctx arrives with refcount == 2, nominally one for the caller
 * and one for "whoever ends up running the lookup". run_lookup() always
 * consumes exactly that second one itself -- releasing it immediately if
 * the thread never gets spawned, or handing it to the thread to release
 * on completion if it does. Either way, ctx is guaranteed to still be
 * alive (held by the caller's own reference) when run_lookup() returns,
 * and the caller must release_ctx() it exactly once, regardless of the
 * return value, once it's done reading any result fields it needs.
 */
static int run_lookup(struct lookup_ctx *ctx, unsigned timeout) {
    pthread_t tid;
    pthread_attr_t attr;
    struct timespec deadline;
    int finished;

    if (atomic_fetch_add(&outstanding_lookups, 1) >= MAX_OUTSTANDING_LOOKUPS) {
        atomic_fetch_sub(&outstanding_lookups, 1);
        errlog("NAME_LOOKUP: Too many outstanding lookups; skipping\n");
        release_ctx(ctx);
        return 0;
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, lookup_worker, ctx) != 0) {
        pthread_attr_destroy(&attr);
        atomic_fetch_sub(&outstanding_lookups, 1);
        log_perror("NAME_LOOKUP: pthread_create() failed");
        release_ctx(ctx);
        return 0;
    }
    pthread_attr_destroy(&attr);
    /* Thread spawned: it now owns the reference it was given and will
     * release_ctx() it itself when done. */

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout;

    pthread_mutex_lock(&ctx->mutex);
    while (!ctx->done) {
        if (pthread_cond_timedwait(&ctx->cond, &ctx->mutex, &deadline) ==
            ETIMEDOUT)
            break;
    }
    finished = ctx->done;
    pthread_mutex_unlock(&ctx->mutex);

    if (!finished)
        oklog("NAME_LOOKUP: Timed out; abandoning lookup in background\n");

    /*
     * If finished, the worker's writes to ctx's result fields happened
     * before it took the mutex to set ctx->done, and we've since taken
     * that same mutex ourselves, so it's safe for the caller to read them
     * now, before releasing its reference.
     */
    return finished;
}

/******************************************************************************
 * Public API. Same signatures as the old subprocess-based implementation,
 * so callers (network.c) need no changes.
 *****************************************************************************/

int initialize_name_lookup(void) {
    /* Nothing to set up: each lookup is entirely self-contained. */
    return 1;
}

const char *lookup_name_from_addr(struct sockaddr_in *addr, unsigned timeout,
                                  int name_lookup) {
    static char *buffer = 0;

    if (name_lookup) {
        struct lookup_ctx *ctx = new_ctx(REQ_NAME_FROM_ADDR);

        if (ctx) {
            int got_result;

            ctx->address = *addr;
            got_result = run_lookup(ctx, timeout) && ctx->name_result;
            if (got_result) {
                free(buffer);
                buffer = strdup(ctx->name_result);
            }
            release_ctx(ctx);
            if (got_result && buffer)
                return buffer;
        }
    }

    /*
     * Either name_lookup was false, the lookup failed or timed out, or we
     * couldn't even start it (allocation failure); fall back to the
     * default, dotted-decimal notation.
     */
    {
        static char decimal[20];
        unsigned32 a = ntohl(addr->sin_addr.s_addr);

        sprintf(decimal, "%u.%u.%u.%u", (unsigned)(a >> 24) & 0xff,
                (unsigned)(a >> 16) & 0xff, (unsigned)(a >> 8) & 0xff,
                (unsigned)a & 0xff);
        return decimal;
    }
}

unsigned32 lookup_addr_from_name(const char *name, unsigned timeout) {
    struct lookup_ctx *ctx = new_ctx(REQ_ADDR_FROM_NAME);
    unsigned32 addr = 0;
    int got_result = 0;

    if (ctx)
        ctx->name = strdup(name);

    if (ctx && ctx->name) {
        got_result = run_lookup(ctx, timeout);
        if (got_result)
            addr = ctx->addr_result;
        release_ctx(ctx); /* always exactly one release after run_lookup */
    } else if (ctx) {
        /* strdup() failed before we ever handed ctx to run_lookup(), so
         * both of its references are still ours to drop. */
        free_ctx(ctx);
    }

    if (!got_result)
        /* Allocation failure, resolution failure, or timeout: numeric
         * addresses should still work without any of the above. */
        addr = inet_addr((void *)name);

    return addr == 0xffffffff ? 0 : addr;
}
