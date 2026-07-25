/* $egnet: foomods.c,v 1.37 2010/07/11 22:08:59 dive Exp $ */

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

#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "foomods.h"

#include "execute.h"
#include "log.h"
#include "options.h"
#include "structures.h"
#include "list.h"
#include "program.h"
#include "bf_register.h"
#include "utils.h"
#include "network.h"

#ifdef CMDLOG

struct cl_ent *cl_top;

char           *
cmdlog_time()
{
	struct tm      *t_tm;
	time_t          t_time;
	char           *strbuf;

	if ((strbuf = (char *) malloc(64)) == NULL) {
		errlog("malloc(64): %s\n", strerror(errno));
		return (NULL);
	}
	t_time = time(NULL);
	t_tm = localtime(&t_time);
	if (strftime(strbuf, 64, "%Y/%m/%d %H:%M:%S", t_tm) == 0) {
		errlog("strftime returned 0.\n");
		return (NULL);
	}
	return (strbuf);
}

void
cmdlog_add(player)
	int player;
{
	struct cl_ent *c;

	if (!is_connected(player)) {
		oklog("Tried to activate command logging on an object with no connection");
		return;
	}

	c = cl_top;

	if (c == NULL) {
		c = (struct cl_ent *)malloc(sizeof(struct cl_ent));
		if (c == NULL) {
			errlog("malloc %d: %s\n",
				sizeof(struct cl_ent), strerror(errno));
			panic("cmdlog malloc fail\n");
		}
		memset(c,0,sizeof(struct cl_ent));
		cl_top = c;
	} else {
		while (c->next != NULL)
			c = c->next;
		c->next = (struct cl_ent *)malloc(sizeof(struct cl_ent));
		if (c->next == NULL) {
			panic("cmdlog malloc fail\n");
			return;
		}

		c = c->next;
	}
	oklog("created new cl_ent node at %p for %d\n",c,player);
	c->player = player;
	c->logging = 0;
	c->next = NULL;			
}

void
cmdlog_del(player)
	int player;
{
	struct cl_ent *m, *p;

	m = cl_top;
	p = NULL;

	while (m != NULL) {
		if (m->player == player) {
			oklog("destroying cl_ent node at %p for %d\n",m,m->player);

			if (p != NULL)
				p->next = m->next;

			if (m == cl_top)
				cl_top = m->next;

			free(m);
			return;
		}
		p = m;
		m = m->next;
	}
}

int
cmdlog_logging(player)
	int player;
{
	struct cl_ent *c;

	c = cl_top;

	if (player < 0) {
#ifdef CMDLOG_LOG_NEGATIVE
		return(1);
#else /* CMDLOG_LOG_NEGATIVE */
		return 0;
#endif /* CMDLOG_LOG_NEGATIVE */
	}

  oklog("cmdlog_logging(%d):\n",player);
	oklog("cl_top     @ %p\n",cl_top);
  oklog("c          @ %p\n",c);
	while (c != NULL) {
	 	oklog("c->player  = %d\n",c->player);
	 	oklog("c->logging = %d\n",c->logging);
	 	oklog("c->next    = %p\n",c->next);
		if (c->player == player) {
			oklog("found player (%d=%d) at node %p. logging = %d\n",
				player,c->player,c,c->logging);
			return c->logging;
		}
		c = c->next;
	}
	return(0);
}

void
cmdlog_activate(player)
	int player;
{
	struct cl_ent *c;

	c = cl_top;
	while (c != NULL) {
		if (c->player == player) {
			c->logging = 1;
			return;
		}
		c = c->next;
	}
}

void
cmdlog_deactivate(player)
	int player;
{
	struct cl_ent *c;

	c = cl_top;
	while (c != NULL) {
		if (c->player == player) {
			c->logging = 0;
			return;
		}
		c = c->next;
	}
}

void
cmdlog_clear()
{
	struct cl_ent *c,*next;

	c = cl_top;
	if (c == NULL)
		return;
	while (c != NULL) {
		next = c->next;
		free(c);
		c = next;
	}
}
		
void
print_cmdlog(player, line)
	int             player;
	const char	*line;
{
	char log_name[PATH_MAX];
	FILE *logfile;

	snprintf(log_name, PATH_MAX, "%sobj%d.log", CMDLOG_DIR, player);
	if ((logfile = fopen(log_name, "a")) == NULL) {
		errlog("fopen %s failed: %s\n", log_name, strerror(errno));
		return;
	}
	if (chmod(log_name, S_IRUSR | S_IWUSR) != 0) {
		errlog("chmod 600 %s: %s\n", log_name, strerror(errno));
		fclose(logfile);
		return;
	}
	if (strlen(line) > 0) {
		char *logline;
		char *timestr;
		size_t sz;

		timestr = cmdlog_time();
		sz = 16; /* slush factor */
		sz += strlen(timestr);
		sz += strlen(line);
		sz++;
		
		if ((logline = (char *)malloc(sz)) == NULL) {
			errlog("malloc %d: %s\n", sz, strerror(errno));
			fclose(logfile);
			free(timestr);
			return;
		}
		snprintf(logline, sz, "%s #%d: ", timestr, player);
		strncat(logline, line, sz);
		strncat(logline, "\n", sz);
		logline[sz-1]=0;
		fputs(logline, logfile);
		fflush(logfile);
		fclose(logfile);
		free(timestr);
		free(logline);
		return;
	}
}

static package
bf_cmdlog_activate(Var arglist, Byte next, void *vdata, Objid progr)
{
	Objid oid = arglist.v.list[1].v.obj;

	free_var(arglist);
	if (!is_wizard(progr))
		return make_error_pack(E_PERM);
	if (!valid(oid))
		return make_error_pack(E_INVARG);
	if (is_user(oid)) {
		oklog("CMDLOG activated on #%d by #%d\n", oid, progr);
		cmdlog_activate(oid);
	}
	return no_var_pack();
}
			
static package
bf_cmdlog_deactivate(Var arglist, Byte next, void *vdata, Objid progr)
{
	Objid oid = arglist.v.list[1].v.obj;

	free_var(arglist);
	if (!is_wizard(progr))
		return make_error_pack(E_PERM);
	if (!valid(oid))
		return make_error_pack(E_INVARG);
	if (is_user(oid)) {
		oklog("CMDLOG deactivated on #%d by #%d\n", oid, progr);
		cmdlog_deactivate(oid);
	}
	return no_var_pack();
}

static package
bf_cmdlog_list(Var arglist, Byte next, void *vdata, Objid progr)
{
	Var ret;
	struct cl_ent *c;

	free_var(arglist);
	if (!is_wizard(progr))
		return make_error_pack(E_PERM);

	c = cl_top;

	ret.type = TYPE_LIST;
	ret = new_list(0);
	while (c != NULL) {
		Var z;
		z.type = TYPE_LIST;
		z = new_list(2);
		z.v.list[1].type = TYPE_OBJ;
		z.v.list[1].v.obj  = c->player;
		z.v.list[2].type = TYPE_INT;
		z.v.list[2].v.num  = c->logging;
		ret = listappend(ret,z);
		c = c->next;
	}
	return make_var_pack(ret);
}

#endif				/* CMDLOG */

#ifdef WRITEPIDFILE

int
checkpidfile()
{
	struct stat     st;

	if (stat(PIDFILE, &st) == 0) {	/* PIDFILE exists */
		return 1;
	} else {		/* PIDFILE doesn't exist */
		return 0;
	}
}

void
writepidfile()
{
	int             pid;
	int             fd;
	char            mypid[8];

	if (checkpidfile() == 1)
		oklog("WARNING: PID file %s already exists, overwriting.\n",
		      PIDFILE);
	pid = getpid();
	snprintf(mypid, 8, "%d\n", pid);
	fd = open(PIDFILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		oklog("WARNING: unable to open PID file %s: %s\n",
		      PIDFILE, strerror(errno));
	} else {
		if (write(fd, mypid, strlen(mypid)) != strlen(mypid)) {
			oklog("WARNING: write didn't return the correct length?");
		} else {
			oklog("Wrote PID (%d) to PID file %s\n", pid, PIDFILE);
			close(fd);
		}
	}
	return;
}

void
unlinkpidfile()
{
	if (checkpidfile() == 1) {
		if (unlink(PIDFILE) != 0) {
			oklog("WARNING: unlink(%s) did not return zero\n",
			      PIDFILE);
		} else {
			oklog("Removed PID file %s\n", PIDFILE);
		}
	} else {
		oklog("WARNING: checkpidfile() returned 0, where'd the file go?\n");
	}
}

void
unlinkpidfile_panic()
{
	unlink(PIDFILE);
}

#endif				/* WRITEPIDFILE */

void
register_foomods()
{
#ifdef CMDLOG
	register_function("cmdlog_activate", 1, 1, bf_cmdlog_activate, TYPE_OBJ);
	register_function("cmdlog_deactivate", 1, 1, bf_cmdlog_deactivate, TYPE_OBJ);
	register_function("cmdlog_list", 0, 0, bf_cmdlog_list);
#endif				/* CMDLOG */
}

