/* $egnet: options.h,v 1.29 2010/07/11 22:08:59 dive Exp $ */

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

#ifndef Options_h
#define Options_h 1

/*
 * If WRITEPIDFILE is defined, write the PID of the main MOO process to PIDFILE.
 *
 * NOTE: If you use this option, do not run multiple MOOs in the same directory,
 * or make sure that PIDFILE is defined differently for each one, or else the
 * PID file will be overwritten.
 */

#define WRITEPIDFILE

#ifdef WRITEPIDFILE
#define PIDFILE "MOO.pid"
#endif				/* WRITEPIDFILE */

/*
 * The server is prepared to keep a log of every command entered by players
 * since the last checkpoint.  The log is flushed whenever a checkpoint is
 * successfully begun and is dumped into the server log when and if the server
 * panics.  Define LOG_COMMANDS to enable this logging.
 *
 */

#undef LOG_COMMANDS

/*
 * If this is defined, the signal handlers for SIGSEGV and SIGBUS will not
 * be installed. This means that if the server segfaults, it will die and
 * drop core like most any other unix program, not try to panic-dump. The idea
 * is to ease debugging.
 */

#undef NO_CRASHHANDLER

/*
 * If this is defined, the server log will be in the format:
 * YYYY/MM/DD HH:MM:SS (TZ): <data>
 * instead of ctime format.
 */

#define LOG_STRFTIME

/*
 * The server normally forks a separate process to make database checkpoints;
 * the original process continues to service user commands as usual while the
 * new process writes out the contents of its copy of memory to a disk file.
 * This checkpointing process can take quite a while, depending on how big your
 * database is, so it's usually quite convenient that the server can continue
 * to be responsive while this is taking place.  On some systems, however,
 * there may not be enough memory to support two simultaneously running server
 * processes.  Define UNFORKED_CHECKPOINTS to disable server forking for
 * checkpoints.
 */

#undef UNFORKED_CHECKPOINTS

/*
 * If OUT_OF_BAND_PREFIX is defined as a non-empty string, then any lines of
 * input from any player that begin with that prefix will bypass both normal
 * command parsing and any pending read()ing task, instead spawning a server
 * task invoking #0:do_out_of_band_command(word-list).  This is intended for
 * use by fancy clients that need to send reliably-understood messages to the
 * server.
 */

#define OUT_OF_BAND_PREFIX "#$#"

/*
 * The following constants define the execution limits placed on all MOO tasks.
 *
 * DEFAULT_MAX_STACK_DEPTH is the default maximum depth allowed for the MOO
 *	verb-call stack, the maximum number of dynamically-nested calls at any
 *	given time.  If defined in the database and larger than this default,
 *	$server_options.max_stack_depth overrides this default.
 * DEFAULT_FG_TICKS and DEFAULT_BG_TICKS are the default maximum numbers of
 *	`ticks' (basic operations) any task is allowed to use without
 *	suspending.  If defined in the database, $server_options.fg_ticks and
 *	$server_options.bg_ticks override these defaults.
 * DEFAULT_FG_SECONDS and DEFAULT_BG_SECONDS are the default maximum numbers of
 *	real-time seconds any task is allowed to use without suspending.  If
 *	defined in the database, $server_options.fg_seconds and
 *	$server_options.bg_seconds override these defaults.
 *
 * The *FG* constants are used only for `foreground' tasks (those started by
 * either player input or the server's initiative and that have never
 * suspended); the *BG* constants are used only for `background' tasks (forked
 * tasks and those of any kind that have suspended).
 *
 * The values given below are documented in the LambdaMOO Programmer's Manual,
 * so they should either be left as they are or else the manual should be
 * updated.
 */

#define DEFAULT_MAX_STACK_DEPTH	32

#define DEFAULT_FG_TICKS	131072
#define DEFAULT_BG_TICKS	65536

#define DEFAULT_FG_SECONDS	8
#define DEFAULT_BG_SECONDS	4

/* the default mode for FUP to make directories */

#define CREATE_NEW_DIR_MODE             0755

/*
 * FIO_SUBDIR defines the subdirectory to use for the File/IO code.
 * FUP_SUBDIR defines the subdirectory to use for the FUP code.
 */

#define FIO_SUBDIR	"files-FIO/"
#define	FUP_SUBDIR	"files-FUP/"

#define DEFAULT_PORT 		4500

/*
 * Define OUTBOUND_NETWORK to enable the built-in MOO function
 * open_network_connection(), which allows (only) wizard-owned MOO code to make
 * outbound network connections from the server.
 * *** THINK VERY HARD BEFORE ENABLING THIS FUNCTION ***
 * In some contexts, this could represent a serious breach of security.  By
 * default, the open_network_connection() function is disabled, always raising
 * E_PERM when called.
 *
 *
 * The above statement should be ignored. If you want player creation mails to
 * be able to go out, you need this enabled. It is only a security hazard if you
 * have untrustworthy wizards on your MOO. -dive
 */

#define OUTBOUND_NETWORK

/*
 * The following constants define certain aspects of the server's network
 * behavior if NETWORK_PROTOCOL is not defined as NP_SINGLE.
 *
 * MAX_QUEUED_OUTPUT is the maximum number of output characters the server is
 *		     willing to buffer for any given network connection before
 *		     discarding old output to make way for new.  The server
 *		     only discards output after attempting to send as much as
 *		     possible on the connection without blocking.
 * MAX_QUEUED_INPUT is the maximum number of input characters the server is
 *		    willing to buffer from any given network connection before
 *		    it stops reading from the connection at all.  The server
 *		    starts reading from the connection again once most of the
 *		    buffered input is consumed.
 * DEFAULT_CONNECT_TIMEOUT is the default number of seconds an un-logged-in
 *			   connection is allowed to remain idle without being
 *			   forcibly closed by the server; this can be
 *			   overridden by defining the `connect_timeout'
 *			   property on $server_options or on L, for connections
 *			   accepted by a given listener L.
 */

#define MAX_QUEUED_OUTPUT	131072
#define MAX_QUEUED_INPUT	MAX_QUEUED_OUTPUT
#define DEFAULT_CONNECT_TIMEOUT	300


/*
 * The following constants define certain aspects of the server's network
 * listener points behavior when they are created. They can be modified on a
 * per port basis by using the options argument in the listen() built-in function.
 *
 * DEFAULT_LISTENER_PRINT_MESSAGES is a boolean. If it is true, then the various
 *        database-configurable messages (also detailed in the chapter on server
 *        assumptions) will be printed on connections received at the new
 *        listening point. $server_options.listener_print_message allows to
 *        override this default.
 *
 * DEFAULT_LISTENER_NAME_LOOKUP is a boolean. If it is true then incoming
 *        connections (at the new listening point) hostname will be looked up
 *        from the IP address and be accessible via connection_name(). If it
 *        is false then connection_name() will return the IP address.
 *        $server_options.listener_name_lookup allows to override this default.
 *
 *
 * Note: the server initial connection in the main loop always use the above
 * DEFAULT constants even if they are overriden by $server_options properties.
 */

#define DEFAULT_LISTENER_PRINT_MESSAGES 1
#define DEFAULT_LISTENER_NAME_LOOKUP  1


/*
 * The server maintains a cache of the most recently used patterns from calls
 * to the match() and rmatch() built-in functions.  PATTERN_CACHE_SIZE controls
 * how many past patterns are remembered by the server.  Do not set it to a
 * number less than 1.
 */

#define PATTERN_CACHE_SIZE	20

/*
 * If you don't plan on using protecting built-in properties (like
 * .name and .location), define IGNORE_PROP_PROTECTED.  The extra
 * property lookups on every reference to a built-in property are
 * expensive.
 */

#undef IGNORE_PROP_PROTECTED

/*
 * The code generator can now recognize situations where the code will not
 * refer to the value of a variable again and generate opcodes that will
 * keep the interpreter from holding references to the value in the runtime
 * environment variable slot.  Before, when doing something like x=f(x), the
 * interpreter was guaranteed to have a reference to the value of x while f()
 * was running, meaning that f() always had to copy x to modify it.  With
 * BYTECODE_REDUCE_REF enabled, f() could be called with the last reference
 * to the value of x.  So for example, x={@x,y} can (if there are no other
 * references to the value of x in variables or properties) just append to
 * x rather than make a copy and append to that.  If it *does* have to copy,
 * the next time (if it's in a loop) it will have the only reference to the
 * copy and then it can take advantage.
 *
 * NOTE WELL    NOTE WELL    NOTE WELL    NOTE WELL    NOTE WELL
 *
 * This option affects the length of certain bytecode sequences.
 * Suspended tasks in a database from a server built with this option
 * are not guaranteed to work with a server built without this option,
 * and vice versa.  It is safe to flip this switch only if there are
 * no suspended tasks in the database you are loading.  (It might work
 * anyway, but hey, it's your database.)  This restriction will be
 * lifted in a future version of the server software.  Consider this
 * option as being BETA QUALITY until then.
 *
 * NOTE WELL    NOTE WELL    NOTE WELL    NOTE WELL    NOTE WELL
 *
 */

#undef BYTECODE_REDUCE_REF

#ifdef BYTECODE_REDUCE_REF
 #error Think carefully before enabling BYTECODE_REDUCE_REF.  This feature is still beta.  Comment out this line if you are sure.
#endif

/*
 * The server can merge duplicate strings on load to conserve memory.  This
 * involves a rather expensive step at startup to dispose of the table used
 * to find the duplicates.  This should be improved eventually, but you may
 * want to trade off faster startup time for increased memory usage.
 *
 * You might want to turn this off if you see a large delay before the
 * INTERN: lines in the log at startup.
 */

#define STRING_INTERNING	/* */

/*****************************************************************************
 ********** You shouldn't need to change anything below this point. **********
 *****************************************************************************/

#ifndef OUT_OF_BAND_PREFIX
#define OUT_OF_BAND_PREFIX ""
#endif

#if PATTERN_CACHE_SIZE < 1
 #error Illegal match() pattern cache size!
#endif

#include "config.h"

#endif				/* !Options_h */
