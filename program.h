/* $egnet: program.h,v 1.6 2007/09/23 16:27:22 dive Exp $ */

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

#ifndef Program_H
#define Program_H

#include "structures.h"
#include "version.h"

typedef unsigned char Byte;

typedef struct {
	Byte            numbytes_label, numbytes_literal, numbytes_fork,
	                numbytes_var_name, numbytes_stack;
	Byte           *vector;
	unsigned        size;
	unsigned        max_stack;
}               Bytecodes;

typedef struct {
	DB_Version      version;
	unsigned        first_lineno;
	unsigned        ref_count;

	Bytecodes       main_vector;

	unsigned        num_literals;
	Var            *literals;

	unsigned        fork_vectors_size;
	Bytecodes      *fork_vectors;

	unsigned        num_var_names;
	const char    **var_names;

	unsigned        cached_lineno;
	unsigned        cached_lineno_pc;
	unsigned        cached_lineno_vec;
}               Program;

#define MAIN_VECTOR 	-1	/* As opposed to an index into fork_vectors */

extern Program *new_program(void);
extern Program *null_program(void);
extern Program *program_ref(Program *);
extern int      program_bytes(Program *);
extern void     free_program(Program *);

#endif				/* !Program_H */
