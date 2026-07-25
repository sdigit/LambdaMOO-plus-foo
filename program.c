/* $egnet: program.c,v 1.8 2007/09/23 16:27:22 dive Exp $ */

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

#include <string.h>

#include "ast.h"
#include "exceptions.h"
#include "list.h"
#include "parser.h"
#include "program.h"
#include "storage.h"
#include "structures.h"
#include "utils.h"

Program        *
new_program(void)
{
	Program        *p = (Program *) mymalloc(sizeof(Program), M_PROGRAM);

	p->ref_count = 1;
	p->first_lineno = 1;
	p->cached_lineno = 1;
	p->cached_lineno_pc = 0;
	return p;
}

Program        *
null_program(void)
{
	static Program *p = 0;
	Var             code, errors;

	if (!p) {
		code = new_list(0);
		p = parse_list_as_program(code, &errors);
		if (!p)
			panic("Can't create the null program!");
		free_var(code);
		free_var(errors);
	}
	return p;
}

Program        *
program_ref(Program * p)
{
	p->ref_count++;

	return p;
}

int
program_bytes(Program * p)
{
	int             i, count;

	count = sizeof(Program);
	count += p->main_vector.size;

	for (i = 0; i < p->num_literals; i++)
		count += value_bytes(p->literals[i]);

	count += sizeof(Bytecodes) * p->fork_vectors_size;
	for (i = 0; i < p->fork_vectors_size; i++)
		count += p->fork_vectors[i].size;

	count += sizeof(const char *) * p->num_var_names;
	for (i = 0; i < p->num_var_names; i++)
		count += strlen(p->var_names[i]) + 1;

	return count;
}

void
free_program(Program * p)
{
	unsigned        i;

	p->ref_count--;
	if (p->ref_count == 0) {

		for (i = 0; i < p->num_literals; i++)
			/*
			 * can't be a list--strings and floats need to be
			 * freed, though.
			 */
			free_var(p->literals[i]);
		if (p->literals)
			myfree(p->literals, M_LIT_LIST);

		for (i = 0; i < p->fork_vectors_size; i++)
			myfree(p->fork_vectors[i].vector, M_BYTECODES);
		if (p->fork_vectors_size)
			myfree(p->fork_vectors, M_FORK_VECTORS);

		for (i = 0; i < p->num_var_names; i++)
			free_str(p->var_names[i]);
		myfree(p->var_names, M_NAMES);

		myfree(p->main_vector.vector, M_BYTECODES);

		myfree(p, M_PROGRAM);
	}
}
