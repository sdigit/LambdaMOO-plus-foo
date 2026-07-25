dnl Process this file with `autoconf' to create `configure'.
dnl
dnl Copyright (c) 1992, 1995, 1996 Xerox Corporation.  All rights reserved.
dnl Portions of this code were written by Stephen White, aka ghond.
dnl Use and copying of this software and preparation of derivative works based
dnl upon this software are permitted.  Any distribution of this software or
dnl derivative works must comply with all applicable United States export
dnl control laws.  This software is made available AS IS, and Xerox Corporation
dnl makes no warranty about the software, its performance or its conformity to
dnl any specification.  Any person obtaining a copy of this software is
dnl requested to send their name and post office or electronic mail address to:
dnl   Pavel Curtis
dnl   Xerox PARC
dnl   3333 Coyote Hill Rd.
dnl   Palo Alto, CA 94304
dnl   Pavel@Xerox.Com
dnl

dnl First pass at a revision which works with autoconf 2
dnl  by Nathanael Nerode (neroden@twcny.rr.com)
dnl This appears to work correctly, but I'm not guru enough to be sure.

dnl mooserver+foo changes: got rid of checks for OS's that nobody is likely to
dnl ever want to use a MOO on. Intentionally forget SCO support. Also got rid of
dnl FIFO checks, since they're not used anymore by the server.
dnl $egnet: configure.ac,v 1.14 2005/06/18 02:41:42 dive Exp $

AC_INIT(Makefile.in)
AC_CONFIG_HEADER(config.h)

dnl ***************************************************************************
dnl 	MOO_MALLOC_H
dnl Define NEED_MALLOC_H if `malloc()' is not declared in <stdlib.h> and
dnl <malloc.h> exists.
AC_DEFUN(MOO_MALLOC_H, [
MOO_FUNC_DECL_CHECK(stdlib.h, malloc, , 
[AC_CHECK_HEADER(malloc.h, [AC_DEFINE(NEED_MALLOC_H)])])
])

dnl ***************************************************************************
dnl	MOO_FUNC_DECL_CHECK(header, func,
dnl			    if-declared [, if-not-declared])
dnl Do `if-declared' is `func' is declared in `header', and if-not-declared
dnl otherwise.
dnl Also print checking messages.
dnl
AC_DEFUN(MOO_FUNC_DECL_CHECK, [
changequote(,)dnl
pattern="[^_a-zA-Z0-9]$2 *\("
changequote([,])dnl
AC_MSG_CHECKING([whether $2 is declared in $1])
AC_EGREP_HEADER($pattern, $1, [
	AC_MSG_RESULT([yes])
	$3
], [
	AC_MSG_RESULT([no])
	$4
])
])

dnl ***************************************************************************
dnl 	MOO_NDECL_FUNCS(header, func1 func2 ...)
dnl Defines NDECL_func1, NDECL_func2, ... if they are not declared in header.
dnl
AC_DEFUN(MOO_NDECL_FUNCS, [
changequote(,)dnl
trfrom='[a-z]' trto='[A-Z]'
changequote([,])dnl
for func in $2
do
func_norm=`echo $func | tr "$trfrom" "$trto"`
MOO_FUNC_DECL_CHECK($1, $func, ,
	AC_DEFINE_UNQUOTED(NDECL_$func_norm))
done
])

dnl ***************************************************************************
dnl	MOO_VAR_DECL_CHECK(header, variable,
dnl			   if-declared [, if-not-declared])
dnl Do `if-declared' is `variable' is declared in `header', and if-not-declared
dnl otherwise.
AC_DEFUN(MOO_VAR_DECL_CHECK, [
changequote(,)dnl
pattern="[^_a-zA-Z0-9]$2"
changequote([,])dnl
AC_MSG_CHECKING(whether $2 is declared in $1)
AC_EGREP_HEADER($pattern, $1, [
   AC_MSG_RESULT(yes)
   $3
], [
   AC_MSG_RESULT(no)
   $4
])
])

dnl ***************************************************************************
dnl 	MOO_NDECL_VARS(header, var1 var2 ...)
dnl Defines NDECL_var1, NDECL_var2, ... if they are not declared in header.
AC_DEFUN(MOO_NDECL_VARS, [
changequote(,)dnl
trfrom='[a-z]' trto='[A-Z]'
changequote([,])dnl
for var in $2
do
var_norm=`echo $var | tr "$trfrom" "$trto"`
MOO_VAR_DECL_CHECK($1, $var, ,
	AC_DEFINE(NDECL_$var_norm))
done
])

dnl ***************************************************************************
dnl 	MOO_HEADER_STANDS_ALONE(header [, extra-code])
dnl Defines header_NEEDS_HELP if can't be compiled all by itself.
AC_DEFUN(MOO_HEADER_STANDS_ALONE, [
changequote(,)dnl
trfrom='[a-z]./' trto='[A-Z]__'
changequote([,])dnl
one_norm=`echo $1 | tr "$trfrom" "$trto"`
AC_MSG_CHECKING(self-sufficiency of $1)
AC_TRY_LINK([
#include <$1>
$2
], ,AC_MSG_RESULT(yes) , [
AC_DEFINE_UNQUOTED($one_norm_NEEDS_HELP)
AC_MSG_RESULT(no)
])
])

dnl ***************************************************************************
dnl	MOO_CONST
dnl Check whether or not the C compiler can cope with simple uses of the ANSI C
dnl `const' keyword, defining `const' as the empty string if it can't.
dnl This is a weaker requirement than the standard test in autoconf 2.
AC_DEFUN(MOO_CONST, [
echo checking for a working const keyword
cat << EOF > conftest.c
#ifdef __GNUC__
#define const __const__
#endif
int foo(const char *x) { return 17 + (x - x); }
int bar() { int x = foo("foo"); return x; }
EOF
if $CC -c conftest.c > conftest.out 2>&1; then
  if test -s conftest.out; then
    AC_DEFINE(const,)
  fi
else
  AC_DEFINE(const,)
fi
rm -f conftest*
])

dnl ***************************************************************************
dnl	MOO_ANSI_C
dnl Check whether or not the C compiler handles ANSI C (i.e., allows function
dnl prototypes and the `void *' type) and try to make it do so by adding 
dnl command-line options like -Aa and -Xa, which some compilers require.  If
dnl nothing works, abort configuration.
AC_DEFUN(MOO_ANSI_C, [
echo "checking that the C compiler handles important ANSI C constructs"
for opt in "" -Aa -Xa -ansi
do
SAVECC="$CC"
CC="$CC $opt"
AC_TRY_RUN([
int main(int argc, char *argv) { void *ptr; exit(0); }
],
[have_ansi=1
break],
[CC="$SAVECC"])
done
if test -z "$have_ansi"; then
echo ""
echo "*** Sorry, but I can't figure out how to find an ANSI C compiler here."
echo "*** Compiling this program requires such a compiler."
exit 1
fi
])

dnl ***************************************************************************
dnl Start of actual configuration tests
dnl ***************************************************************************

AC_PROG_CC
AC_PROG_GCC_TRADITIONAL
test -n "$GCC" && CC="$CC"
AC_PROG_CPP
AC_ISC_POSIX
MOO_ANSI_C
MOO_CONST
CPP=""
AC_PROG_CPP
AC_CHECK_PROGS(YACC, [byacc yacc 'bison -y'])
if test -z "$YACC"; then
echo "*** I couldn't find a suitable yacc program in your path."
echo "*** This codebase cannot be compiled without one."
exit 1
fi
AC_CHECK_PROG(GPERF, [gperf], [gperf])
if test -z "$GPERF"; then
echo "*** I couldn't find a gperf binary in your path."
echo "*** gperf is required by this codebase."
exit 1
fi
MOO_MALLOC_H
AC_CHECK_HEADER(sys/bsdtypes.h, AC_DEFINE(NEED_BSDTYPES_H))
AC_CHECK_HEADER(sys/select.h, AC_DEFINE(NEED_SELECT_H))
AC_CHECK_HEADER(sys/filio.h, AC_DEFINE(HAVE_FILIO_H))
AC_CHECK_HEADER(memory.h, AC_DEFINE(NEED_MEMORY_H))
AC_TYPE_PID_T
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_STRUCT_TIMEZONE

AC_SEARCH_LIBS(sqrt, m, AC_DEFINE(HAVE_SQRT))
AC_SEARCH_LIBS(waitpid, posix, AC_DEFINE(HAVE_WAITPID))
AC_SEARCH_LIBS(sigemptyset, posix, AC_DEFINE(HAVE_SIGEMPTYSET))
AC_SEARCH_LIBS(accept, socket inet, AC_DEFINE(HAVE_ACCEPT))
AC_SEARCH_LIBS(t_open, nsl nsl_s, AC_DEFINE(HAVE_T_OPEN))
AC_SEARCH_LIBS(crypt, crypt cryptd, AC_DEFINE(HAVE_CRYPT))

AC_CHECK_HEADERS(unistd.h sys/cdefs.h stdlib.h tiuser.h machine/endian.h locale.h)
AC_CHECK_FUNCS(remove rename select strerror strftime strtoul matherr)
AC_CHECK_FUNCS(random lrand48 wait3 wait2 sigsetmask sigprocmask sigrelse)

MOO_NDECL_FUNCS(ctype.h, tolower)
MOO_NDECL_FUNCS(fcntl.h, fcntl)
MOO_NDECL_FUNCS(netinet/in.h, htonl)
MOO_NDECL_FUNCS(sys/ioctl.h, ioctl)
MOO_NDECL_FUNCS(signal.h, kill sigemptyset sigprocmask sigrelse)
MOO_NDECL_FUNCS(sys/socket.h, accept bind shutdown)
MOO_NDECL_FUNCS(sys/stat.h, fstat)
MOO_NDECL_FUNCS(stdio.h, fclose perror remove vfprintf)
MOO_NDECL_FUNCS(stdlib.h, random srandom strtod strtol strtoul)
dnl *** if we need memory.h, behave differently
if test "$ac_cv_header_memory_h" != yes; then
MOO_NDECL_FUNCS(string.h, bzero memcpy memset strerror)
else
dnl *** BZERO
MOO_FUNC_DECL_CHECK(string.h, bzero, , [
MOO_FUNC_DECL_CHECK(memory.h, bzero, ,
AC_DEFINE(NDECL_FUNC_BZERO)
)])
dnl *** MEMCPY
MOO_FUNC_DECL_CHECK(string.h, memcpy, , [
MOO_FUNC_DECL_CHECK(memory.h, memcpy, ,
AC_DEFINE(NDECL_FUNC_MEMCPY)
)])
dnl *** MEMSET
MOO_FUNC_DECL_CHECK(string.h, memset, , [
MOO_FUNC_DECL_CHECK(memory.h, memset, ,
AC_DEFINE(NDECL_FUNC_MEMSET)
)])
dnl *** STRERROR
MOO_FUNC_DECL_CHECK(string.h, strerror, , [
MOO_FUNC_DECL_CHECK(memory.h, strerror, ,
AC_DEFINE(NDECL_FUNC_STRERROR)
)])
fi
MOO_FUNC_DECL_CHECK(stdlib.h, bzero, AC_DEFINE(BZERO_IN_STDLIB_H))
dnl *** behave differently if we need sys/select.h
if test "$ac_cv_header_sys_select_h" != yes; then
MOO_NDECL_FUNCS(sys/time.h, getitimer setitimer select)
else
dnl *** GETITIMER
MOO_FUNC_DECL_CHECK(sys/time.h, getitimer, , [
MOO_FUNC_DECL_CHECK(sys/types.h, getitimer, , [
MOO_FUNC_DECL_CHECK(sys/select.h, getitimer, ,
AC_DEFINE(NDECL_FUNC_GETITIMER)
)])])
dnl *** SETITIMER
MOO_FUNC_DECL_CHECK(sys/time.h, setitimer, , [
MOO_FUNC_DECL_CHECK(sys/types.h, setitimer, , [
MOO_FUNC_DECL_CHECK(sys/select.h, setitimer, ,
AC_DEFINE(NDECL_FUNC_SETITIMER)
)])])
dnl *** SELECT
MOO_FUNC_DECL_CHECK(sys/time.h, select, , [
MOO_FUNC_DECL_CHECK(sys/types.h, select, , [
MOO_FUNC_DECL_CHECK(sys/select.h, select, ,
AC_DEFINE(NDECL_FUNC_SELECT)
)])])
fi

MOO_NDECL_FUNCS(time.h, strftime time)
MOO_NDECL_VARS(time.h, tzname)
MOO_NDECL_FUNCS(tiuser.h, t_open)
MOO_NDECL_VARS(tiuser.h, t_errlist)
MOO_NDECL_FUNCS(unistd.h, fork)
MOO_NDECL_FUNCS(sys/wait.h, waitpid)

MOO_HEADER_STANDS_ALONE(arpa/inet.h, [struct in_addr foo;])
MOO_HEADER_STANDS_ALONE(signal.h)
MOO_HEADER_STANDS_ALONE(sys/socket.h)
MOO_HEADER_STANDS_ALONE(sys/stat.h)
MOO_HEADER_STANDS_ALONE(time.h)

dnl ***************************************************************************
AC_MSG_CHECKING(for string.h declaring some functions incorrectly)
dnl **can't use $DEFS here any more...
dnl **use a different method to split on needing memory.h
if test "$ac_cv_header_memory_h" != yes; then
echo "
#include <sys/types.h>
#include <string.h>
" > conftest.c
else
echo "
#include <sys/types.h>
#include <string.h>
#include <memory.h>
" > conftest.c
fi
$CC conftest.c -o conftest $LIBS > conftest.out 2>&1
if grep "conflicting types for built-in function" conftest.out >/dev/null; then
  AC_DEFINE(USE_OWN_STRING_H)
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
fi
rm -f conftest*

dnl ***************************************************************************
AC_MSG_CHECKING(for compatibility between <sys/ioctl.h> and <stropts.h>)
AC_TRY_CPP([
#include <sys/ioctl.h>
#include <stropts.h>
], AC_MSG_RESULT(yes),
AC_MSG_RESULT(no)
AC_DEFINE(UNDEF_IO_IN_STROPTS_H))

AC_OUTPUT(Makefile)
