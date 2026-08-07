/* $egnet: match.c,v 1.8 2007/09/23 16:27:21 dive Exp $ */

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

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "db.h"
#include "exceptions.h"
#include "match.h"
#include "parse_cmd.h"
#include "storage.h"
#include "structures.h"
#include "unparse.h"
#include "utils.h"

static Var *aliases(Objid oid) {
    Var value;
    db_prop_handle h;

    h = db_find_property(oid, "aliases", &value);
    if (!h.ptr || value.type != TYPE_LIST) {
        /* Simulate a pointer to an empty list */
        return &zero;
    } else
        return value.v.list;
}

struct match_data {
    int lname;
    const char *name;
    Objid exact, partial;
};

static int match_proc(void *data, Objid oid) {
    struct match_data *d = data;
    Var *names = aliases(oid);
    int i;
    const char *name;

    for (i = 0; i <= names[0].v.num; i++) {
        if (i == 0)
            name = db_object_name(oid);
        else if (names[i].type != TYPE_STR)
            continue;
        else
            name = names[i].v.str;

        if (!mystrncasecmp(name, d->name, d->lname)) {
            if (name[d->lname] == '\0') { /* exact match */
                if (d->exact == NOTHING || d->exact == oid)
                    d->exact = oid;
                else
                    return 1;
            } else { /* partial match */
                if (d->partial == FAILED_MATCH || d->partial == oid)
                    d->partial = oid;
                else
                    d->partial = AMBIGUOUS;
            }
        }
    }

    return 0;
}

static Objid match_contents(Objid player, const char *name) {
    Objid loc;
    int step;
    Objid oid;
    struct match_data d;

    d.lname = strlen(name);
    d.name = name;
    d.exact = NOTHING;
    d.partial = FAILED_MATCH;

    if (!valid(player))
        return FAILED_MATCH;
    loc = db_object_location(player);

    for (oid = player, step = 0; step < 2; oid = loc, step++) {
        if (!valid(oid))
            continue;
        if (db_for_all_contents(oid, match_proc, &d))
            /*
             * We only abort the enumeration for exact ambiguous
             * matches...
             */
            return AMBIGUOUS;
    }

    if (d.exact != NOTHING)
        return d.exact;
    else
        return d.partial;
}

Objid match_object(Objid player, const char *name) {
    if (name[0] == '\0')
        return NOTHING;
    if (name[0] == '#') {
        char *p;
        Objid r = strtol(name + 1, &p, 10);

        if (*p != '\0' || !valid(r))
            return FAILED_MATCH;
        return r;
    }
    if (!valid(player))
        return FAILED_MATCH;
    if (!mystrcasecmp(name, "me"))
        return player;
    if (!mystrcasecmp(name, "here"))
        return db_object_location(player);
    return match_contents(player, name);
}
