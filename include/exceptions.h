/* $egnet: exceptions.h,v 1.6 2007/09/23 16:27:21 dive Exp $ */

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

/* Copyright 1989 Digital Equipment Corporation.                             */
/* Distributed only by permission.                                           */
/*****************************************************************************/
/* File: exceptions.h                                                        */
/* Taken originally from:                                                    */
/* Implementing Exceptions in C                                 */
/* Eric S. Roberts                                              */
/* Research Report #40                                          */
/* DEC Systems Research Center                                  */
/* March 21, 1989                                               */
/* Modified slightly by Pavel Curtis for use in the LambdaMOO server code.   */
/* ------------------------------------------------------------------------- */
/* The exceptions package provides a general exception handling mechanism    */
/* for use with C that is portable across a variety of compilers and         */
/* operating systems. The design of this facility is based on the            */
/* exception handling mechanism used in the Modula-2+ language at DEC/SRC    */
/* and is described in detail in the paper cited above.                      */
/* For more background on the underlying motivation for this design, see     */
/* SRC Research Report #3.                                                   */
/*****************************************************************************/

/*
 * Syntax:    Exception my_exception;
 * 
 * TRY stmts; EXCEPT (my_exception)   [ANY matches all exceptions] stmts;
 * [int exception_value available here] ... ENDTRY
 * 
 * RAISE(my_exception, value);
 * 
 * TRY stmts; FINALLY stmts; ENDTRY
 */

#ifndef Exceptions_H
#define Exceptions_H 1

#include <setjmp.h>

#include "config.h"

#define ES_MaxExceptionsPerScope	10

typedef enum ES_Value {
	ES_Initialize, ES_EvalBody, ES_Exception
}               ES_Value;

typedef struct {
	int             junk;
}               Exception;	/* Only addr. of exception is used. */

typedef volatile struct ES_CtxBlock ES_CtxBlock;
struct ES_CtxBlock {
	jmp_buf         jmp;
	int             nx;
	Exception      *array[ES_MaxExceptionsPerScope];
	Exception      *id;
	int             value;
	int             finally;
	ES_CtxBlock    *link;
};

extern Exception ANY;
extern ES_CtxBlock *ES_exceptionStack;
extern void     ES_RaiseException(Exception * exception, int value);

#define RAISE(e, v)	ES_RaiseException(&e, v)


#define TRY							\
	{							\
	    ES_CtxBlock		ES_ctx;				\
	    volatile ES_Value	ES_es = ES_Initialize;		\
								\
	    ES_ctx.nx = 0;					\
	    ES_ctx.finally = 0;					\
	    ES_ctx.link = ES_exceptionStack;			\
	    ES_exceptionStack = &ES_ctx;			\
	    							\
	    if (setjmp((void *) ES_ctx.jmp) != 0)		\
		ES_es = ES_Exception;				\
		    						\
	    while (1) {						\
		if (ES_es == ES_EvalBody) {			\
				/* TRY body goes here */


#define EXCEPT(e)						\
		    /* TRY body or handler goes here */		\
		    if (ES_es == ES_EvalBody)			\
			ES_exceptionStack = ES_ctx.link;	\
		    break;					\
		}						\
		if (ES_es == ES_Initialize) {			\
		    if (ES_ctx.nx >= ES_MaxExceptionsPerScope)	\
			server_panic("Too many EXCEPT clauses!");	\
		    ES_ctx.array[ES_ctx.nx++] = &e;		\
		} else if (ES_ctx.id == &e  ||  &e == &ANY) {	\
		    int	exception_value = ES_ctx.value;		\
								\
		    ES_exceptionStack = ES_ctx.link;		\
		    exception_value = exception_value;		\
			/* avoid warnings */			\
				/* handler goes here */


#define FINALLY							\
		    /* TRY body goes here */			\
		}						\
		if (ES_es == ES_Initialize)			\
		    ES_ctx.finally = 1;				\
		else {						\
		    ES_exceptionStack = ES_ctx.link;		\
		    /* FINALLY body goes here */		\


#define ENDTRY								\
		    /* FINALLY body or handler goes here */		\
		    if (ES_ctx.finally  &&  ES_es == ES_Exception)  	\
			ES_RaiseException((Exception *) ES_ctx.id,	\
					  (int) ES_ctx.value);		\
		    break;						\
		}							\
		ES_es = ES_EvalBody;					\
	    }								\
	}


/* The exceptions package doesn't provide this function, but it calls it */
/* whenever a fatal error occurs:                                        */
/* 1) Too many EXCEPT clauses in a single TRY construct.            */
/* 2) An unhandled exception is RAISEd.                             */

extern void     server_panic(const char *message);

#endif				/* !Exceptions_H */
