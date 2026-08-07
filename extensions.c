/* $egnet: extensions.c,v 1.32 2010/07/11 22:08:59 dive Exp $ */

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

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "bf_register.h"
#include "config.h"
#include "db_private.h"
#include "dirent.h"
#include "exceptions.h"
#include "functions.h"
#include "list.h"
#include "log.h"
#include "network.h"
#include "numbers.h"
#include "options.h"
#include "random.h"
#include "regexpr.h"
#include "server.h"
#include "storage.h"
#include "streams.h"
#include "structures.h"
#include "tasks.h"
#include "unparse.h"
#include "utils.h"
#include "version.h"

#define FILE_IO_MAX_FILES 256
#define FILE_IO_BUFFER_LENGTH 4096

#define OneK 1024
#define BUF_LEN (2*OneK)
#define MAX_INT 32760
#define TRUE    1
#define FALSE   0

extern Var      do_match(Var arglist, int reverse);

int             find_insert(Var lst, Var key);
Var             value_compare(Var a, Var b);
void            makelowercase(char *string);

int
okfile(char *path)
{
	struct stat st;
	int ret;

	ret = stat(path,&st);
	if (ret != 0) {
		if (errno == ENOENT)
			return 1;
		else
			return 0;
	}
	if ((st.st_mode & S_IFMT) != S_IFREG)
		return 0;
	else
		return 1;
}

void 
InitListToZero(Var list)
{
	int             i;
	Var             z;

	z.type = TYPE_INT;
	z.v.num = 0;

	for (i = 1; i <= list.v.list[0].v.num; i++)
		list.v.list[i] = z;
}

static          package
bf_make(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             ret, elt;
	int             n = arglist.v.list[1].v.num, i;

	if (n < 0) {
		free_var(arglist);
		return make_error_pack(E_INVARG);
	} else if (n >= INT_MAX) {
		free_var(arglist);
		return make_error_pack(E_INVARG);
	}
	ret = new_list(n);
	InitListToZero(ret);

	if (arglist.v.list[0].v.num == 2) {
		elt = var_dup(arglist.v.list[2]);
	} else {
		elt.type = TYPE_INT;
		elt.v.num = 0;
	}

	for (i = 1; i <= n; i++)
		ret.v.list[i] = var_dup(elt);

	free_var(elt);
	free_var(arglist);
	return make_var_pack(ret);
}

static          package
bf_slice(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             ret, list = arglist.v.list[1];
	int             n = list.v.list[0].v.num, c, i;

	if (n < 0) {
		free_var(arglist);
		return make_error_pack(E_INVARG);
	}
	ret = new_list(n);
	InitListToZero(ret);

	if (arglist.v.list[0].v.num == 2)
		c = arglist.v.list[2].v.num;
	else
		c = 1;

	for (i = 1; i <= n; i++)
		if (list.v.list[i].type != TYPE_LIST || list.v.list[i].v.list[0].v.num < c) {
			free_var(arglist);
			return make_error_pack(E_INVARG);
		} else {
			ret.v.list[i] = var_dup(list.v.list[i].v.list[c]);
		}

	free_var(arglist);
	return make_var_pack(ret);
}

/* Remove_Duplicates - from Access_Denied@LambdaMOO. */
static          package
bf_remove_duplicates(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             r;
	int             i;

	r = new_list(0);
	for (i = 1; i <= arglist.v.list[1].v.list[0].v.num; i++)
		r = setadd(r, var_ref(arglist.v.list[1].v.list[i]));

	free_var(arglist);
	return make_var_pack(r);
}
/* End Remove_Duplicates */

Var
list_assoc(Var vtarget, Var vlist, int vindex)
{
	int             i;

	for (i = 1; i <= vlist.v.list[0].v.num; i++) {
		if (vlist.v.list[i].type == TYPE_LIST &&
		    vlist.v.list[i].v.list[0].v.num >= vindex &&
		    equality(vlist.v.list[i].v.list[vindex], vtarget, 0)) {

			return var_dup(vlist.v.list[i]);
		}
	}
	return new_list(0);
}

int
list_iassoc(Var vtarget, Var vlist, int vindex)
{
	int             i;

	for (i = 1; i <= vlist.v.list[0].v.num; i++) {
		if (vlist.v.list[i].type == TYPE_LIST &&
		    vlist.v.list[i].v.list[0].v.num >= vindex &&
		    equality(vlist.v.list[i].v.list[vindex], vtarget, 0)) {

			return i;
		}
	}
	return 0;
}


static          package
bf_iassoc(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{				/* (ANY, LIST[, INT]) */
	Var             r;
	int             index = 1;

	r.type = TYPE_INT;
	if (arglist.v.list[0].v.num == 3)
		index = arglist.v.list[3].v.num;

	if (index < 1) {
		free_var(arglist);
		return make_error_pack(E_RANGE);
	}
	r.v.num = list_iassoc(arglist.v.list[1], arglist.v.list[2], index);

	free_var(arglist);
	return make_var_pack(r);
}				/* end bf_listiassoc() */

static          package
bf_assoc(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{				/* (ANY, LIST[, INT]) */
	Var             r;
	int             index = 1;

	if (arglist.v.list[0].v.num == 3)
		index = arglist.v.list[3].v.num;

	if (index < 1) {
		free_var(arglist);
		return make_error_pack(E_RANGE);
	}
	r = list_assoc(arglist.v.list[1], arglist.v.list[2], index);

	free_var(arglist);
	return make_var_pack(r);
}				/* end bf_listassoc() */

static          package
bf_sort(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	/*
	 * sort(list) => sorts and returns list. sort({1,3,2}) => {1,2,3}
	 */

	/* returns E_TYPE is list is not all the same type */

	Var             sorted = new_list(0), tmp;
	Var             e;
	int             i, l;

	e.type = TYPE_NONE;

	for (i = 1; i <= arglist.v.list[1].v.list[0].v.num; i++) {
		e = var_ref(arglist.v.list[1].v.list[i]);
		l = find_insert(sorted, e);
		if (l == -10) {
			free_var(arglist);
			return make_error_pack(E_TYPE);
		}
		tmp = listinsert(var_ref(sorted), var_ref(e), l);
		sorted = var_ref(tmp);
	}

	free_var(arglist);
	return make_var_pack(sorted);
}

int 
find_insert(Var lst, Var key)
{
	/*
	 * find_insert(sortedlist,key) => index of first element in
	 * sortedlist > key.  sortedlist is assumed to bem sorted in
	 * increasing order and the number returned is anywhere from 1 to
	 * length(sortedlist)+1, inclusive.
	 */

	/* returns -10 if an E_TYPE occurs */

	Var             compare;
	int             r = lst.v.list[0].v.num, l = 1, i;

	while (r >= l) {
		compare = value_compare(var_ref(key), var_ref(lst.v.list[i = ((r + l) / 2)]));
		if (compare.type == TYPE_ERR) {
			free_var(compare);
			return -10;
		}
		if (compare.v.num < 0) {
			r = i - 1;
		} else {
			l = i + 1;
		}
	}
	return l;
}

Var 
value_compare(Var a, Var b)
{
	char           *sa = 0, *sb = 0;
	Var             r;

	if (a.type != b.type) {
		r.type = TYPE_ERR;
		r.v.err = E_TYPE;
		return r;
	}
	switch ((int)a.type) {
	case TYPE_STR:
		sa = str_dup(a.v.str);
		sb = str_dup(b.v.str);
		makelowercase(sa);
		makelowercase(sb);
		r.v.num = strcmp(sa, sb);
		r.type = TYPE_INT;
		break;
	case TYPE_INT:
	case TYPE_FLOAT:
		r = compare_numbers(a, b);
		break;
	case TYPE_OBJ:
	case TYPE_ERR:
		a.type = b.type = TYPE_INT;
		r = compare_numbers(a, b);
		break;
	default:
		r.v.err = E_TYPE;
		r.type = TYPE_ERR;
	}

	return r;
}

void 
makelowercase(char *string)
{
	int             i = 0;
	for (; string[i]; i++)
		if (string[i] > 64 && string[i] < 91)
			string[i] = string[i] + 32;
}

/*
 * clock(): return a list, members are: {user seconds, user microseconds,
 * system seconds, system microseconds}
 */
static          package
bf_clock(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{

	static struct rusage ru;
	Var             r;

	getrusage(RUSAGE_SELF, &ru);
	r = new_list(4);
	r.v.list[1].type = TYPE_INT;
	r.v.list[1].v.num = ru.ru_utime.tv_sec;
	r.v.list[2].type = TYPE_INT;
	r.v.list[2].v.num = ru.ru_utime.tv_usec;
	r.v.list[3].type = TYPE_INT;
	r.v.list[3].v.num = ru.ru_stime.tv_sec;
	r.v.list[4].type = TYPE_INT;
	r.v.list[4].v.num = ru.ru_stime.tv_usec;
	free_var(arglist);
	return make_var_pack(r);
}


static          package
bf_gettimeofday(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	/** gettimeofday()
            return {tv_sec, tv_usec}
        */

	static struct timeval tv;
	static struct timezone tz = {0, 0};
	Var             r;

	gettimeofday(&tv, &tz);
	r = new_list(2);
	r.v.list[1].type = TYPE_INT;
	r.v.list[1].v.num = tv.tv_sec;
	r.v.list[2].type = TYPE_INT;
	r.v.list[2].v.num = tv.tv_usec;

	free_var(arglist);
	return make_var_pack(r);
}


static          package
bf_find_verb(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{

	/** find_verb(OBJ where, STR verbspec[, ANY executable])
            Return {OBJ definer, INT index} if verb exists[ and is executable].
            Raise E_VERBNF if not.
            Raise E_INVARG if where object is invalid.
        */

	Objid           oid = arglist.v.list[1].v.obj, foid;
	const char     *vname = arglist.v.list[2].v.str;
	int             check_x_bit = (arglist.v.list[0].v.num >= 3) && is_true(arglist.v.list[3]);
	Var             r;
	Object         *o;
	Verbdef        *v;
	int             i, found = 0;

	if (!valid(oid)) {
		free_var(arglist);
		return make_error_pack(E_INVARG);
	}
	for (o = dbpriv_find_object(foid = oid); o; o = dbpriv_find_object(foid = o->parent)) {
		for (i = 1, v = o->verbdefs; v; v = v->next, i++)
			if (verbcasecmp(v->name, vname) && (!check_x_bit || (v->perms & VF_EXEC))) {
				found = i;
				goto bf_find_verb_done;
			}
	}

bf_find_verb_done:

	free_var(arglist);

	if (!found)
		return make_error_pack(E_VERBNF);

	r = new_list(2);

	r.v.list[1].type = TYPE_OBJ;
	r.v.list[1].v.obj = foid;

	r.v.list[2].type = TYPE_INT;
	r.v.list[2].v.num = found;

	return make_var_pack(r);
}

/** NOTE:

    I didn't write the following two builtins.  I don't know their original
    origin, but they basically just return the results of functions already
    extant in the server. I changed the registered names (used in MOO) to
    suit my own anal tastes.

    -Quinn <quinn@netsville.com>
*/

static          package
bf_verbname_match(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	/** match_verbname(STR pattern, STR string)
            True if string would match a MOO verb with name 'pattern'.
         */

	Var             ret;

	ret.type = TYPE_INT;
	ret.v.num = verbcasecmp(arglist.v.list[1].v.str, arglist.v.list[2].v.str);
	free_var(arglist);
	return make_var_pack(ret);
}

static          package
bf_strhash(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	/** string_hash_as_int(STR string)
            Return a (probably unique) integer describing the given string.
         */

	Var             ret;

	ret.type = TYPE_INT;
	ret.v.num = str_hash(arglist.v.list[1].v.str);
	free_var(arglist);
	return make_var_pack(ret);
}
/**
 * asc(STR char) => INT code
 * asc() returns the ASCII value of the first character
 * of its string argument.
 */
static          package
bf_asc(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             v;

	if (0 == arglist.v.list[1].v.str[0]) {
		/* empty string */
		free_var(arglist);
		return make_raise_pack(E_INVARG, "Invalid argument", zero);
	}
	v.type = TYPE_INT;
	v.v.num = (unsigned char) arglist.v.list[1].v.str[0];
	free_var(arglist);
	return make_var_pack(v);
}

/**
 * chr(INT code) => STR char
 * chr() returns a string containing one character,
 * the ASCII representation of its integer argument.
 */
static          package
bf_chr(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             v;

	if (arglist.v.list[1].v.num < 0 ||
	    arglist.v.list[1].v.num > UCHAR_MAX) {
		/* not an integer in valid ASCII range */
		free_var(arglist);
		return make_raise_pack(E_INVARG, "Invalid argument", zero);
	}
	v.type = TYPE_STR;
	v.v.str = (char *) mymalloc(2, M_STRING);
	((char *) v.v.str)[0] = arglist.v.list[1].v.num;
	((char *) v.v.str)[1] = 0;
	free_var(arglist);
	return make_var_pack(v);
}

/* Logical XOR - one and only one of args[1] and args[2] can be true. */
static          package
bf_xor(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             r;
	r.type = TYPE_INT;
	r.v.num = (is_true(arglist.v.list[1]) && (!is_true(arglist.v.list[2]))) || ((!is_true(arglist.v.list[1])) && is_true(arglist.v.list[2]));
	free_var(arglist);
	return make_var_pack(r);
}
/* End XOR */
static          package
bf_nprogs(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Objid           oid, max_oid = db_last_used_objid();
	int             nprogs = 0;
	Verbdef        *v;
	Var             r;

	free_var(arglist);

	for (oid = 0; oid <= max_oid; oid++)
		if (valid(oid))
			for (v = dbpriv_find_object(oid)->verbdefs; v; v = v->next)
				if (v->program)
					nprogs++;

	r.type = TYPE_INT;
	r.v.num = nprogs;
	return make_var_pack(r);
}
/* End nprogs */


/* Random_Of - Random item of a list. */
static          package
bf_random_of(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             r;
	int             n;

	n = arglist.v.list[1].v.list[0].v.num;

	if (!n) {
		free_var(arglist);
		return make_error_pack(E_INVARG);
	}
	n = RANDOM() % n + 1;
	r = var_ref(arglist.v.list[1].v.list[n]);

	free_var(arglist);
	return make_var_pack(r);
}
/* End Random_Of */


/* Enlist - Make args[1] into a list, if it isn't. */
static          package
bf_enlist(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Var             r;

	if (arglist.v.list[1].type != TYPE_LIST) {
		r = new_list(1);
		r.v.list[1] = var_ref(arglist.v.list[1]);
	} else {
		r = var_ref(arglist.v.list[1]);
	}
	free_var(arglist);
	return make_var_pack(r);
}


static          package
bf_isa(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	Objid           what = arglist.v.list[1].v.obj, targ = arglist.v.list[2].v.obj;
	Var             r;

	free_var(arglist);
	r.type = TYPE_INT;
	r.v.num = 0;

	while (valid(what)) {
		if (what == targ) {
			r.v.num = 1;
			return make_var_pack(r);
		}
		what = db_object_parent(what);
	}
	return make_var_pack(r);
}

typedef unsigned short umode_t;
/* your system may define o_mode_t instead -- AAB 06/03/97 */
/* typedef o_mode_t umode_t; */

/*****************************************************
 * Utility functions
 *****************************************************/

const char     *
raw_bytes_to_clean(const char *buffer, int buflen)
{
	static Stream  *s = 0;
	int             i;
	if (!s)
		s = new_stream(100);

	for (i = 0; i < buflen; i++) {
		unsigned char   c = buffer[i];

		if (isgraph(c) || c == ' ' || c == '\t')
			stream_add_char(s, c);
		/* else drop it on the floor */
	}

	return reset_stream(s);
}

const char     *
clean_to_raw_bytes(const char *buffer, int *buflen)
{
	*buflen = strlen(buffer);
	return buffer;
}


/******************************************************
 * Module-internal data structures
 *****************************************************/

/*
 *  File types are either TEXT or BINARY
 */

typedef struct file_type *file_type;

struct file_type {

	const char     *(*in_filter) (const char *data, int buflen);

	const char     *(*out_filter) (const char *data, int *buflen);

};

file_type       file_type_binary = NULL;
file_type       file_type_text = NULL;



#define FILE_O_READ       1
#define FILE_O_WRITE      2
#define FILE_O_FLUSH      4

typedef unsigned char file_mode;

typedef struct file_handle file_handle;

struct file_handle {
	char            valid;	/* Is this a valid entry?   */
	char           *name;	/* pathname of the file     */
	file_type       type;	/* text or binary, sir?     */
	file_mode       mode;	/* readin', writin' or both */

	FILE           *file;	/* the actual file handle   */
};

typedef struct line_buffer line_buffer;

struct line_buffer {
	char           *line;
	struct line_buffer *next;
};


/*
 * this is in server.c
 */

int             notify_bytes(Objid player, const char *bytes, int len);

/***************************************************************
 * Version and package informaion
 ***************************************************************/

char            file_package_name[] = "FIO";
/* version moved to version.c */

/***************************************************************
 * File <-> FHANDLE descriptor table interface
 ***************************************************************/


file_handle     file_table[FILE_IO_MAX_FILES];

char 
file_handle_valid(Var fhandle)
{
	int32           i = fhandle.v.num;
	if (fhandle.type != TYPE_INT)
		return 0;
	if ((i < 0) || (i >= FILE_IO_MAX_FILES))
		return 0;
	return file_table[i].valid;
}


FILE           *
file_handle_file(Var fhandle)
{
	int32           i = fhandle.v.num;
	return file_table[i].file;
}

const char     *
file_handle_name(Var fhandle)
{
	int32           i = fhandle.v.num;
	return file_table[i].name;
}

file_type 
file_handle_type(Var fhandle)
{
	int32           i = fhandle.v.num;
	return file_table[i].type;
}

file_mode 
file_handle_mode(Var fhandle)
{
	int32           i = fhandle.v.num;
	return file_table[i].mode;
}


void 
file_handle_destroy(Var fhandle)
{
	int32           i = fhandle.v.num;
	file_table[i].file = NULL;
	file_table[i].valid = 0;
	free_str(file_table[i].name);
}


int32 
file_allocate_next_handle(void)
{
	static int32    current_handle = 0;
	int32           wrapped = current_handle;

	if (current_handle > FILE_IO_MAX_FILES)
		wrapped = current_handle = 0;

	while (current_handle < FILE_IO_MAX_FILES) {
		if (!file_table[current_handle].valid)
			break;

		current_handle++;
		if (current_handle > FILE_IO_MAX_FILES)
			current_handle = 0;
		if (current_handle == wrapped)
			current_handle = FILE_IO_MAX_FILES;
	}
	if (current_handle == FILE_IO_MAX_FILES) {
		current_handle = 0;
		return -1;
	}
	return current_handle;
}


Var 
file_handle_new(const char *name, file_type type, file_mode mode)
{
	Var             r;
	int32           handle = file_allocate_next_handle();

	r.type = TYPE_INT;
	r.v.num = handle;

	if (handle >= 0) {
		file_table[handle].valid = 1;
		file_table[handle].name = str_dup(name);
		file_table[handle].type = type;
		file_table[handle].mode = mode;
	}
	return r;
}

void 
file_handle_set_file(Var fhandle, FILE * f)
{
	int32           i = fhandle.v.num;
	file_table[i].file = f;
}

/***************************************************************
 * Interface for modestrings
 ***************************************************************/

/*
 *  Convert modestring to settings for type and mode.
 *  Returns pointer to stdio modestring if successfull and
 *  NULL if not.
 */

const char     *
file_modestr_to_mode(const char *s, file_type * type, file_mode * mode)
{
	static char     buffer[4] = {0, 0, 0, 0};
	int             p = 0;
	file_type       t;
	file_mode       m = 0;

	if (!file_type_binary) {
		file_type_binary = mymalloc(sizeof(struct file_type), M_STRING);
		file_type_text = mymalloc(sizeof(struct file_type), M_STRING);
		file_type_binary->in_filter = raw_bytes_to_binary;
		file_type_binary->out_filter = binary_to_raw_bytes;
		file_type_text->in_filter = raw_bytes_to_clean;
		file_type_text->out_filter = clean_to_raw_bytes;
	}
	if (strlen(s) != 4)
		return 0;

	if (s[0] == 'r')
		m |= FILE_O_READ;
	else if (s[0] == 'w')
		m |= FILE_O_WRITE;
	else if (s[0] == 'a')
		m |= FILE_O_WRITE;
	else
		return NULL;


	buffer[p++] = s[0];

	if (s[1] == '+') {
		m |= (s[0] == 'r') ? FILE_O_WRITE : FILE_O_READ;
		buffer[p++] = '+';
	} else if (s[1] != '-') {
		return NULL;
	}
	if (s[2] == 't')
		t = file_type_text;
	else if (s[2] == 'b') {
		t = file_type_binary;
		buffer[p++] = 'b';
	} else
		return NULL;

	if (s[3] == 'f')
		m |= FILE_O_FLUSH;
	else if (s[3] != 'n')
		return NULL;

	*type = t;
	*mode = m;
	buffer[p] = 0;
	return buffer;
}


/***************************************************************
 * Various error handlers
 ***************************************************************/

package
file_make_error(const char *errtype, const char *msg)
{
	package         p;
	Var             value;

	value.type = TYPE_STR;
	value.v.str = str_dup(errtype);

	p.kind = BI_RAISE;
	p.u.raise.code.type = TYPE_STR;
	p.u.raise.code.v.str = str_dup("E_FILE");
	p.u.raise.msg = str_dup(msg);
	p.u.raise.value = value;

	return p;
}

package 
file_raise_errno(const char *value_str)
{
	char           *strerr;

	if (errno) {
		strerr = strerror(errno);
		return file_make_error(value_str, strerr);
	} else {
		return file_make_error("EOF", "EOF");
	}

}

package 
file_raise_notokcall([[maybe_unused]] const char *funcid, [[maybe_unused]] Objid progr)
{
	return make_error_pack(E_PERM);
}

package 
file_raise_notokfilename([[maybe_unused]] const char *funcid, const char *pathname)
{
	Var             p;

	p.type = TYPE_STR;
	p.v.str = str_dup(pathname);
	return make_raise_pack(E_INVARG, "Invalid pathname", p);
}

/***************************************************************
 * Security verification
 ***************************************************************/

int 
file_verify_caller(Objid progr)
{
	return is_wizard(progr);
}

int 
file_verify_path(const char *pathname)
{
	/*
	      *  A pathname is OK does not contain a
         *  any of instances the substring "/."
         */

	if (pathname[0] == '\0')
		return 1;

	if ((strlen(pathname) > 1) && (pathname[0] == '.') && (pathname[1] == '.'))
		return 0;

	if (strindex(pathname, "/.", 0))
		return 0;

	return 1;
}

/***************************************************************
 * Common code for FHANDLE-using functions
 **************************************************************/

FILE           *
file_handle_file_safe(Var handle)
{
	if (!file_handle_valid(handle))
		return NULL;
	else
		return file_handle_file(handle);
}

const char     *
file_handle_name_safe(Var handle)
{
	if (!file_handle_valid(handle))
		return NULL;
	else
		return file_handle_name(handle);
}

/***************************************************************
 * Common code for file opening functions
 ***************************************************************/

const char     *
file_resolve_path(const char *pathname)
{
	static Stream  *s = 0;

	if (!s)
		s = new_stream(strlen(pathname) + strlen(FIO_SUBDIR) + 1);

	if (!file_verify_path(pathname))
		return NULL;

	stream_add_string(s, FIO_SUBDIR);
	if (pathname[0] == '/')
		stream_add_string(s, pathname + 1);
	else
		stream_add_string(s, pathname);

	return reset_stream(s);

}

/***************************************************************
 * Built in functions
 * file_version
 ***************************************************************/

static          package
bf_file_version([[maybe_unused]] Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
	char            tmpbuffer[50];
	Var             rv;

	sprintf(tmpbuffer, "%s/%s", file_package_name, file_package_version);

	rv.type = TYPE_STR;
	rv.v.str = str_dup(tmpbuffer);

	return make_var_pack(rv);

}


/***************************************************************
 * File open and close.
 ***************************************************************/


/*
 * FHANDLE file_open(STR name, STR mode)
 */

static          package
bf_file_open(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle;
	const char     *real_filename;
	const char     *filename = arglist.v.list[1].v.str;
	const char     *mode = arglist.v.list[2].v.str;
	const char     *fmode;
	file_mode       rmode;
	file_type       type;
	FILE           *f;

	if (!file_verify_caller(progr))
		r = file_raise_notokcall("file_open", progr);
	else if ((real_filename = file_resolve_path(filename)) == NULL)
		r = file_raise_notokfilename("file_open", filename);
	else if ((fmode = file_modestr_to_mode(mode, &type, &rmode)) == NULL)
		r = make_raise_pack(E_INVARG, "Invalid mode string", var_ref(arglist.v.list[2]));
	else if ((fhandle = file_handle_new(filename, type, rmode)).v.num < 0)
		r = make_raise_pack(E_QUOTA, "Too many files open", zero);
	else if ((f = fopen(real_filename, fmode)) == NULL) {
		file_handle_destroy(fhandle);
		r = file_raise_errno("file_open");
	} else {
		/* phew, we actually got a successfull open */
		file_handle_set_file(fhandle, f);
		r = make_var_pack(fhandle);
	}
	free_var(arglist);
	return r;
}

/*
 * void file_close(FHANDLE handle);
 */

static          package
bf_file_close(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	FILE           *f;

	if (!file_verify_caller(progr))
		r = file_raise_notokcall("file_close", progr);
	else if ((f = file_handle_file_safe(fhandle)) == NULL)
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	else {
		fclose(f);
		file_handle_destroy(fhandle);
		r = no_var_pack();
	}
	free_var(arglist);
	return r;
}

/*
 * STR file_name(FHANDLE handle)
 */

static          package
bf_file_name(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	const char     *name;
	Var             rv;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_name", progr);
	} else if ((name = file_handle_name_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else {
		rv.type = TYPE_STR;
		rv.v.str = str_dup(name);
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

static          package
bf_file_openmode(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	char            buffer[5] = {0, 0, 0, 0, 0};
	file_mode       mode;
	file_type       type;
	Var             rv;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_name", progr);
	} else if (!file_handle_valid(fhandle)) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else {
		type = file_handle_type(fhandle);
		mode = file_handle_mode(fhandle);
		if (mode & FILE_O_READ) {
			buffer[0] = 'r';
		} else if (mode & FILE_O_WRITE) {
			buffer[0] = 'w';
		}
		if (mode & (FILE_O_READ | FILE_O_WRITE))
			buffer[1] = '+';
		else
			buffer[1] = '-';

		if (type == file_type_binary)
			buffer[2] = 'b';
		else
			buffer[2] = 't';

		if (mode & FILE_O_FLUSH)
			buffer[3] = 'f';
		else
			buffer[3] = 'n';


		rv.type = TYPE_STR;
		rv.v.str = str_dup(buffer);
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}



/**********************************************************
 * string (line-based) i/o
 **********************************************************/

/*
 * common functionality of file_readline and file_readlines
 */

static const char *
file_read_line(Var fhandle, int *count)
{
	static Stream  *str = 0;
	const char     *rv;
	char            buffer[FILE_IO_BUFFER_LENGTH];
	int             len = 0, total_len = 0, used_stream = 0;
	FILE           *f;

	f = file_handle_file(fhandle);

	if (str == 0)
		str = new_stream(FILE_IO_BUFFER_LENGTH);

try_again:

	if (fgets(buffer, sizeof(buffer), f) == NULL) {

		/*
		 * Yes, this means it's an error to read an incomplete line
		 * at the end of a file.  No magic vanishing \n's. Since it's
		 * not possible to WRITEline a line with no \n it shouldn't
		 * be possible read one.
		 */

		if (used_stream)
			reset_stream(str);
		rv = NULL;
	} else {
		len = strlen(buffer);

		total_len += len;

		if (len == sizeof(buffer) - 1 && buffer[len - 1] != '\n') {
			used_stream = 1;
			stream_add_string(str, buffer);
			goto try_again;
		}
		if (buffer[len - 1] == '\n')
			buffer[len - 1] = '\0';

		if (used_stream) {
			stream_add_string(str, buffer);
			rv = reset_stream(str);
		} else {
            stream_add_string(str, buffer); /* not 100% this is right -dive@20260730 */
            rv = reset_stream(str);
		}
	}
	*count = total_len - 1;
	return rv;
}


/*
 * STR file_readline(FHANDLE handle)
 */

static          package
bf_file_readline(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	Var             rv;
	int             len;
	file_mode       mode;
	file_type       type;
	const char     *line;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_readline", progr);
	} else if (!file_handle_valid(fhandle)) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else if (!(mode = file_handle_mode(fhandle)) & FILE_O_READ)
		r = make_raise_pack(E_INVARG, "File is open write-only", fhandle);
	else {
		type = file_handle_type(fhandle);
		if ((line = file_read_line(fhandle, &len)) == NULL)
			r = file_raise_errno("readline");
		else {
			rv.type = TYPE_STR;
			rv.v.str = str_dup((type->in_filter) (line, len));
			r = make_var_pack(rv);
		}
	}
	free_var(arglist);
	return r;
}

/*
 * STR file_readlines(FHANDLE handle, INT start, INT end)
 */

void 
free_line_buffer(line_buffer * head, int strings_too)
{
	line_buffer    *next;
	if (head) {
		next = head->next;
		free(head);
		head = next;
		while (head != NULL) {
			next = head->next;
			if (strings_too)
				free_str(head->line);
			myfree(head, M_STRING);
			head = next;
		}
	}
}

line_buffer    *
new_line_buffer(char *line)
{
	line_buffer    *p = mymalloc(sizeof(line_buffer), M_STRING);
	p->line = line;
	p->next = NULL;
	return p;
}

static          package
bf_file_readlines(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	int32           begin = arglist.v.list[2].v.num;
	int32           end = arglist.v.list[3].v.num;
	int32           begin_loc = 0, linecount = 0;
	file_type       type;
	file_mode       mode;
	Var             rv;
	int             current_line = 0, len = 0, i = 0;
	const char     *line = NULL;
	FILE           *f;
	line_buffer    *linebuf_head = NULL, *linebuf_cur = NULL;


	if ((begin < 1) || (begin > end))
		return make_error_pack(E_INVARG);
	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_readlines", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else if (!(mode = file_handle_mode(fhandle)) & FILE_O_READ)
		r = make_raise_pack(E_INVARG, "File is open write-only", fhandle);
	else {

		/* Back to the beginning ... */
		rewind(f);

		/* "seek" to that line */
		begin--;
		while ((current_line != begin)
		       && ((line = file_read_line(fhandle, &len)) != NULL))
			current_line++;

		if (((begin != 0) && (line == NULL)) || ((begin_loc = ftell(f)) == -1))
			r = file_raise_errno("read_line");
		else {
			type = file_handle_type(fhandle);

			/*
			 * now that we have where to begin, it's time to
			 * slurp lines and seek to EOF or to the end_line,
			 * whichever comes first
			 */

			linebuf_head = linebuf_cur = new_line_buffer(NULL);

			while ((current_line != end)
			       && ((line = file_read_line(fhandle, &len)) != NULL)) {
				linebuf_cur->next = new_line_buffer(str_dup((type->in_filter) (line, len)));
				linebuf_cur = linebuf_cur->next;

				current_line++;
			}
			linecount = current_line - begin;

			linebuf_cur = linebuf_head->next;

			if (fseek(f, begin_loc, SEEK_SET) == -1) {
				free_line_buffer(linebuf_head, 1);
				r = file_raise_errno("seeking");
			} else {
				rv = new_list(linecount);
				i = 1;
				while (linebuf_cur != NULL) {
					rv.v.list[i].type = TYPE_STR;
					rv.v.list[i].v.str = linebuf_cur->line;
					linebuf_cur = linebuf_cur->next;
					i++;
				}
				free_line_buffer(linebuf_head, 0);
				r = make_var_pack(rv);
			}
		}
	}

	free_var(arglist);
	return r;
}

/*
 * void file_writeline(FHANDLE handle, STR line)
 */

static          package
bf_file_writeline(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	const char     *buffer = arglist.v.list[2].v.str;
	int             len;
	file_mode       mode;
	file_type       type;
	FILE           *f;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_writeline", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else if (!((mode = file_handle_mode(fhandle)) & FILE_O_WRITE))
		r = make_raise_pack(E_INVARG, "File is open read-only", fhandle);
	else {
		type = file_handle_type(fhandle);
		if ((fputs((type->out_filter) (buffer, &len), f) == EOF) || (fputc('\n', f) != '\n'))
			r = file_raise_errno(file_handle_name(fhandle));
		else {
			if (mode & FILE_O_FLUSH) {
				/* printf("flushing...\n"); */
				fflush(f);
			}
			r = no_var_pack();
		}
	}
	free_var(arglist);
	return r;
}

/********************************************************
 * For sending I/O directly to connections
 ********************************************************

static package
bf_file_send(Var arglist, Byte next, void *vdata, Objid progr)
{
  package r;
  Objid victim = arglist.v.list[1].v.obj;
  const char *filename = arglist.v.list[2].v.str;
  Var rv;
  const char *real_filename;
  char buffer[FILE_IO_BUFFER_LENGTH];
  FILE *f;
  int read = 0, total_sent = 0;


  if(!file_verify_caller(progr)) {
	 r = file_raise_notokcall("file_send", progr);
  } else if((real_filename = file_resolve_path(filename)) == NULL) {
	 r =  file_raise_notokfilename("file_send", filename);
  } else {
	 if((f = fopen(real_filename, "rb")) == NULL) {
		return file_raise_errno("file_send");
	 } else {
		while((read = fread(buffer, sizeof(char), sizeof(buffer), f))) {
		  total_sent += read;
		  notify_bytes(victim, buffer, read);
		}
		fclose(f);
	 }
  }
  rv.type = TYPE_INT;
  rv.v.num = total_sent;
  return make_var_pack(rv);
}

* removed due to the server.c hack and not being really necessary
********************************************************/

/********************************************************
 * binary i/o
 ********************************************************/

/*
 * STR file_read(FHANDLE handle, INT record_length)
 */

static          package
bf_file_read(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;

	Var             fhandle = arglist.v.list[1];
	file_mode       mode;
	file_type       type;
	int32           record_length = arglist.v.list[2].v.num;
	int32           read_length;

	char            buffer[FILE_IO_BUFFER_LENGTH];

	Var             rv;

	static Stream  *str = 0;
	int             len = 0, read = 0;

	FILE           *f;

	read_length = ((size_t)record_length > sizeof(buffer)) ? (int32)sizeof(buffer) : record_length;

	if (str == 0)
		str = new_stream(FILE_IO_BUFFER_LENGTH);

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_read", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else if (!(mode = file_handle_mode(fhandle)) & FILE_O_READ)
		r = make_raise_pack(E_INVARG, "File is open write-only", fhandle);
	else {
		type = file_handle_type(fhandle);

try_again:
		read = fread(buffer, sizeof(char), read_length, f);
		if (!read && !len) {
			/*
			 * No more to read.  This is only an error if nothing
			 * has been read so far.
			 * 
			 */
			r = file_raise_errno(file_handle_name(fhandle));
		} else if (read && ((len += read) < record_length)) {
			/*
			 * We got something this time, but it isn't enough.
			 */
			stream_add_string(str, (type->in_filter) (buffer, read));
			read = 0;
			goto try_again;
		} else {
			/*
			 * We didn't get anything last time, but we have something already
			 * OR
			 * We got everything we need.
			 */

			stream_add_string(str, (type->in_filter) (buffer, read));

			rv.type = TYPE_STR;
			rv.v.str = str_dup(reset_stream(str));

			r = make_var_pack(rv);
		}
	}
	free_var(arglist);
	return r;
}

/*
 * void file_flush(FHANDLE handle)
 */

static          package
bf_file_flush(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	FILE           *f;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_flush", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else {
		if (fflush(f))
			r = file_raise_errno("flushing");
		else
			r = no_var_pack();
	}
	free_var(arglist);
	return r;
}


/*
 * INT file_write(FHANDLE fh, STR data)
 */

static          package
bf_file_write(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1], rv;
	const char     *buffer = arglist.v.list[2].v.str;
	const char     *rawbuffer;
	file_mode       mode;
	file_type       type;
	int             len;
	int             written;
	FILE           *f;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_write", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", fhandle);
	} else if (!((mode = file_handle_mode(fhandle)) & FILE_O_WRITE))
		r = make_raise_pack(E_INVARG, "File is open read-only", fhandle);
	else {
		type = file_handle_type(fhandle);
		rawbuffer = (type->out_filter) (buffer, &len);
		written = fwrite(rawbuffer, sizeof(char), len, f);
		if (!written)
			r = file_raise_errno(file_handle_name(fhandle));
		else {
			if (mode & FILE_O_FLUSH)
				fflush(f);
			rv.type = TYPE_INT;
			rv.v.num = written;
			r = make_var_pack(rv);
		}
	}
	free_var(arglist);
	return r;
}


/************************************************
 * navigating the file
 ************************************************/

/*
 * void file_seek(FHANDLE handle, FLOC location, STR whence)
 * whence in {"SEEK_SET", "SEEK_CUR", "SEEK_END"}
 */

static          package
bf_file_seek(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	int32           seek_to = arglist.v.list[2].v.num;
	const char     *whence = arglist.v.list[3].v.str;
	int             whnce = 0, whence_ok = 1;
	FILE           *f;

	if (!mystrcasecmp(whence, "SEEK_SET"))
		whnce = SEEK_SET;
	else if (!mystrcasecmp(whence, "SEEK_CUR"))
		whnce = SEEK_CUR;
	else if (!mystrcasecmp(whence, "SEEK_END"))
		whnce = SEEK_END;
	else
		whence_ok = 0;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_seek", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", var_ref(fhandle));
	} else if (!whence_ok) {
		r = make_raise_pack(E_INVARG, "Invalid whence", zero);
	} else {
		if (fseek(f, seek_to, whnce))
			r = file_raise_errno(file_handle_name(fhandle));
		else
			r = no_var_pack();
	}
	free_var(arglist);
	return r;
}

/*
 * FLOC file_tell(FHANDLE handle)
 */

static          package
bf_file_tell(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	Var             rv;
	FILE           *f;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_tell", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", var_ref(fhandle));
	} else {
		rv.type = TYPE_INT;
		if ((rv.v.num = ftell(f)) < 0)
			r = file_raise_errno(file_handle_name(fhandle));
		else
			r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*
 * INT file_eof(FHANDLE handle)
 */

static          package
bf_file_eof(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             fhandle = arglist.v.list[1];
	Var             rv;
	FILE           *f;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_eof", progr);
	} else if ((f = file_handle_file_safe(fhandle)) == NULL) {
		r = make_raise_pack(E_INVARG, "Invalid FHANDLE", var_ref(fhandle));
	} else {
		rv.type = TYPE_INT;
		rv.v.num = feof(f);
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*****************************************************************
 * Functions that stat()
 *****************************************************************/

/*
 * (internal) int(statok) file_stat(Var filespec, package *r, struct stat *buf)
 */

int 
file_stat(Objid progr, Var filespec, package * r, struct stat * buf)
{
	int             statok = 0;

	if (!file_verify_caller(progr)) {
		*r = file_raise_notokcall("file_stat", progr);
	} else if (filespec.type == TYPE_STR) {
		const char     *filename = filespec.v.str;
		const char     *real_filename;

		if ((real_filename = file_resolve_path(filename)) == NULL) {
			*r = file_raise_notokfilename("file_stat", filename);
		} else {
			if (stat(real_filename, buf) != 0)
				*r = file_raise_errno(filename);
			else {
				statok = 1;
			}
		}
	} else {
		FILE           *f;
		if ((f = file_handle_file_safe(filespec)) == NULL)
			*r = make_raise_pack(E_INVARG, "Invalid FHANDLE", filespec);
		else {
			if (fstat(fileno(f), buf) != 0)
				*r = file_raise_errno(file_handle_name(filespec));
			else {
				statok = 1;
			}
		}
	}
	return statok;
}

const char     *
file_type_string(umode_t st_mode)
{
	if (S_ISREG(st_mode))
		return "reg";
	else if (S_ISDIR(st_mode))
		return "dir";
	else if (S_ISFIFO(st_mode))
		return "fifo";
	else if (S_ISBLK(st_mode))
		return "block";
	else if (S_ISSOCK(st_mode))
		return "socket";
	else
		return "unknown";
}

const char     *
file_mode_string(umode_t st_mode)
{
	static Stream  *s = 0;
	if (!s)
		s = new_stream(4);
	stream_printf(s, "%03o", st_mode & 0777);
	return reset_stream(s);
}

/*
 * INT file_size(STR filename)
 * INT file_size(FHANDLE fh)
 */

static          package
bf_file_size(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             rv;
	Var             filespec = arglist.v.list[1];
	struct stat     buf;

	if (file_stat(progr, filespec, &r, &buf)) {
		rv.type = TYPE_INT;
		rv.v.num = buf.st_size;
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*
 * STR file_mode(STR filename)
 * STR file_mode(FHANDLE fh)
 */

static          package
bf_file_mode(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             rv;
	Var             filespec = arglist.v.list[1];
	struct stat     buf;

	if (file_stat(progr, filespec, &r, &buf)) {
		rv.type = TYPE_STR;
		rv.v.str = str_dup(file_mode_string(buf.st_mode));
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*
 * STR file_type(STR filename)
 * STR file_type(FHANDLE fh)
 */

static          package
bf_file_type(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             rv;
	Var             filespec = arglist.v.list[1];
	struct stat     buf;

	if (file_stat(progr, filespec, &r, &buf)) {
		rv.type = TYPE_STR;
		rv.v.str = str_dup(file_type_string(buf.st_mode));
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*
 * INT file_last_access(STR filename)
 * INT file_last_access(FHANDLE fh)
 */

static          package
bf_file_last_access(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             rv;
	Var             filespec = arglist.v.list[1];
	struct stat     buf;

	if (file_stat(progr, filespec, &r, &buf)) {
		rv.type = TYPE_INT;
		rv.v.num = buf.st_atime;
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*
 * INT file_last_modify(STR filename)
 * INT file_last_modify(FHANDLE fh)
 */

static          package
bf_file_last_modify(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             rv;
	Var             filespec = arglist.v.list[1];
	struct stat     buf;

	if (file_stat(progr, filespec, &r, &buf)) {
		rv.type = TYPE_INT;
		rv.v.num = buf.st_mtime;
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*
 * INT file_last_change(STR filename)
 * INT file_last_change(FHANDLE fh)
 */

static          package
bf_file_last_change(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             rv;
	Var             filespec = arglist.v.list[1];
	struct stat     buf;

	if (file_stat(progr, filespec, &r, &buf)) {
		rv.type = TYPE_INT;
		rv.v.num = buf.st_ctime;
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*
 * INT file_stat(STR filename)
 * INT file_stat(FHANDLE fh)
 */

static          package
bf_file_stat(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	Var             rv;
	Var             filespec = arglist.v.list[1];
	struct stat     buf;

	if (file_stat(progr, filespec, &r, &buf)) {
		rv = new_list(8);
		rv.v.list[1].type = TYPE_INT;
		rv.v.list[1].v.num = buf.st_size;
		rv.v.list[2].type = TYPE_STR;
		rv.v.list[2].v.str = str_dup(file_type_string(buf.st_mode));
		rv.v.list[3].type = TYPE_STR;
		rv.v.list[3].v.str = str_dup(file_mode_string(buf.st_mode));
		rv.v.list[4].type = TYPE_STR;
		rv.v.list[4].v.str = str_dup("");
		rv.v.list[5].type = TYPE_STR;
		rv.v.list[5].v.str = str_dup("");
		rv.v.list[6].type = TYPE_INT;
		rv.v.list[6].v.num = buf.st_atime;
		rv.v.list[7].type = TYPE_INT;
		rv.v.list[7].v.num = buf.st_mtime;
		rv.v.list[8].type = TYPE_INT;
		rv.v.list[8].v.num = buf.st_ctime;
		r = make_var_pack(rv);
	}
	free_var(arglist);
	return r;
}

/*****************************************************************
 * Housekeeping functions
 *****************************************************************/

/*
 * LIST file_list(STR pathname, [ANY detailed])
 */

int 
file_list_select(const struct dirent * d)
{
	const char     *name = d->d_name;
	int             l = strlen(name);
	if ((l == 1) && (name[0] == '.'))
		return 0;
	else if ((l == 2) && (name[0] == '.') && (name[1] == '.'))
		return 0;
	else
		return 1;
}

static          package
bf_file_list(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	/*
	 * modified to use opendir/readdir which is slightly more "standard"
	 * than the original scandir method.   -- AAB 06/03/97
	 */
	package         r;
	const char     *pathspec = arglist.v.list[1].v.str;
	const char     *real_pathname;
	int             detailed = (arglist.v.list[0].v.num > 1
				    ? is_true(arglist.v.list[2])
				    : 0);

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_list", progr);
	} else if ((real_pathname = file_resolve_path(pathspec)) == NULL) {
		r = file_raise_notokfilename("file_list", pathspec);
	} else {
		DIR            *curdir;
		Stream         *s = new_stream(64);
		int             failed = 0;
		struct stat     buf;
		Var             rv, detail;
		struct dirent  *curfile;

		if (!(curdir = opendir(real_pathname)))
			r = file_raise_errno(pathspec);
		else {
			rv = new_list(0);
			while ((curfile = readdir(curdir)) != 0) {
				if (strncmp(curfile->d_name, ".", 2) != 0 && strncmp(curfile->d_name, "..", 3) != 0) {
					if (detailed) {
						stream_add_string(s, real_pathname);
						stream_add_char(s, '/');
						stream_add_string(s, curfile->d_name);
						if (stat(reset_stream(s), &buf) != 0) {
							failed = 1;
							break;
						} else {
							detail = new_list(4);
							detail.v.list[1].type = TYPE_STR;
							detail.v.list[1].v.str = str_dup(curfile->d_name);
							detail.v.list[2].type = TYPE_STR;
							detail.v.list[2].v.str = str_dup(file_type_string(buf.st_mode));
							detail.v.list[3].type = TYPE_STR;
							detail.v.list[3].v.str = str_dup(file_mode_string(buf.st_mode));
							detail.v.list[4].type = TYPE_INT;
							detail.v.list[4].v.num = buf.st_size;
						}
					} else {
						detail.type = TYPE_STR;
						detail.v.str = str_dup(curfile->d_name);
					}
					rv = listappend(rv, detail);
				}
			}
			if (failed) {
				free_var(rv);
				r = file_raise_errno(pathspec);
			} else
				r = make_var_pack(rv);
			closedir(curdir);
		}
		free_stream(s);
	}
	free_var(arglist);
	return r;
}


/*
 * void file_mkdir(STR pathname)
 */

static          package
bf_file_mkdir(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	const char     *pathspec = arglist.v.list[1].v.str;
	const char     *real_pathname;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_mkdir", progr);
	} else if ((real_pathname = file_resolve_path(pathspec)) == NULL) {
		r = file_raise_notokfilename("file_mkdir", pathspec);
	} else {
		if (mkdir(real_pathname, 0777) != 0)
			r = file_raise_errno(pathspec);
		else
			r = no_var_pack();

	}
	free_var(arglist);
	return r;
}

/*
 * void file_rmdir(STR pathname)
 */

static          package
bf_file_rmdir(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	const char     *pathspec = arglist.v.list[1].v.str;
	const char     *real_pathname;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_rmdir", progr);
	} else if ((real_pathname = file_resolve_path(pathspec)) == NULL) {
		r = file_raise_notokfilename("file_rmdir", pathspec);
	} else {
		if (rmdir(real_pathname) != 0)
			r = file_raise_errno(pathspec);
		else
			r = no_var_pack();

	}
	free_var(arglist);
	return r;
}

/*
 * void file_remove(STR pathname)
 */

static          package
bf_file_remove(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	const char     *pathspec = arglist.v.list[1].v.str;
	const char     *real_pathname;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_remove", progr);
	} else if ((real_pathname = file_resolve_path(pathspec)) == NULL) {
		r = file_raise_notokfilename("file_remove", pathspec);
	} else {
		if (remove(real_pathname) != 0)
			r = file_raise_errno(pathspec);
		else
			r = no_var_pack();
	}
	free_var(arglist);
	return r;
}

/*
 * void file_rename(STR pathname)
 */

static          package
bf_file_rename(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	const char     *fromspec = arglist.v.list[1].v.str;
	const char     *tospec = arglist.v.list[2].v.str;
	char           *real_fromspec = NULL;
	const char     *real_tospec;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_rename", progr);
	} else if ((real_fromspec = str_dup(file_resolve_path(fromspec))) == NULL) {
		r = file_raise_notokfilename("file_rename", fromspec);
	} else if ((real_tospec = file_resolve_path(tospec)) == NULL) {
		r = file_raise_notokfilename("file_rename", tospec);
	} else {
		if (rename(real_fromspec, real_tospec) != 0)
			r = file_raise_errno("rename");
		else
			r = no_var_pack();
	}
	if (real_fromspec)
		free_str(real_fromspec);
	free_var(arglist);
	return r;
}



/*
 * void file_chmod(STR pathname, STR mode)
 */


int 
file_chmodstr_to_mode(const char *modespec, mode_t * newmode)
{
	mode_t          m = 0;
	int             i = 0, fct = 1;

	if (strlen(modespec) != 3)
		return 0;
	else {
		for (i = 2; i >= 0; i--) {
			char            c = modespec[i];
			if (!((c >= '0') && (c <= '7')))
				return 0;
			else {
				m += fct * (c - '0');
			}
			fct *= 8;
		}
	}
	*newmode = m;
	return 1;
}

static          package
bf_file_chmod(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, Objid progr)
{
	package         r;
	const char     *pathspec = arglist.v.list[1].v.str;
	const char     *modespec = arglist.v.list[2].v.str;
	mode_t          newmode;
	const char     *real_filename;

	if (!file_verify_caller(progr)) {
		r = file_raise_notokcall("file_chmod", progr);
	} else if (!file_chmodstr_to_mode(modespec, &newmode)) {
		r = make_raise_pack(E_INVARG, "Invalid mode string", zero);
	} else if ((real_filename = file_resolve_path(pathspec)) == NULL) {
		r = file_raise_notokfilename("file_chmod", pathspec);
	} else {
		if (chmod(real_filename, newmode) != 0)
			r = file_raise_errno("chmod");
		else
			r = no_var_pack();
	}
	free_var(arglist);
	return r;
}

int
matches(char *subject, const char *pattern)
{
    Var ans, req;
    int  result;
    
    req = new_list(2);
    req.v.list[1].type = TYPE_STR;
    req.v.list[2].type = TYPE_STR;
    req.v.list[1].v.str  = str_dup(subject);
    req.v.list[2].v.str  = str_dup(pattern);
    ans = do_match(req, 0);
    result = is_true(ans);
    free_var(ans);
    free_var(req);
    return result;
}

void
remove_LAST_character(char *theStr)
{
 theStr[strlen(theStr)-1] = '\0';
}

void
remove_special_characters(char *theStr)
{
    register char *cp,*cp2;
    char buf[BUF_LEN];
    int currlen = 0;

    cp = theStr;
    cp2 = buf;
    while (( *cp ) && (currlen < BUF_LEN)) {
        switch (*cp) {
        case '&':
        case '|':
        case ';':
        case '<':
        case '>':
        case '(':
        case ')':
        case '\'':
        case '\\':
        case '"':
        case '`':
        case ':':
        case '$':
        case '!':
        case ' ':
            cp++;
            break;
        default: {
            *cp2++ = *cp++;
            currlen++;
            }
        }
    }
    *cp2 = '\0';
    strcpy( theStr, buf);
}

int
build_dir_name(char *thePathStr, char *theDirName, char spec)
{
    char external_files  [BUF_LEN];
    char localthePathStr [BUF_LEN];
    struct stat st;

    if (strlen(thePathStr) > BUF_LEN)  return E_INVARG;

    strcpy(localthePathStr, thePathStr);
    remove_special_characters(localthePathStr);
    if (( strstr(localthePathStr,"/.")) ||
       (!strncmp(localthePathStr,".",1))) {
     return E_PERM;
    }
    strcpy(external_files,FUP_SUBDIR);
    sprintf(theDirName,"%s%s", external_files, localthePathStr);
    
    if (stat(theDirName, &st) != 0) return E_INVARG;

    errno = 0;
    switch (spec)
	    {
	    case 'd':
	      if (!(st.st_mode & S_IFDIR)) return E_INVIND; 	      
	      break;
	    case 'r':
	      if ((access (theDirName, R_OK)) !=0) return E_PERM;
	      break;
	    case 'w':
	      if ((access (theDirName, W_OK)) !=0) return E_PERM;
	      break;
	    case 'x':
	      if ((access (theDirName, X_OK)) !=0) return E_PERM;
	      break;
	    default:
	        return E_ARGS;
	    }
     return E_NONE;
}

int
build_file_name(char *thePathStr, char *theNameStr, char *theFileName, char spec)
{
    char external_files  [BUF_LEN];
    char localthePathStr [BUF_LEN];
    char localtheNameStr [BUF_LEN];
    struct stat st;

#ifdef EXTERN_FILES_DIR_READ_ONLY
    if (strlen(thePathStr) == 0) {
       switch (spec)
       {
        case 'w':
        case 'd':
           return E_PERM;
           break;
       }
    }
#endif

    if ((strlen(thePathStr) > BUF_LEN) || 
        (strlen(theNameStr) > BUF_LEN))  return E_INVARG;

    strcpy(localthePathStr, thePathStr);
    strcpy(localtheNameStr, theNameStr);
    remove_special_characters(localthePathStr);
    remove_special_characters(localtheNameStr);
    
    if (( strstr(localthePathStr,"/.")) ||
       (!strncmp(localthePathStr,".",1)) ||
       (strstr(localtheNameStr,"/"))) {
     return E_PERM;
    }
    strcpy(external_files,FUP_SUBDIR);
    sprintf(theFileName,"%s%s/%s", external_files, localthePathStr, 
                                   localtheNameStr);
    
    if (stat(theFileName, &st) != 0) return E_INVARG;

    errno = 0;
    switch (spec)
	    {
	    case 'd':
	      if (!(st.st_mode & S_IFDIR)) return E_INVIND; 	      
	      break;
	    case 'r':
	      if ((access (theFileName, R_OK)) !=0) return E_PERM;
	      break;
	    case 'w':
	      if ((access (theFileName, W_OK)) !=0) return E_PERM;
	      break;
	    case 'x':
	      if ((access (theFileName, X_OK)) !=0) return E_PERM;
	      break;
	    default:
	        return E_ARGS;
	    }
     return E_NONE;
}

static package
bf_fileexists(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename) */
        char infileName[BUF_LEN];
        Var ret;
        
        ret.type = TYPE_INT;
        ret.v.num = 1;
        if (build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,'r') != E_NONE) {
			ret.v.num = 0;        
		}
        free_var(arglist);
        return make_var_pack(ret);
}

static package
bf_filelength(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename) */
        FILE *f ;
        char infileName[BUF_LEN];
        int num_lines = -1;
        char buffer[BUF_LEN];
        Var ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'r');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }
		f = fopen(infileName, "r");
        for (num_lines = 0; fgets(buffer, BUF_LEN, f); num_lines++);
        fclose(f);
        free_var(arglist);
        ret.type = TYPE_INT;
        ret.v.num = num_lines;
        return make_var_pack(ret);
}

static package
bf_filesize(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename) */
        char infileName[BUF_LEN];
        struct stat st;
        Var ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'r');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }
	    if (stat(infileName, &st) != 0) {
	        free_var(arglist);
	        return make_error_pack(E_INVARG);
	    }
        ret.type = TYPE_INT;
        ret.v.num = (long)st.st_size;
        free_var(arglist);
        return make_var_pack(ret);
}

static package
bf_filewrite(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename, list, [start, end]) */

        FILE *inFile = NULL;
        FILE *outFile = NULL;
        char infileName[BUF_LEN];
        char outfileName[BUF_LEN*2];
        int i, thelength;
        int index;
        int start_line = 1;
        int end_line   = MAX_INT;
        char buffer[BUF_LEN];
        Var ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'w');
        if (result == E_PERM) {
                free_var(arglist);
                return make_error_pack(result);
        }

        ret.type = TYPE_INT;
        ret.v.num = 1;

        snprintf(outfileName,BUF_LEN*2,"%s.%li", infileName,time(0));

        if (arglist.v.list[0].v.num > 3) 
           start_line = arglist.v.list[4].v.num;

       thelength = arglist.v.list[3].v.list[0].v.num;

        if (arglist.v.list[0].v.num > 4) {
              end_line = arglist.v.list[5].v.num;
           } else {
              end_line = arglist.v.list[4].v.num + thelength - 1;
           }

        if ((outFile = fopen(outfileName, "w")) == 0) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
         }

        inFile = fopen(infileName, "r");
        index = 1;
        if (inFile) {
            while ((index < start_line) && (!feof(inFile))) {
               fgets(buffer, BUF_LEN, inFile);
               fputs(buffer,outFile);
               index++;
                }
            while ((index <= end_line) && (!feof(inFile))) {
               fgets(buffer, BUF_LEN, inFile);
               index++;
                }
         }

    for (i = 1; i <= thelength; i++) {
        switch ((int)arglist.v.list[3].v.list[i].type) {
          case TYPE_INT:
            fprintf (outFile, "%d\n",  arglist.v.list[3].v.list[i].v.num);
            break;
          case TYPE_FLOAT:
            fprintf (outFile, "%g\n",  *(arglist.v.list[3].v.list[i].v.fnum));
            break;
          case TYPE_OBJ:
            fprintf (outFile, "#%d\n",  arglist.v.list[3].v.list[i].v.obj);
            break;
          case TYPE_STR:
            fprintf (outFile, "%s\n",  arglist.v.list[3].v.list[i].v.str);
            break;
          case TYPE_ERR:
              fprintf (outFile, "%s\n", unparse_error( arglist.v.list[3].v.list[i].v.err));
            break;
          case TYPE_LIST:
            fprintf (outFile,  "%s\n", "{list}");
            break;
          default:
            fprintf (outFile,  "%s\n", "*** unrecognized VAR TYPE (this should never happen) ***");
        }
      }

        if (inFile) {
            while (!feof(inFile)) {
               fgets(buffer, BUF_LEN, inFile);
               if (!feof(inFile)) {fputs(buffer,outFile);}
                }
        }

        if (outFile) {fclose(outFile);}
        if (inFile)  {fclose(inFile);} 
        rename(outfileName,infileName);
        free_var(arglist);
        return make_var_pack(ret);
}

static package
bf_fileread(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename [start, end]) */

        FILE *f;
        char infileName[BUF_LEN];
        char buffer[BUF_LEN];
        Var ret, theline;
        int index;
        int start_line = 1;
        int end_line   = MAX_INT;
         int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'r');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }

        if (arglist.v.list[0].v.num > 2)
                start_line = arglist.v.list[3].v.num;

        if (arglist.v.list[0].v.num > 3)
                end_line = arglist.v.list[4].v.num;
     
        if ((f = fopen(infileName, "r")) == 0) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
        }

        ret.type = TYPE_LIST;
        ret = new_list(0);
        theline.type = TYPE_STR;

        index = 1;
        while ((index < start_line) && (!feof(f))) {
                fgets(buffer, BUF_LEN, f);
                index++;
                }

        while ((index <= end_line) && (!feof(f))) {
                fgets(buffer, BUF_LEN, f);
                if (!feof(f)) {
                   buffer[strlen(buffer)-1] = '\0';
                   theline.v.str = str_dup(buffer);
                   ret = listappend(ret, theline);
                   }
                index++;
                }

        fclose(f);
        free_var(arglist);
        return make_var_pack(ret);
}

static package
bf_fileappend(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename, list) */

        FILE *outFile = NULL;
        char outfileName[BUF_LEN];
        int i, thelength;
        Var ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            outfileName,
                            'w');
        if (result == E_PERM) {
                free_var(arglist);
                return make_error_pack(result);
        }

        ret.type = TYPE_INT;
        ret.v.num = 1;

        if ((outFile = fopen(outfileName, "a")) == 0) {
            free_var(arglist);
            return make_error_pack(E_INVARG);
        }

      thelength = arglist.v.list[3].v.list[0].v.num;

    for (i = 1; i <= thelength; i++) {
        switch ((int)arglist.v.list[3].v.list[i].type) {
          case TYPE_INT:
            fprintf (outFile, "%d\n",  arglist.v.list[3].v.list[i].v.num);
            break;
          case TYPE_FLOAT:
            fprintf (outFile, "%g\n",  *(arglist.v.list[3].v.list[i].v.fnum));
            break;
          case TYPE_OBJ:
            fprintf (outFile, "#%d\n",  arglist.v.list[3].v.list[i].v.obj);
            break;
          case TYPE_STR:
            fprintf (outFile, "%s\n",  arglist.v.list[3].v.list[i].v.str);
            break;
          case TYPE_ERR:
              fprintf (outFile, "%s\n", unparse_error( arglist.v.list[3].v.list[i].v.err));
            break;
          case TYPE_LIST:
            fprintf (outFile,  "%s\n", "{list}");
            break;
          default:
            fprintf (outFile,  "%s\n", "*** unrecognized VAR TYPE (this should never happen) ***");
        }
      }

        if (outFile) {fclose(outFile);}
        free_var(arglist);
        return make_var_pack(ret);
}

/* new option for inserting lines */
static package
bf_fileinsert(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename, list, start, end) */
 
        FILE *inFile = NULL;
        FILE *outFile = NULL;
        char infileName[BUF_LEN];
        char outfileName[BUF_LEN*2];
        int i, thelength;
        int index;
        int start_line = 1;
        char buffer[BUF_LEN];
        Var ret;
        int result;

        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'w');
        if (result == E_PERM) {
                free_var(arglist);
                return make_error_pack(result);
        }

        ret.type = TYPE_INT;
        ret.v.num = 1;

        snprintf(outfileName,BUF_LEN*2,"%s.%li", infileName,time(0));

        if (arglist.v.list[0].v.num > 3) 
           start_line = arglist.v.list[4].v.num;

       thelength = arglist.v.list[3].v.list[0].v.num;


        if ((outFile = fopen(outfileName, "w")) == 0) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
         }

        inFile = fopen(infileName, "r");
        index = 1;
        if (inFile) {
            while ((index < start_line) && (!feof(inFile))) {
               fgets(buffer, BUF_LEN, inFile);
               fputs(buffer,outFile);
               index++;
                }
        }

    for (i = 1; i <= thelength; i++) {
        switch ((int)arglist.v.list[3].v.list[i].type) {
          case TYPE_INT:
            fprintf (outFile, "%d\n",  arglist.v.list[3].v.list[i].v.num);
            break;
          case TYPE_FLOAT:
            fprintf (outFile, "%g\n",  *(arglist.v.list[3].v.list[i].v.fnum));
            break;
          case TYPE_OBJ:
            fprintf (outFile, "#%d\n",  arglist.v.list[3].v.list[i].v.obj);
            break;
          case TYPE_STR:
            fprintf (outFile, "%s\n",  arglist.v.list[3].v.list[i].v.str);
            break;
          case TYPE_ERR:
              fprintf (outFile, "%s\n", unparse_error( arglist.v.list[3].v.list[i].v.err));
            break;
          case TYPE_LIST:
            fprintf (outFile,  "%s\n", "{list}");
            break;
          default:
            fprintf (outFile,  "%s\n", "*** unrecognized VAR TYPE (this should never happen) ***");
        }
      }

        if (inFile) {
            while (!feof(inFile)) {
               fgets(buffer, BUF_LEN, inFile);
               if (!feof(inFile)) {fputs(buffer,outFile);}
                }
        }

        if (outFile) {fclose(outFile);}
        if (inFile)  {fclose(inFile);} 
        rename(outfileName,infileName);
        free_var(arglist);
        return make_var_pack(ret);
}


static package
bf_filecut(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename, [start, end]) */

        FILE *inFile = NULL;
        FILE *outFile = NULL;
        char infileName[BUF_LEN];
        char outfileName[BUF_LEN*2];
        int index;
        int start_line = 1;
        int end_line   = MAX_INT;
        char buffer[BUF_LEN];
        Var ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'w');
        if (result == E_PERM) {
                free_var(arglist);
                return make_error_pack(result);
        }

        ret.type = TYPE_INT;
        ret.v.num = 1;

        snprintf(outfileName,BUF_LEN*2,"%s.%li", infileName,time(0));

        if (arglist.v.list[0].v.num > 2)
           start_line = arglist.v.list[3].v.num;

        if (arglist.v.list[0].v.num > 3) {
              end_line = arglist.v.list[4].v.num;
           } else {
              end_line = arglist.v.list[3].v.num - 1;
           }

        if ((outFile = fopen(outfileName, "w")) == 0) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
         }

        inFile = fopen(infileName, "r");
        index = 1;
        if (inFile) {
            while ((index < start_line) && (!feof(inFile))) {
               fgets(buffer, BUF_LEN, inFile);
               fputs(buffer,outFile);
               index++;
                }
            while ((index <= end_line) && (!feof(inFile))) {
               fgets(buffer, BUF_LEN, inFile);
               index++;
                }
         }

        if (inFile) {
            while (!feof(inFile)) {
               fgets(buffer, BUF_LEN, inFile);
               if (!feof(inFile)) {fputs(buffer,outFile);}
                }
        }

        if (outFile) {fclose(outFile);}
        if (inFile)  {fclose(inFile);}
        rename(outfileName,infileName);
        free_var(arglist);
        return make_var_pack(ret);
}


static package
bf_filedelete(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename) */
        char infileName[BUF_LEN];
        Var ret;
         int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'w');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }

    if ((remove(infileName)) != 0) {
        free_var(arglist);
        return make_error_pack(E_INVARG);
    }

        free_var(arglist);
        ret.type = TYPE_INT;
        ret.v.num = 1;
        return make_var_pack(ret);
}


static package
bf_filelist(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory) */

        typedef struct dirent MYDIRENT ;
        DIR *dirp;
        DIR *subdir;
        MYDIRENT *dp;
        char rootDir [BUF_LEN];
        char dirName [BUF_LEN*2];
        Var ret, listOfDirs, listOfFiles, theline;
        int srchlen = 0;
        int result;
        result = build_dir_name(arglist.v.list[1].v.str,
                            rootDir,
                            'd');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }

        if (!(dirp = opendir (rootDir))) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
        }

       if (arglist.v.list[0].v.num > 1) {
           srchlen = strlen(arglist.v.list[2].v.str);
        }
        ret.type = TYPE_LIST;
        ret = new_list(0);
        listOfDirs.type = TYPE_LIST;
        listOfDirs = new_list(0);
        listOfFiles.type = TYPE_LIST;
        listOfFiles = new_list(0);
        theline.type = TYPE_STR;

        while ((dp = readdir (dirp)) != 0) {
           if (strncmp(dp->d_name,".",1)) {
               snprintf(dirName,BUF_LEN*2,"%s/%s", rootDir,dp->d_name);
               if ((subdir = opendir(dirName))) {
                    closedir(subdir);
                    theline.v.str = str_dup(dp->d_name);
                    listOfDirs = listappend(listOfDirs, theline);
                }
                else {
                  if ((srchlen == 0) || 
                      (matches(dp->d_name,arglist.v.list[2].v.str))) {
                      theline.v.str = str_dup(dp->d_name);
                      listOfFiles = listappend(listOfFiles, theline);
                  } 
                }
            }
        }
        closedir (dirp);
        ret = listappend(ret, listOfFiles);
        ret = listappend(ret, listOfDirs);
        free_var(arglist);
        return make_var_pack(ret);
}

static package
bf_filegrep(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename, pattern, [option]) */

        FILE *f;
        char infileName[BUF_LEN];
        char buffer[BUF_LEN];
        int line_num = 0;
        Var ret, theline, anum, slist, nlist;
        int strings = TRUE;
        int numbers = FALSE;
        int showfound = TRUE;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'r');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }
        
        if(arglist.v.list[0].v.num == 4) {
          if(strstr(arglist.v.list[4].v.str,"n")) {
             numbers = TRUE;
             if(!(strstr(arglist.v.list[4].v.str,"s"))) {strings = FALSE;}
          }
          if(strstr(arglist.v.list[4].v.str,"v")) {showfound = FALSE;}
        }

        if ((f = fopen(infileName, "r")) == 0) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
        }

        slist.type = TYPE_LIST;
        slist = new_list(0);
        nlist.type = TYPE_LIST;
        nlist = new_list(0);
        ret.type = TYPE_LIST;
        ret = new_list(0);

        theline.type = TYPE_STR;
        anum.type = TYPE_INT;
        while (!feof(f)) {
            fgets(buffer, BUF_LEN, f);
            line_num++;
            
            if (matches(buffer,arglist.v.list[3].v.str) == showfound) {
                    if ((strings == TRUE) && (!feof(f))) {
                      buffer[strlen(buffer)-1] = '\0';
                      theline.v.str = str_dup(buffer);
                      slist = listappend(slist, theline);
                    }
                    if ((numbers == TRUE) && (!feof(f))) {
                      anum.v.num = line_num;
                      nlist = listappend(nlist, anum);
                    }
                 }
              }

        fclose(f);
        free_var(arglist);
        ret = listappend(ret, slist);
        ret = listappend(ret, nlist);
        return make_var_pack(ret);
}

static package
bf_fileextract(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename, start_pattern, end_pattern [,extra_pattern]) */

        FILE *f;
        char infileName[BUF_LEN];
        char buffer[BUF_LEN];
        Var ret;
        Var startList, endList;
        Var startLine, endLine;
        int numOfLine = 0;
        int status = 1;
        int requiredPattern = (arglist.v.list[0].v.num > 4);
        int result;
        
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            infileName,
                            'r');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }

        if ((strlen(arglist.v.list[2].v.str) == 0) ||
            (strlen(arglist.v.list[3].v.str) == 0) ||
            (strlen(arglist.v.list[4].v.str) == 0) ||
            (strlen(arglist.v.list[arglist.v.list[0].v.num].v.str) == 0)) {
          free_var(arglist);
          return make_error_pack(E_INVARG); 
        }

         if ((f = fopen(infileName, "r")) == 0) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
        }

       ret.type = TYPE_LIST;
        ret = new_list(0);

        startList = new_list(0);
        startList.type = TYPE_LIST;

        endList = new_list(0);
        endList.type = TYPE_LIST;
        
        startLine.type = TYPE_INT;
        endLine.type = TYPE_INT;
 
        while (!feof(f)) {
            fgets(buffer, BUF_LEN, f);
            numOfLine++;
            
            if (status == 1) {
               if (matches(buffer,arglist.v.list[3].v.str)) {
	                startLine.v.num = numOfLine;
                        if (requiredPattern == TRUE) {
	            	    status = 2;}
                        else {
                            status = 3;}
	            }
            }
            
            if (status == 2) {
               if (matches(buffer,arglist.v.list[arglist.v.list[0].v.num].v.str)) {
	            	status = 3;
	            }
            }
            
            if ((status == 2) || (status == 3)) {
              if (matches(buffer,arglist.v.list[4].v.str)) {
                        if (status == 3) {
                          startList = listappend(startList, startLine);
                          endLine.v.num = numOfLine;
                          endList = listappend(endList, endLine);
                        }
                        status = 1;
                     }
            }
        }

        ret = listappend(ret,startList);
        ret = listappend(ret,endList);        
        fclose(f);
        free_var(arglist);
        return make_var_pack(ret);
}

static package
bf_fileversion(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
    Var ret;
    ret.type = TYPE_STR;
    ret.v.str = str_dup(FUP_version);
    free_var(arglist);
    return make_var_pack(ret);
}

static package
bf_filerename(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, oldFilename, newFilename) */
        char oldFilename[BUF_LEN];
        char newFilename[BUF_LEN];
        Var ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            oldFilename,
                            'w');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[3].v.str,
                            newFilename,
                            'w');
        if ((result != E_NONE) && (result != E_INVARG)){
                free_var(arglist);
                return make_error_pack(result);
        }

    if ((rename(oldFilename,newFilename)) != 0) {
        return make_error_pack(E_INVARG);
        }
        ret.type = TYPE_INT;
        ret.v.num = 1;
        free_var(arglist);
        return make_var_pack(ret);
}

#ifdef INCLUDE_FILECHMOD

static package
bf_filechmod(Var arglist, Byte next, void *vdata, Objid progr)
{ /* (directory, filename, filemode) */
        char theRequestedAction[BUF_LEN];
        char filename[BUF_LEN];
        char external_files  [BUF_LEN];
        Var ret;
        struct stat st;
        mode_t  mode;
        char filemode[BUF_LEN];
        int r1, r2; 
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            filename,
                            'w');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }

        remove_special_characters(arglist.v.list[3].v.str);
        if (strlen(arglist.v.list[3].v.str) == 0) {
                   free_var(arglist);
                   return make_error_pack(E_INVARG); }

        strcpy(external_files,FUP_SUBDIR);
        sprintf(theRequestedAction,"chmod %s %s%s/%s\n",
                                   arglist.v.list[3].v.str,
                                   external_files,
                                   arglist.v.list[1].v.str,
                                   arglist.v.list[2].v.str);

    if ((system(theRequestedAction)) == 0) {
          return make_error_pack(E_INVARG);
        }

	stat(filename, &st);
        ret.type = TYPE_STR;
        mode = st.st_mode;
        if (S_ISREG(st.st_mode))  mode = st.st_mode - 32768;
        if (S_ISDIR(st.st_mode))  mode = st.st_mode - 16384;
        if (S_ISCHR(st.st_mode))  mode = st.st_mode -  8192;
        if (S_ISBLK(st.st_mode))  mode = st.st_mode - 24576;
        if (S_ISSOCK(st.st_mode)) mode = st.st_mode - 49152;
        if (mode != st.st_mode) {
           r1 = mode / 8;
           r1 = r1 - ((r1 / 8) * 8);
           r2 = mode - ((mode/8) * 8);
           sprintf(filemode,"%ld%d%d",(long)mode/64,r1,r2);
           ret.v.str = str_dup(filemode);
           }
        else ret.v.str = str_dup("????");
        free_var(arglist);
        return make_var_pack(ret);
}

#endif

static package
bf_fileinfo(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{ /* (directory, filename) */
        char filename[BUF_LEN];
        Var ret, atime, mtime, ctime, fsize, ftype, fmode, fuid, fgid;
        struct stat st;
        struct passwd *pw;
        struct group *grp;
        mode_t  mode;
        int r1, r2;
        char filemode[BUF_LEN];
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            filename,
                            'r');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }

        if (stat(filename, &st) != 0) {
           free_var(arglist);
           return make_error_pack(E_INVARG);
        }

        fsize.type = TYPE_INT;
        fsize.v.num = (long)st.st_size;

        ftype.type = TYPE_STR;
        ftype.v.str = str_dup("???");
        mode = st.st_mode;
        if (S_ISREG(st.st_mode)) {
           ftype.v.str = str_dup("reg");
           mode = st.st_mode - 32768;
           }
        if (S_ISDIR(st.st_mode)) {
           ftype.v.str = str_dup("dir");
           mode = st.st_mode - 16384;
           }

        if (S_ISFIFO(st.st_mode)) { oklog("FIFO %ld\n",(long)st.st_mode);
           }

        if (S_ISCHR(st.st_mode)) {
           ftype.v.str = str_dup("chr");
           mode = st.st_mode - 8192;
           }

        if (S_ISBLK(st.st_mode)) {
           ftype.v.str = str_dup("blk");
           mode = st.st_mode - 24576;
           }

        if (S_ISLNK(st.st_mode)) { oklog("LNK  %ld\n",(long)st.st_mode);
           }

        if (S_ISSOCK(st.st_mode)) {
           ftype.v.str = str_dup("sck");
           mode = st.st_mode - 49152;
           }

        fmode.type = TYPE_STR;
        fmode.v.str = str_dup("????");
        if (mode != st.st_mode) {
           r1 = mode / 8;
           r1 = r1 - ((r1 / 8) * 8);
           r2 = mode - ((mode/8) * 8);
           sprintf(filemode,"%ld%d%d",(long)mode/64,r1,r2);
           fmode.v.str = str_dup(filemode);
           }

        pw = getpwuid((short)st.st_uid);
        fuid.type = TYPE_STR;
        fuid.v.str = str_dup(pw->pw_name);

        grp = getgrgid((short)st.st_gid);
        fgid.type = TYPE_STR;
        fgid.v.str = str_dup(grp->gr_name);

        atime.type = TYPE_INT;
        atime.v.num = st.st_atime;

        mtime.type = TYPE_INT;
        mtime.v.num = st.st_mtime;

        ctime.type = TYPE_INT;
        ctime.v.num = st.st_ctime;
        
        ret.type = TYPE_LIST;
        ret = new_list(0);
        ret = listappend(ret, fsize); /* total size of file, bytes */
        ret = listappend(ret, ftype); /* file type */
        ret = listappend(ret, fmode); /* file mode */
        ret = listappend(ret, fuid);  /* user ID of owner */
        ret = listappend(ret, fgid);  /* group ID of owner */
        ret = listappend(ret, atime); /* file last access time */
        ret = listappend(ret, mtime); /* file last modify time */
        ret = listappend(ret, ctime); /* file last change time */

        free_var(arglist);
        return make_var_pack(ret);
}

#ifdef INCLUDE_FILERUN

static package
bf_filerun(Var arglist, Byte next, void *vdata, Objid progr)
{ /* (filename, arguments) */
        char theRequestedAction[BUF_LEN];
        Var ret, theArgs, theline;
        int i, numOfArgs, result;
        char fileName[BUF_LEN];
        char external_bin [BUF_LEN];

        theline.type = TYPE_STR;
        theArgs.type = TYPE_LIST;
        theArgs = new_list(0);
        numOfArgs = arglist.v.list[0].v.num;
        for (i = 1; i <= numOfArgs; i++) {
	        switch (arglist.v.list[i].type) {
	          case TYPE_STR:
                theline.v.str = str_dup(arglist.v.list[i].v.str);
                remove_special_characters( theline.v.str); 

                if (( strstr(theline.v.str,"/.")) ||
                   (!strncmp(theline.v.str,".",1)) ||
                   (!strncmp(theline.v.str,"/",1))) {
                    free_var(arglist);
                    free_var(theline);
                    return make_error_pack(E_PERM);
                    }

                theArgs = listappend(theArgs,theline);
	        break;
	          case TYPE_LIST:
                    if (arglist.v.list[i].v.list[0].v.num < 2) {
                        theline.v.str = str_dup("");
                        theArgs = listappend(theArgs,theline);
                    } else {
                    if ((arglist.v.list[i].v.list[1].type != TYPE_STR) ||
                       (arglist.v.list[i].v.list[2].type != TYPE_STR)) {
			            free_var(arglist);
                                    free_var(theline);
			            return make_error_pack(E_TYPE); 
                       }
                    result = build_file_name(arglist.v.list[i].v.list[1].v.str,
                                             arglist.v.list[i].v.list[2].v.str,
                                             fileName,
                                             'r');
                        theline.v.str = str_dup(fileName);
                        theArgs = listappend(theArgs,theline);
					}
	            break;
	          default:
                free_var(arglist);
                free_var(theline);
                return make_error_pack(E_INVARG);
	        }        
        }
       
       numOfArgs = theArgs.v.list[0].v.num;
       strcpy(external_bin,EXTERN_BIN_DIR);
       sprintf(theRequestedAction,"%s%s ",external_bin,theArgs.v.list[1].v.str);

        if ((numOfArgs > 1) && (strlen(theArgs.v.list[2].v.str)!=0)) {
                 sprintf(theRequestedAction,"cat %s | %s%s",
                                   theArgs.v.list[2].v.str,
                                   external_bin,
                                   theArgs.v.list[1].v.str);
        } else {
                 sprintf(theRequestedAction,"%s%s ",
                                   external_bin,
                                   theArgs.v.list[1].v.str);
        }

        for (i = 4; i <= numOfArgs; i++) {
        sprintf(theRequestedAction,"%s %s",
                                   theRequestedAction,
                                   theArgs.v.list[i].v.str);
        }

        if ((numOfArgs > 2) && (strlen(theArgs.v.list[3].v.str))){
            sprintf(theRequestedAction,"%s > %s",
                                   theRequestedAction,
                                   theArgs.v.list[3].v.str);
        }

        sprintf(theRequestedAction,"%s 2>&1", theRequestedAction);
        system(theRequestedAction);

        ret.type = TYPE_INT;
        ret.v.num = 1; /* always !! */
        free_var(arglist);
        free_var(theline);
        return make_var_pack(ret);
}
#endif

static package
bf_filemkdir(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{  /* filemkdir(base-directory-name, new-directory-name) */
      char newdirName[BUF_LEN];
      mode_t create_mode = CREATE_NEW_DIR_MODE;
      Var ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            newdirName,
                            'd');
        if ((result != E_NONE) && (result != E_INVARG)){
                free_var(arglist);
                return make_error_pack(result);
        }

    if ((mkdir(newdirName, create_mode)) != 0) {
            free_var(arglist);
            return make_error_pack(E_PERM); }

    ret.type = TYPE_INT;
    ret.v.num = 1;
    free_var(arglist);
    return make_var_pack(ret);
}

static package
bf_filermdir(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid prog)
{  /* filermdir(base-directory-name, directory-name) */
        char rmDirName[BUF_LEN];
        Var  ret;
        int result;
        result = build_file_name(arglist.v.list[1].v.str,
                            arglist.v.list[2].v.str,
                            rmDirName,
                            'd');
        if (result != E_NONE) {
                free_var(arglist);
                return make_error_pack(result);
        }

    if ((rmdir(rmDirName)) != 0) {
        free_var(arglist);
        return make_error_pack(E_PERM); 
    }
    free_var(arglist);
    ret.type = TYPE_INT;
    ret.v.num = 1;
    return make_var_pack(ret);
}

static package
bf_fileerror(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
    Var ret;
    ret.type = TYPE_STR;
    ret.v.str = str_dup(strerror(errno));
    free_var(arglist);
    return make_var_pack(ret);
}

void
register_extensions(void)
{
	register_function("asc", 1, 1, bf_asc, TYPE_STR);
	register_function("assoc", 2, 3, bf_assoc, TYPE_ANY, TYPE_LIST, TYPE_INT);
	register_function("chr", 1, 1, bf_chr, TYPE_INT);
	register_function("clock", 0, 0, bf_clock);
	register_function("enlist", 1, 1, bf_enlist, TYPE_ANY);
	register_function("file_chmod", 2, 2, bf_file_chmod, TYPE_STR, TYPE_STR);
	register_function("file_close", 1, 1, bf_file_close, TYPE_INT);
	register_function("file_eof", 1, 1, bf_file_eof, TYPE_INT);
	register_function("file_flush", 1, 1, bf_file_flush, TYPE_INT);
	register_function("file_last_access", 1, 1, bf_file_last_access, TYPE_ANY);
	register_function("file_last_change", 1, 1, bf_file_last_change, TYPE_ANY);
	register_function("file_last_modify", 1, 1, bf_file_last_modify, TYPE_ANY);
	register_function("file_list", 1, 2, bf_file_list, TYPE_STR, TYPE_ANY);
	register_function("file_mkdir", 1, 1, bf_file_mkdir, TYPE_STR);
	register_function("file_mode", 1, 1, bf_file_mode, TYPE_ANY);
	register_function("file_name", 1, 1, bf_file_name, TYPE_INT);
	register_function("file_open", 2, 2, bf_file_open, TYPE_STR, TYPE_STR);
	register_function("file_openmode", 1, 1, bf_file_openmode, TYPE_INT);
	register_function("file_read", 2, 2, bf_file_read, TYPE_INT, TYPE_INT);
	register_function("file_readline", 1, 1, bf_file_readline, TYPE_INT);
	register_function("file_readlines", 3, 3, bf_file_readlines, TYPE_INT, TYPE_INT, TYPE_INT);
	register_function("file_remove", 1, 1, bf_file_remove, TYPE_STR);
	register_function("file_rename", 2, 2, bf_file_rename, TYPE_STR, TYPE_STR);
	register_function("file_rmdir", 1, 1, bf_file_rmdir, TYPE_STR);
	register_function("file_seek", 3, 3, bf_file_seek, TYPE_INT, TYPE_INT, TYPE_STR);
	register_function("file_size", 1, 1, bf_file_size, TYPE_ANY);
	register_function("file_stat", 1, 1, bf_file_stat, TYPE_ANY);
	register_function("file_tell", 1, 1, bf_file_tell, TYPE_INT);
	register_function("file_type", 1, 1, bf_file_type, TYPE_ANY);
	register_function("file_version", 0, 0, bf_file_version);
	register_function("file_write", 2, 2, bf_file_write, TYPE_INT, TYPE_STR);
	register_function("file_writeline", 2, 2, bf_file_writeline, TYPE_INT, TYPE_STR);
	register_function("fileappend", 3, 3, bf_fileappend, TYPE_STR, TYPE_STR, TYPE_LIST);
#ifdef INCLUDE_FILECHMOD
	register_function("filechmod",   3,  3, bf_filechmod,   TYPE_STR, TYPE_STR, TYPE_STR);
#endif
	register_function("filedelete", 2, 2, bf_filedelete, TYPE_STR, TYPE_STR);
	register_function("fileerror", 0, 0, bf_fileerror);
	register_function("fileexists", 2, 2, bf_fileexists, TYPE_STR, TYPE_STR);
	register_function("fileextract", 4, 5, bf_fileextract, TYPE_STR, TYPE_STR, TYPE_STR, TYPE_STR, TYPE_STR);
	register_function("filegrep", 3, 4, bf_filegrep, TYPE_STR, TYPE_STR, TYPE_STR, TYPE_STR);
	register_function("fileinfo", 2, 2, bf_fileinfo, TYPE_STR, TYPE_STR);
	register_function("filelength", 2, 2, bf_filelength, TYPE_STR, TYPE_STR);
	register_function("filelist", 1, 2, bf_filelist, TYPE_STR, TYPE_STR);
	register_function("filemkdir", 2, 2, bf_filemkdir, TYPE_STR, TYPE_STR);
	register_function("fileread", 2, 4, bf_fileread, TYPE_STR, TYPE_STR, TYPE_INT, TYPE_INT);
	register_function("filerename", 3, 3, bf_filerename, TYPE_STR, TYPE_STR, TYPE_STR);
	register_function("filermdir", 2, 2, bf_filermdir, TYPE_STR, TYPE_STR);
#ifdef INCLUDE_FILERUN
	register_function("filerun",     1, -1, bf_filerun,     TYPE_STR, TYPE_LIST, TYPE_LIST); 
#endif
	register_function("filesize", 2, 2, bf_filesize, TYPE_STR, TYPE_STR);
	register_function("fileversion", 0, 0, bf_fileversion);
	register_function("filewrite", 3, 3, bf_filewrite, TYPE_STR, TYPE_STR, TYPE_LIST);
	register_function("fileinsert",  3,  5, bf_fileinsert, TYPE_STR, TYPE_STR, TYPE_LIST, TYPE_INT, TYPE_INT);
	register_function("filecut",     2,  4, bf_filecut, TYPE_STR, TYPE_STR, TYPE_INT,  TYPE_INT);
	register_function("find_verb", 2, 3, bf_find_verb, TYPE_OBJ, TYPE_STR, TYPE_ANY);
	register_function("iassoc", 2, 3, bf_iassoc, TYPE_ANY, TYPE_LIST, TYPE_INT);
	register_function("isa", 2, 2, bf_isa, TYPE_OBJ, TYPE_OBJ);
	register_function("make", 1, 2, bf_make, TYPE_INT, TYPE_ANY);
	register_function("match_verbname", 2, 2, bf_verbname_match, TYPE_STR, TYPE_STR);
	register_function("gettimeofday", 0, 0, bf_gettimeofday);
	register_function("nprogs", 0, 0, bf_nprogs);
	register_function("random_of", 1, 1, bf_random_of, TYPE_LIST);
	register_function("remove_duplicates", 1, 1, bf_remove_duplicates, TYPE_LIST);
	register_function("slice", 1, 2, bf_slice, TYPE_LIST, TYPE_INT);
	register_function("sort", 1, 1, bf_sort, TYPE_LIST);
	register_function("string_hash_as_int", 1, 1, bf_strhash, TYPE_STR);
	register_function("xor", 2, 2, bf_xor, TYPE_ANY, TYPE_ANY);
}
