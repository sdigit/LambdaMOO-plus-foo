/* $egnet: str_intern.c,v 1.12 2007/09/23 16:27:22 dive Exp $ */

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
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "storage.h"
#include "str_intern.h"
#include "utils.h"

#ifdef STRING_INTERNING

struct intern_entry {
    const char *s;
    size_t len;
    unsigned hash;
};

/*
 * Open addressing is a good fit here because entries are never removed
 * individually: the entire table is discarded when database loading ends.
 * Keeping entries directly in the table also eliminates the separate entry
 * allocation/hunk allocator and avoids pointer chasing during lookup.
 */
static struct intern_entry *intern_table;
static size_t intern_table_size;
static size_t intern_table_count;

static size_t intern_bytes_saved;
static size_t intern_allocations_saved;

#define INTERN_TABLE_SIZE_INITIAL 10007
#define INTERN_LOAD_NUMERATOR 3
#define INTERN_LOAD_DENOMINATOR 4

static size_t normalize_table_size(size_t requested)
{
    size_t size = 16;

    while (size < requested && size <= SIZE_MAX / 2)
        size <<= 1;

    return size;
}

static struct intern_entry *make_intern_table(size_t size)
{
    struct intern_entry *table;

    table = mymalloc(sizeof(*table) * size, M_INTERN_ENTRY);
    memset(table, 0, sizeof(*table) * size);
    return table;
}

static size_t intern_index(unsigned hash, size_t table_size)
{
    return (size_t)hash & (table_size - 1);
}

static void insert_intern_entry(struct intern_entry *table,
                                size_t table_size,
                                const struct intern_entry *entry)
{
    size_t index = intern_index(entry->hash, table_size);

    for (;;) {
        struct intern_entry *slot = &table[index];

        if (slot->s == NULL) {
            *slot = *entry;
            return;
        }

        index = (index + 1) & (table_size - 1);
    }
}

static struct intern_entry *find_interned_string(const char *s,
                                                  size_t len,
                                                  unsigned hash)
{
    size_t index = intern_index(hash, intern_table_size);

    for (;;) {
        struct intern_entry *entry = &intern_table[index];

        if (entry->s == NULL)
            return NULL;

        if (entry->hash == hash && entry->len == len &&
            memcmp(s, entry->s, len) == 0)
            return entry;

        index = (index + 1) & (intern_table_size - 1);
    }
}

static void intern_rehash(size_t new_size)
{
    struct intern_entry *old_table = intern_table;
    size_t old_size = intern_table_size;
    size_t i;

    intern_table = make_intern_table(new_size);
    intern_table_size = new_size;

    for (i = 0; i < old_size; i++) {
        if (old_table[i].s != NULL)
            insert_intern_entry(intern_table, intern_table_size,
                                &old_table[i]);
    }

    myfree(old_table, M_INTERN_ENTRY);
}

void str_intern_open(int table_size)
{
    size_t size;

    if (table_size == 0)
        table_size = INTERN_TABLE_SIZE_INITIAL;

    size = normalize_table_size((size_t)table_size);
    intern_table = make_intern_table(size);
    intern_table_size = size;
    intern_table_count = 0;
    intern_bytes_saved = 0;
    intern_allocations_saved = 0;
}

void str_intern_close(void)
{
    size_t i;
    size_t final_count = intern_table_count;
    size_t final_size = intern_table_size;

    for (i = 0; i < intern_table_size; i++) {
        if (intern_table[i].s != NULL)
            free_str(intern_table[i].s);
    }

    myfree(intern_table, M_INTERN_ENTRY);
    intern_table = NULL;
    intern_table_size = 0;
    intern_table_count = 0;

    oklog("INTERN: %zu allocations saved, %zu bytes\n",
          intern_allocations_saved, intern_bytes_saved);
    oklog("INTERN: at end, %zu entries in a %zu bucket hash table.\n",
          final_count, final_size);
}

/*
 * Make an immutable copy of s.  If there's an intern table open, possibly
 * share storage.
 */
const char *str_intern(const char *s)
{
    struct intern_entry *entry;
    unsigned hash;
    const char *r;
    size_t len;

    if (s == NULL || *s == '\0') {
        /* str_dup already has a canonical empty string */
        return str_dup(s);
    }
    if (intern_table == NULL)
        return str_dup(s);

    len = strlen(s);
    hash = str_hash(s);

    entry = find_interned_string(s, len, hash);
    if (entry != NULL) {
        intern_allocations_saved++;
        intern_bytes_saved += len;
        return str_ref(entry->s);
    }

    if (intern_table_count >=
        (intern_table_size * INTERN_LOAD_NUMERATOR) /
            INTERN_LOAD_DENOMINATOR)
        intern_rehash(intern_table_size * 2);

    r = str_dup(s);
    r = str_ref(r); /* one reference for the intern table */

    {
        struct intern_entry new_entry = {
            .s = r,
            .len = len,
            .hash = hash,
        };
        insert_intern_entry(intern_table, intern_table_size, &new_entry);
    }
    intern_table_count++;

    return r;
}

#else /* STRING_INTERNING */

const char *str_intern(const char *s) { return str_dup(s); }

void str_intern_close(void) { ; }

void str_intern_open(int table_size) { ; }

#endif /* STRING_INTERNING */
