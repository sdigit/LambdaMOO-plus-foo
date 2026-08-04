/* $egnet: network.c,v 1.20 2009/01/18 09:38:39 dive Exp $ */

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
  Copyright (c) 1992 Xerox Corporation.  All rights reserved.
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
 * My first attempt at a sane network implementation for the LambdaMOO server.
 * This should work on BSDish and SYSVish systems; I wonder whether the old
 * implementation needed to be quite so disgusting.
 * -dive
 */


#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "config.h"
#include "exceptions.h"
#include "list.h"
#include "log.h"
#include "name_lookup.h"
#include "options.h"
#include "server.h"
#include "streams.h"
#include "structures.h"
#include "storage.h"
#include "timers.h"
#include "utils.h"

#ifdef HAVE_FILIO_H
#include <sys/filio.h>
#endif


#define EOL_STR "\r\n"
#define EOL_LEN 2

typedef struct text_block {
	struct text_block *next;
	int             length;
	char           *buffer;
	char           *start;
}               text_block;

typedef struct nhandle {
	struct nhandle *next, **prev;
	server_handle   shandle;
	int             rfd;
	int             wfd;
	char           *name;
	Stream         *input;
	int             last_input_was_CR;
	int             input_suspended;
	text_block     *output_head;
	text_block    **output_tail;
	int             output_length;
	int             output_lines_flushed;
	int             outbound, binary;
	int             client_echo;
}               nhandle;

typedef struct nlistener {
	struct nlistener *next, **prev;
	server_listener slistener;
	int             fd;
	int             name_lookup;
	Var             options;
	const char     *name;
}               nlistener;

typedef void    (*network_fd_callback) (int, void *);

typedef struct fd_reg_t {
	int             fd;
	network_fd_callback readable;
	network_fd_callback writable;
	void           *data;
	struct fd_reg_t *next;
}               fd_reg;

static fd_reg  *reg_fds = 0;
static nlistener *all_nlisteners = 0;
static nhandle *all_nhandles = 0;
static fd_set   input, output;
static int      maxfd;

void
mplex_clear()
{
	FD_ZERO(&input);
	FD_ZERO(&output);
	maxfd = -1;
}

void
mplex_add_reader(fd)
	int             fd;
{
	FD_SET(fd, &input);
	if (fd > maxfd)
		maxfd = fd;
}

void
mplex_add_writer(fd)
	int             fd;
{
	FD_SET(fd, &output);
	if (fd > maxfd)
		maxfd = fd;
}

int
mplex_wait(timeout)
	unsigned long   timeout;
{
	struct timeval  tv;
	int             n;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	n = select(maxfd + 1, (fd_set *) & input, (fd_set *) & output, (fd_set *) NULL, &tv);
	if (n < 0) {
		if (errno != EINTR)
			log_perror("selecting");
		return 1;
	} else {
		return (n == 0);
	}
}

int
mplex_is_readable(fd)
	int             fd;
{
	return FD_ISSET(fd, &input);
}

int
mplex_is_writable(fd)
	int             fd;
{
	return FD_ISSET(fd, &output);
}

enum error
tcp_make_listener(desc, fd, canon, name)
	Var             desc;
	int            *fd;
	Var            *canon;
	const char    **name;
{
	int             s, port;
	int             option = 1;
	unsigned int	length;
	static Stream  *st = 0;
	struct sockaddr_in address;

	if (!st)
		st = new_stream(20);

	if (desc.type != TYPE_INT)
		return E_TYPE;

	port = desc.v.num;
	length = sizeof(address);
	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		log_perror("socket() failed");
		return E_QUOTA;
	}
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
		log_perror("setsockopt(SOL_REUSEADDR) failed");
		close(s);
		return E_QUOTA;
	}
	if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option)) < 0) {
		log_perror("setsockopt(SO_KEEPALIVE) failed");
		close(s);
		return E_QUOTA;
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);
	if (bind(s, (struct sockaddr *) & address, length) < 0) {
		log_perror("bind() failed");
		close(s);
		if (errno == EACCES)
			return E_PERM;
		else
			return E_QUOTA;
	}
	if (port == 0) {
		if (getsockname(s, (struct sockaddr *) & address, &length) < 0) {
			log_perror("getsockname() failed");
			close(s);
			return E_QUOTA;
		}
		canon->type = TYPE_INT;
		canon->v.num = ntohs(address.sin_port);
	} else {
		*canon = var_ref(desc);
	}
	stream_printf(st, "port %d", canon->v.num);
	*name = reset_stream(st);
	*fd = s;
	return E_NONE;
}

enum proto_accept_error {
	PA_OKAY, PA_FULL, PA_OTHER
};

enum proto_accept_error
tcp_accept(listener_fd, read_fd, write_fd, name, name_lookup)
	int             listener_fd;
	int            *read_fd;
	int            *write_fd;
	const char    **name;
	int             name_lookup;
{
	int             timeout = server_int_option("name_lookup_timeout", 5);
	int             fd;
	struct sockaddr_in address;
	unsigned int	addr_length = sizeof(address);
	static Stream  *s = 0;
	int		option = 1;

	if (!s)
		s = new_stream(100);

	fd = accept(listener_fd, (struct sockaddr *) & address, &addr_length);
	if (fd < 0) {
		if (errno == EMFILE) {
			return PA_FULL;
		} else {
			log_perror("accept() failed");
			return PA_OTHER;
		}
	}
	if (setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&option,sizeof(option)) < 0) {
		log_perror("setsockopt(SO_KEEPALIVE) failed");
	}
	*read_fd = *write_fd = fd;
	stream_printf(s, "%s, port %d",
		      lookup_name_from_addr(&address, timeout, name_lookup),
		      (int) ntohs(address.sin_port));
	*name = reset_stream(s);
	return PA_OKAY;
}

static Exception timeout_exception;

static void
timeout_proc([[maybe_unused]] Timer_ID id, [[maybe_unused]] Timer_Data data)
{
	RAISE(timeout_exception, 0);
}

#ifdef OUTBOUND_NETWORK

enum error
tcp_open(arglist, read_fd, write_fd, local_name, remote_name)
	Var             arglist;
	int            *read_fd;
	int            *write_fd;
	const char    **local_name;
	const char    **remote_name;
{
	static const char *host_name;
	static int      port;
	static Timer_ID id;
	int             s, result;
	unsigned int	length;
	int             timeout = server_int_option("name_lookup_timeout", 5);
	static struct sockaddr_in addr;
	static Stream  *st1 = 0, *st2 = 0;
	int		option = 1;

	if (!st1) {
		st1 = new_stream(20);
		st2 = new_stream(50);
	}
	if (arglist.v.list[0].v.num != 2)
		return E_ARGS;
	else if (arglist.v.list[1].type != TYPE_STR ||
		 arglist.v.list[2].type != TYPE_INT)
		return E_TYPE;

	host_name = arglist.v.list[1].v.str;
	port = arglist.v.list[2].v.num;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = lookup_addr_from_name(host_name, timeout);
	if (addr.sin_addr.s_addr == 0)
		return E_INVARG;

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		log_perror("socket() failed");
		return E_QUOTA;
	}
	if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof(option)) < 0) {
		log_perror("setsockopt(SO_KEEPALIVE) failed");
		close(s);
		return E_QUOTA;
	}
	TRY
		id = set_timer(server_int_option("outbound_connect_timeout", 5), timeout_proc, 0);
	result = connect(s, (struct sockaddr *) & addr, sizeof(addr));
	cancel_timer(id);
	EXCEPT(timeout_exception)
		result = -1;
	errno = ETIMEDOUT;
	reenable_timers();
	ENDTRY
		if (result < 0) {
		close(s);
		if (errno == EADDRNOTAVAIL ||
		    errno == ECONNREFUSED ||
		    errno == ENETUNREACH ||
		    errno == ETIMEDOUT)
			return E_INVARG;
		log_perror("tcp_open()");
		return E_QUOTA;
	}
	length = sizeof(addr);
	if (getsockname(s, (struct sockaddr *) & addr, &length) < 0) {
		close(s);
		log_perror("getsockname() failed");
		return E_QUOTA;
	}
	*read_fd = *write_fd = s;
	stream_printf(st1, "port %d", (int) ntohs(addr.sin_port));
	*local_name = reset_stream(st1);
	stream_printf(st2, "%s, port %d", host_name, port);
	*remote_name = reset_stream(st2);
	return E_NONE;
}
#endif				/* OUTBOUND_NETWORK */

void
close_connection(int read_fd, [[maybe_unused]] int write_fd)
{
	close(read_fd);
}

void
close_listener(fd)
	int             fd;
{
	close(fd);
}

void
network_register_fd(fd, readable, writable, data)
	int             fd;
	network_fd_callback readable;
	network_fd_callback writable;
	void           *data;
{
	fd_reg         *regptr;

	regptr = reg_fds;
	if (!regptr) {
		regptr = mymalloc(sizeof(fd_reg), M_NETWORK);
		regptr->next = NULL;
		reg_fds = regptr;
	} else {
		while (regptr->next != NULL)
			regptr = regptr->next;
		regptr->next = mymalloc(sizeof(fd_reg), M_NETWORK);
		regptr = regptr->next;
	}
	regptr->fd = fd;
	regptr->readable = readable;
	regptr->writable = writable;
	regptr->data = data;
}

void
network_unregister_fd(fd)
	int             fd;
{
	fd_reg         *ptr;
	fd_reg         *lastptr;

	ptr = reg_fds;
	if (ptr == NULL)
		server_panic("[unregister_fd] reg_fds is NULL!");
	if (ptr->fd == fd) {
		reg_fds = ptr->next;
		free(ptr);
	} else {
        if (ptr->next == NULL) {
            free(ptr);
            return;
        }
		while (ptr->next != NULL) {
			lastptr = ptr;
			ptr = ptr->next;
			if (ptr->fd == fd) {
				lastptr->next = ptr->next;
                free(ptr);
			}
		}
	}
}

static void
add_registered_fds()
{
	fd_reg         *r;

	r = reg_fds;
	while (r != NULL) {
		if (r->readable)
			mplex_add_reader(r->fd);
		if (r->writable)
			mplex_add_writer(r->fd);
		r = r->next;
	}
}

static void
check_registered_fds()
{
	fd_reg         *r;
	r = reg_fds;
	while (r != NULL) {
		if (r->readable && mplex_is_readable(r->fd))
			(*r->readable) (r->fd, r->data);
		if (r->writable && mplex_is_writable(r->fd))
			(*r->writable) (r->fd, r->data);
		r = r->next;
	}
}

static void
free_text_block(b)
	text_block     *b;
{
	free(b->buffer);
	free(b);
}

int
network_set_nonblocking(fd)
	int             fd;
{
	int             a;

	a = 1;
	if (ioctl(fd, FIONBIO, &a) < 0)
		return 0;
	else
		return 1;
}

static int
push_output(h)
	nhandle        *h;
{
	text_block     *b;
	int             count;

	if (h->output_lines_flushed > 0) {
		char            buf[128];
		int             length;
		snprintf(buf, 128, "\r\n>> network buffer overflow: %u line(s) flushed\r\n",
			 h->output_lines_flushed);
		length = strlen(buf);
		count = write(h->wfd, buf, length);
		if (count == length)
			h->output_lines_flushed = 0;
		else
			return (count >= 0 || errno == EAGAIN || errno == EWOULDBLOCK);
	}
	while ((b = h->output_head) != 0) {
		count = write(h->wfd, b->start, b->length);
		if (count < 0)
			return (errno == EAGAIN || errno == EWOULDBLOCK);
		h->output_length -= count;
		if (count == b->length) {
			h->output_head = b->next;
			free_text_block(b);
		} else {
			b->start += count;
			b->length -= count;
		}
	}
	if (h->output_head == 0)
		h->output_tail = &(h->output_head);
	return 1;
}

static int
pull_input(h)
	nhandle        *h;
{
	Stream         *s = h->input;
	int             count;
	char            buffer[1024];
	char           *ptr, *end;
	unsigned char   c;

	if ((count = read(h->rfd, buffer, sizeof(buffer))) > 0) {
		if (h->binary) {
			stream_add_string(s, raw_bytes_to_binary(buffer, count));
			server_receive_line(h->shandle, reset_stream(s));
			h->last_input_was_CR = 0;
		} else {
			for (ptr = buffer, end = buffer + count; ptr < end; ptr++) {
				c = *ptr;
				if (c == 0x08 || c == 0x7F)
					(s->current > 0) ? s->current-- : 0;
				if (isgraph(c) || c == ' ' || c == '\t')
					stream_add_char(s, c);
				else if (c == '\r' || (c == '\n' && !h->last_input_was_CR))
					server_receive_line(h->shandle, reset_stream(s));
				h->last_input_was_CR = (c == '\r');
			}
		}
		return 1;
	} else {
		return (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
	}
}

static nhandle *
new_nhandle(rfd, wfd, local_name, remote_name, outbound)
	int             rfd;
	int             wfd;
	const char     *local_name;
	const char     *remote_name;
	int             outbound;
{
	nhandle        *h;
	static Stream  *s = 0;

	s = new_stream(100);
	if (!network_set_nonblocking(rfd) || (rfd != wfd && !network_set_nonblocking(wfd)))
		log_perror("setting non blocking");
	h = mymalloc(sizeof(nhandle), M_NETWORK);
	if (all_nhandles)
		all_nhandles->prev = &(h->next);
	h->next = all_nhandles;
	h->prev = &all_nhandles;
	all_nhandles = h;
	h->rfd = rfd;
	h->wfd = wfd;
	h->input = new_stream(100);
	h->last_input_was_CR = 0;
	h->input_suspended = 0;
	h->output_head = 0;
	h->output_tail = &(h->output_head);
	h->output_length = 0;
	h->output_lines_flushed = 0;
	h->binary = 0;
	h->client_echo = 1;
	stream_printf(s, "%s %s %s",
		      local_name, outbound ? "to" : "from", remote_name);
	h->name = str_dup(reset_stream(s));
	return h;
}

static void
close_nhandle(h)
	nhandle        *h;
{
	text_block     *b, *bb;

	push_output(h);
	*(h->prev) = h->next;
	if (h->next)
		h->next->prev = h->prev;
	b = h->output_head;
	while (b) {
		bb = b->next;
		free_text_block(b);
		b = bb;
	}
	free_stream(h->input);
	close_connection(h->rfd, h->wfd);
	free_str(h->name);
	myfree(h, M_NETWORK);
}

static void
close_nlistener(l)
	nlistener      *l;
{
	*(l->prev) = l->next;
	if (l->next)
		l->next->prev = l->prev;
	close_listener(l->fd);
	free_str(l->name);
	free_var(l->options);
	myfree(l, M_NETWORK);
}

static void
make_new_connection(sl, rfd, wfd, local_name, remote_name, outbound)
	server_listener sl;
	int             rfd;
	int             wfd;
	const char     *local_name;
	const char     *remote_name;
	int             outbound;
{
	nhandle        *h;
	network_handle  nh;

	nh.ptr = h = new_nhandle(rfd, wfd, local_name, remote_name, outbound);
	h->shandle = server_new_connection(sl, nh, outbound);
}

static void
accept_new_connection(l)
	nlistener      *l;
{
	int             rfd, wfd;
	const char     *host_name;
	int             name_lookup = l->name_lookup;
	switch (tcp_accept(l->fd, &rfd, &wfd, &host_name, name_lookup)) {
	case PA_OKAY:
		make_new_connection(l->slistener, rfd, wfd, l->name, host_name, 0);
		break;
	case PA_FULL:
		errlog("PA_FULL!!! :(\n");
		break;
	case PA_OTHER:
		break;
	}
}

static int
enqueue_output(nh, line, line_length, add_eol, flush_ok)
	network_handle  nh;
	const char     *line;
	int             line_length;
	int             add_eol;
	int             flush_ok;
{
	nhandle        *h = nh.ptr;
	int             length = line_length + (add_eol ? 2 : 0);
	char           *buffer;
	text_block     *block, *b;
	int             to_flush;

	if (h->output_length != 0 && h->output_length + length + MAX_QUEUED_OUTPUT) {
		push_output(h);
		to_flush = h->output_length + length - MAX_QUEUED_OUTPUT;
		if (to_flush > 0 && !flush_ok)
			return 0;
		while (to_flush > 0 && (b = h->output_head)) {
			h->output_length -= b->length;
			to_flush -= b->length;
			h->output_lines_flushed++;
			h->output_head = b->next;
			free_text_block(b);
		}
		if (h->output_head == 0)
			h->output_tail = &(h->output_head);
	}
	buffer = mymalloc(length * sizeof(char), M_NETWORK);
	block = mymalloc(sizeof(text_block), M_NETWORK);
	memcpy(buffer, line, line_length);
	if (add_eol)
		memcpy(buffer + line_length, EOL_STR, EOL_LEN);
	block->buffer = block->start = buffer;
	block->length = length;
	block->next = 0;
	*(h->output_tail) = block;
	h->output_tail = &(block->next);
	h->output_length += length;

	return 1;
}

int
network_initialize(argc, argv, desc)
	int             argc;
	char          **argv;
	Var            *desc;
{
	int             port = DEFAULT_PORT;
	char           *p;

	initialize_name_lookup();
	signal(SIGPIPE, SIG_IGN);
	if (argc > 1)
		return 0;
	else if (argc == 1) {
		port = strtoul(argv[0], &p, 10);
		if (*p != '\0')
			return 0;
	}
	desc->type = TYPE_INT;
	desc->v.num = port;
	return 1;
}

enum error
network_make_listener(sl, desc, nl, canon, name, options)
	server_listener sl;
	Var             desc;
	network_listener *nl;
	Var            *canon;
	const char    **name;
	Var            *options;
{
	int             fd;
	enum error      e = tcp_make_listener(desc, &fd, canon, name);
	nlistener      *l;

	if (e == E_NONE) {
		Var             optval;
		int             name_lookup;
		*options = var_ref(*options);
		if (options_list_value(*options, "name-lookup", &optval)) {
			name_lookup = is_true(optval);
		} else {
			Var             nl_option;
			name_lookup = server_flag_option_default("listener_name_lookup",
					      DEFAULT_LISTENER_NAME_LOOKUP);
			nl_option = new_list(2);
			nl_option.v.list[1].type = TYPE_STR;
			nl_option.v.list[1].v.str = str_dup("name-lookup");
			nl_option.v.list[2].type = TYPE_INT;
			nl_option.v.list[2].v.num = name_lookup;
			*options = listappend(*options, nl_option);
		}
		nl->ptr = l = mymalloc(sizeof(nlistener), M_NETWORK);
		l->fd = fd;
		l->slistener = sl;
		l->name = str_dup(*name);
		l->name_lookup = name_lookup;
		l->options = *options;
		if (all_nlisteners)
			all_nlisteners->prev = &(l->next);
		l->next = all_nlisteners;
		l->prev = &all_nlisteners;
		all_nlisteners = l;
	}
	return e;
}

int
network_listen(nl)
	network_listener nl;
{
	nlistener      *l = nl.ptr;
	listen(l->fd, SOMAXCONN);
	return 1;
}

int
network_send_line(nh, line, flush_ok)
	network_handle  nh;
	const char     *line;
	int             flush_ok;
{
	return enqueue_output(nh, line, strlen(line), 1, flush_ok);
}

int
network_send_bytes(nh, buffer, buflen, flush_ok)
	network_handle  nh;
	const char     *buffer;
	int             buflen;
	int             flush_ok;
{
	return enqueue_output(nh, buffer, buflen, 0, flush_ok);
}

int
network_buffered_output_length(nh)
	network_handle  nh;
{
	nhandle        *h = nh.ptr;
	return h->output_length;
}

void
network_suspend_input(nh)
	network_handle  nh;
{
	nhandle        *h = nh.ptr;
	h->input_suspended = 1;
}

void
network_resume_input(nh)
	network_handle  nh;
{
	nhandle        *h = nh.ptr;
	h->input_suspended = 0;
}

int
network_process_io(timeout)
	int             timeout;
{
	nhandle        *h, *hnext;
	nlistener      *l;

	mplex_clear();
	for (l = all_nlisteners; l; l = l->next)
		mplex_add_reader(l->fd);
	for (h = all_nhandles; h; h = h->next) {
		if (!h->input_suspended)
			mplex_add_reader(h->rfd);
		if (h->output_head)
			mplex_add_writer(h->wfd);
	}
	add_registered_fds();

	if (mplex_wait(timeout)) {
		return 0;
	} else {
		for (l = all_nlisteners; l; l = l->next)
			if (mplex_is_readable(l->fd))
				accept_new_connection(l);
		for (h = all_nhandles; h; h = hnext) {
			hnext = h->next;
			if ((mplex_is_readable(h->rfd) && !pull_input(h))
			|| (mplex_is_writable(h->wfd) && !push_output(h))) {
				server_close(h->shandle);
				close_nhandle(h);
			}
		}
		check_registered_fds();
		return 1;
	}
}

const char     *
network_connection_name(nh)
	network_handle  nh;
{
	nhandle        *h = (nhandle *) nh.ptr;
	return h->name;
}

void
network_set_connection_binary(nh, do_binary)
	network_handle  nh;
	int             do_binary;
{
	nhandle        *h = nh.ptr;
	h->binary = do_binary;
}

Var
network_connection_options(nh, list)
	network_handle  nh;
	Var             list;
{
	nhandle        *h = nh.ptr;
	Var             pair;

	pair = new_list(2);
	pair.v.list[1].type = TYPE_STR;
	pair.v.list[1].v.str = str_dup("client-echo");
	pair.v.list[2].type = TYPE_INT;
	pair.v.list[2].v.num = h->client_echo;
	list = listappend(list, pair);
	return list;
}

int
network_connection_option(nh, option, value)
	network_handle  nh;
	const char     *option;
	Var            *value;
{
	nhandle        *h = nh.ptr;

	if (!mystrcasecmp(option, "client-echo")) {
		value->type = TYPE_INT;
		value->v.num = h->client_echo;
		return 1;
	}
	return 0;
}

int
network_set_connection_option(nh, option, value)
	network_handle  nh;
	const char     *option;
	Var             value;
{
	nhandle        *h = nh.ptr;

#define TN_IAC	255
#define TN_WILL	251
#define TN_WONT	252
#define TN_ECHO	1
	static char     telnet_cmd[4] = {TN_IAC, 0, TN_ECHO, 0};
	if (!mystrcasecmp(option, "client-echo")) {
		h->client_echo = is_true(value);
		if (h->client_echo)
			telnet_cmd[1] = TN_WONT;
		else
			telnet_cmd[1] = TN_WILL;
		enqueue_output(nh, telnet_cmd, 3, 0, 1);
		return 1;
	}
	return 0;
}

enum error
network_open_connection(arglist)
	Var             arglist;
{
	int             rfd, wfd;
	const char     *local_name, *remote_name;
	enum error      e;

	e = tcp_open(arglist, &rfd, &wfd, &local_name, &remote_name);
	if (e == E_NONE)
		make_new_connection(null_server_listener, rfd, wfd, local_name, remote_name, 1);
	return e;
}

void
network_close(h)
	network_handle  h;
{
	close_nhandle(h.ptr);
}

void
network_close_listener(nl)
	network_listener nl;
{
	close_nlistener(nl.ptr);
}

void
network_shutdown(void)
{
	while (all_nhandles)
		close_nhandle(all_nhandles);
	while (all_nlisteners)
		close_nlistener(all_nlisteners);
}
