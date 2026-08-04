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
		if (write(fd, mypid, strlen(mypid)) != (ssize_t)strlen(mypid)) {
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
unlinkpidfile_server_panic()
{
	unlink(PIDFILE);
}

#endif				/* WRITEPIDFILE */

int
read_urandom(void *buf, size_t len)
{
    char *p = buf;
    int fd;

    fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return -1;

    while (len > 0) {
        ssize_t n = read(fd, p, len);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            close(fd);
            return -1;
        }

        if (n == 0) {      /* Should never happen */
            close(fd);
            errno = EIO;
            return -1;
        }

        p += n;
        len -= (size_t)n;
    }

    close(fd);
    return 0;
}

static package
bf_urandom(Var arglist, [[maybe_unused]] Byte next, [[maybe_unused]] void *vdata, [[maybe_unused]] Objid progr)
{
    Var ret;
    int n = arglist.v.list[1].v.num;
    int i,idx;
    char *buf;

    if (n < 1)
    {
        return make_error_pack(E_INVARG);
    }

    ret = new_list(n);
    buf = malloc(sizeof(char)*n);
    idx = 0;
    if (read_urandom(buf,n) != 0)
    {
        return make_error_pack(E_RANGE);
    }
    for (i=1;i<=n;i++)
    {
        ret.v.list[i].type = TYPE_INT;
        ret.v.list[i].v.num = buf[idx++];
    }
    free(buf);
    free_var(arglist);
    return make_var_pack(ret);
}


void
register_foomods()
{
	register_function("urandom",1,1,bf_urandom,TYPE_INT);
}

