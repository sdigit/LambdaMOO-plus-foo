/* $egnet: utils.h,v 1.7 2007/09/23 16:27:22 dive Exp $ */

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

#ifndef Utils_h
#define Utils_h 1

#include <stdio.h>

#include "config.h"
#include "execute.h"

#undef MAX
#undef MIN
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

#define Arraysize(x) (sizeof(x) / sizeof(*x))

extern int      mystrcasecmp(const char *, const char *);
extern int      mystrncasecmp(const char *, const char *, int);

extern int      verbcasecmp(const char *verb, const char *word);

extern unsigned str_hash(const char *);

extern void     complex_free_var(Var);
extern Var      complex_var_ref(Var);
extern Var      complex_var_dup(Var);
extern int      var_refcount(Var);

static inline void
free_var(Var v)
{
	if (v.type & TYPE_COMPLEX_FLAG)
		complex_free_var(v);
}

static inline   Var
var_ref(Var v)
{
	if (v.type & TYPE_COMPLEX_FLAG)
		return complex_var_ref(v);
	else
		return v;
}

static inline   Var
var_dup(Var v)
{
	if (v.type & TYPE_COMPLEX_FLAG)
		return complex_var_dup(v);
	else
		return v;
}

extern int      equality(Var lhs, Var rhs, int case_matters);
extern int      is_true(Var v);

extern char    *strsub(const char *, const char *, const char *, int);
extern int      strindex(const char *, const char *, int);
extern int      strrindex(const char *, const char *, int);

extern Var      get_system_property(const char *);
extern Objid    get_system_object(const char *);

extern int      value_bytes(Var);

extern const char *raw_bytes_to_binary(const char *buffer, int buflen);
extern const char *binary_to_raw_bytes(const char *binary, int *rawlen);

#endif
