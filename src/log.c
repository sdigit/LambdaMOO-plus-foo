/* $egnet: log.c,v 1.11 2007/09/23 16:27:21 dive Exp $ */

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
  Copyright (c) 1992, 1995, 1996 Xerox Corporation.  All rights reserved.
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#if defined(__linux__) || defined(LOG_STRFTIME)
#include <time.h>
#endif
#include <sys/time.h>

#include "bf_register.h"
#include "config.h"
#include "functions.h"
#include "log.h"
#include "options.h"
#include "storage.h"
#include "streams.h"
#include "utils.h"

static FILE *log_file = 0;

void set_log_file(FILE *f) { log_file = f; }

static void do_log(const char *fmt, va_list args, const char *prefix) {
    FILE *f;

    if (log_file) {
#ifndef LOG_STRFTIME
        time_t now = time(0);
        char *nowstr = ctime(&now);

        nowstr[19] = '\0'; /* kill the year and newline at the
                            * end */
        f = log_file;
        fprintf(f, "%s: %s", nowstr + 4, prefix); /* skip the day of week */
#else
        time_t a;
        struct tm *t;
        size_t len;
        char *timebuf;

        a = time(0);
        t = localtime(&a);
#if defined(__sun__) &&                                                        \
    defined(__svr4__) /* no tm_zone in struct tm on Solaris... */
        len = 23 + strlen(tzname[0]);
#else
        len = 23 + strlen(t->tm_zone);
#endif
        timebuf = (char *)mymalloc(len, M_STRING);
        len = strftime(timebuf, len, "%Y/%m/%d %H:%M:%S (%Z)", t);
        f = log_file;
        fprintf(f, "%s: %s", timebuf, prefix);
        myfree(timebuf, M_STRING);
#endif /* LOG_STRFTIME */
    } else
        f = stderr;

    vfprintf(f, fmt, args);
    fflush(f);
}

void oklog(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    do_log(fmt, args, "");
    va_end(args);
}

void errlog(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    do_log(fmt, args, "*** ");
    va_end(args);
}

void log_perror(const char *what) { errlog("%s: %s\n", what, strerror(errno)); }

#ifdef LOG_COMMANDS
static Stream *command_history = 0;
#endif

void reset_command_history() {
#ifdef LOG_COMMANDS
    if (command_history == 0)
        command_history = new_stream(1024);
    else
        reset_stream(command_history);
#endif
}

void log_command_history() {
#ifdef LOG_COMMANDS
    errlog("COMMAND HISTORY:\n%s", stream_contents(command_history));
#endif
}

void add_command_to_history([[maybe_unused]] Objid player,
                            [[maybe_unused]] const char *command) {
#ifdef LOG_COMMANDS
    time_t now = time(0);
    char *nowstr = ctime(&now);

    nowstr[19] = '\0'; /* kill the year and newline at the end */
    stream_printf(command_history, "%s: #%d: %s\n",
                  nowstr + 4, /* skip day of week */
                  player, command);
#endif /* LOG_COMMANDS */
}

/**** built in functions ****/

static package bf_server_log(Var arglist, [[maybe_unused]] Byte next,
                             [[maybe_unused]] void *vdata, Objid progr) {
    if (!is_wizard(progr)) {
        free_var(arglist);
        return make_error_pack(E_PERM);
    } else {
        int is_error =
            (arglist.v.list[0].v.num == 2 && is_true(arglist.v.list[2]));

        if (is_error)
            errlog("> %s\n", arglist.v.list[1].v.str);
        else
            oklog("> %s\n", arglist.v.list[1].v.str);

        free_var(arglist);
        return no_var_pack();
    }
}

void register_log(void) {
    register_function("server_log", 1, 2, bf_server_log, TYPE_STR, TYPE_ANY);
}
