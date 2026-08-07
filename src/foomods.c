/* $egnet: foomods.c,v 1.37 2010/07/11 22:08:59 dive Exp $ */

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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "foomods.h"
#include "gitinfo.h"

#include "bf_register.h"
#include "execute.h"
#include "ietf/sha.h"
#include "list.h"
#include "log.h"
#include "network.h"
#include "options.h"
#include "program.h"
#include "storage.h"
#include "structures.h"
#include "utils.h"

#ifdef WRITEPIDFILE

int checkpidfile() {
    struct stat st;

    if (stat(PIDFILE, &st) == 0) { /* PIDFILE exists */
        return 1;
    } else { /* PIDFILE doesn't exist */
        return 0;
    }
}

void writepidfile() {
    int pid;
    int fd;
    char mypid[8];

    if (checkpidfile() == 1)
        oklog("WARNING: PID file %s already exists, overwriting.\n", PIDFILE);
    pid = getpid();
    snprintf(mypid, 8, "%d\n", pid);
    fd = open(PIDFILE, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        oklog("WARNING: unable to open PID file %s: %s\n", PIDFILE,
              strerror(errno));
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

void unlinkpidfile() {
    if (checkpidfile() == 1) {
        if (unlink(PIDFILE) != 0) {
            oklog("WARNING: unlink(%s) did not return zero\n", PIDFILE);
        } else {
            oklog("Removed PID file %s\n", PIDFILE);
        }
    } else {
        oklog("WARNING: checkpidfile() returned 0, where'd the file go?\n");
    }
}

void unlinkpidfile_server_panic() { unlink(PIDFILE); }

#endif /* WRITEPIDFILE */

int read_urandom(void *buf, size_t len) {
    uint8_t *p = buf;
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

        if (n == 0) { /* Should never happen */
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

static package bf_urandom(Var arglist, [[maybe_unused]] Byte next,
                          [[maybe_unused]] void *vdata,
                          [[maybe_unused]] Objid progr) {
    Var ret;
    int n = arglist.v.list[1].v.num;
    int i, idx;
    uint8_t *buf;

    if (n < 1 || n > 4096) /* why do you need > 4096 bytes this is MOO lol just
                              do multiple calls */
    {
        return make_error_pack(E_INVARG);
    }

    buf = malloc(sizeof(uint8_t) * n);
    if (read_urandom(buf, n) != 0) {
        free(buf);
        return make_error_pack(E_RANGE);
    }
    ret = new_list(n);
    idx = 0;
    for (i = 1; i <= n; i++) {
        ret.v.list[i].type = TYPE_INT;
        ret.v.list[i].v.num = buf[idx++];
    }
    free(buf);
    free_var(arglist);
    return make_var_pack(ret);
}

static package bf_build_info(Var arglist, [[maybe_unused]] Byte next,
                             [[maybe_unused]] void *vdata,
                             [[maybe_unused]] Objid progr) {
    Var ret;

    ret = new_list(7);

    ret.v.list[1].type = TYPE_STR;
    ret.v.list[1].v.str = str_dup(BUILT_AT);
    ret.v.list[2].type = TYPE_STR;
    ret.v.list[2].v.str = str_dup(BUILD_HOST_SYSTEM);
    ret.v.list[3].type = TYPE_STR;
    ret.v.list[3].v.str = str_dup(BUILD_HOST_SYSTEM_PROCESSOR);
    ret.v.list[4].type = TYPE_STR;
    ret.v.list[4].v.str = str_dup(C_COMPILER_ID);
    ret.v.list[5].type = TYPE_STR;
    ret.v.list[5].v.str = str_dup(C_COMPILER_VERSION);
    ret.v.list[6].type = TYPE_STR;
    ret.v.list[6].v.str = str_dup(GIT_COMMIT_HASH);
    ret.v.list[7].type = TYPE_STR;
    ret.v.list[7].v.str = str_dup(GIT_BRANCH);

    free_var(arglist);

    return make_var_pack(ret);
}

static char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

char *hex(uint8_t *in, const size_t in_len) {
    uint64_t i = 0, n = 0;
    char *buf;

    buf = calloc(1, (in_len * 2) + 1);
    if (buf == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    while (i < in_len) {
        buf[n] = hexmap[(in[i] & 0xF0) >> 4];
        buf[n + 1] = hexmap[in[i] & 0x0F];
        n += 2;
        i++;
    }
    buf[n] = '\0';
    return buf;
}

static const char *hash_bytes_sha256(const char *input, int length) {
    uint8_t *result;
    char *hex_str;
    SHA256Context ctx;

    result = malloc(32);

    SHA256Reset(&ctx);
    SHA256Input(&ctx, (uint8_t *)input, (unsigned int)length);
    SHA256Result(&ctx, result);
    SHA256Reset(&ctx);

    hex_str = hex(result, 32);
    free(result);
    return hex_str;
}

static const char *hash_bytes_sha384(const char *input, int length) {
    uint8_t *result;
    char *hex_str;
    SHA384Context ctx;

    result = malloc(48);

    SHA384Reset(&ctx);
    SHA384Input(&ctx, (uint8_t *)input, (unsigned int)length);
    SHA384Result(&ctx, result);
    SHA384Reset(&ctx);

    hex_str = hex(result, 48);
    free(result);
    return hex_str;
}

static const char *hash_bytes_sha512(const char *input, int length) {
    uint8_t *result;
    char *hex_str;
    SHA512Context ctx;

    result = malloc(64);

    SHA512Reset(&ctx);
    SHA512Input(&ctx, (uint8_t *)input, (unsigned int)length);
    SHA512Result(&ctx, result);
    SHA512Reset(&ctx);

    hex_str = hex(result, 64);
    free(result);
    return hex_str;
}

static package bf_string_hash_sha256(Var arglist, [[maybe_unused]] Byte next,
                                     [[maybe_unused]] void *vdata,
                                     [[maybe_unused]] Objid progr) {
    Var r;
    const char *str = arglist.v.list[1].v.str;
    char *hexresult;

    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha256(str, strlen(str));
    r.v.str = str_dup(hexresult);
    free(hexresult);
    free_var(arglist);
    return make_var_pack(r);
}

static package bf_string_hash_sha384(Var arglist, [[maybe_unused]] Byte next,
                                     [[maybe_unused]] void *vdata,
                                     [[maybe_unused]] Objid progr) {
    Var r;
    const char *str = arglist.v.list[1].v.str;
    char *hexresult;

    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha384(str, strlen(str));
    r.v.str = str_dup(hexresult);
    free(hexresult);
    free_var(arglist);
    return make_var_pack(r);
}

static package bf_string_hash_sha512(Var arglist, [[maybe_unused]] Byte next,
                                     [[maybe_unused]] void *vdata,
                                     [[maybe_unused]] Objid progr) {
    Var r;
    const char *str = arglist.v.list[1].v.str;
    char *hexresult;

    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha512(str, strlen(str));
    r.v.str = str_dup(hexresult);
    free(hexresult);
    free_var(arglist);
    return make_var_pack(r);
}

static package bf_binary_hash_sha256(Var arglist, [[maybe_unused]] Byte next,
                                     [[maybe_unused]] void *vdata,
                                     [[maybe_unused]] Objid progr) {
    Var r;
    int length;
    const char *bytes = binary_to_raw_bytes(arglist.v.list[1].v.str, &length);
    char *hexresult;
    free_var(arglist);
    if (!bytes)
        return make_error_pack(E_INVARG);
    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha256(bytes, length);
    r.v.str = str_dup(hexresult);
    free(hexresult);
    return make_var_pack(r);
}

static package bf_binary_hash_sha384(Var arglist, [[maybe_unused]] Byte next,
                                     [[maybe_unused]] void *vdata,
                                     [[maybe_unused]] Objid progr) {
    Var r;
    int length;
    const char *bytes = binary_to_raw_bytes(arglist.v.list[1].v.str, &length);
    char *hexresult;

    free_var(arglist);
    if (!bytes)
        return make_error_pack(E_INVARG);
    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha384(bytes, length);
    r.v.str = str_dup(hexresult);
    free(hexresult);
    return make_var_pack(r);
}

static package bf_binary_hash_sha512(Var arglist, [[maybe_unused]] Byte next,
                                     [[maybe_unused]] void *vdata,
                                     [[maybe_unused]] Objid progr) {
    Var r;
    int length;
    const char *bytes = binary_to_raw_bytes(arglist.v.list[1].v.str, &length);
    char *hexresult;

    free_var(arglist);
    if (!bytes)
        return make_error_pack(E_INVARG);
    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha512(bytes, length);
    r.v.str = str_dup(hexresult);
    free(hexresult);
    return make_var_pack(r);
}

static package bf_value_hash_sha256(Var arglist, [[maybe_unused]] Byte next,
                                    [[maybe_unused]] void *vdata,
                                    [[maybe_unused]] Objid progr) {
    Var r;
    const char *lit = value_to_literal(arglist.v.list[1]);
    char *hexresult;

    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha256(lit, strlen(lit));
    r.v.str = str_dup(hexresult);
    free(hexresult);
    free_var(arglist);
    return make_var_pack(r);
}

static package bf_value_hash_sha384(Var arglist, [[maybe_unused]] Byte next,
                                    [[maybe_unused]] void *vdata,
                                    [[maybe_unused]] Objid progr) {
    Var r;
    const char *lit = value_to_literal(arglist.v.list[1]);
    char *hexresult;

    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha384(lit, strlen(lit));
    r.v.str = str_dup(hexresult);
    free(hexresult);
    free_var(arglist);
    return make_var_pack(r);
}

static package bf_value_hash_sha512(Var arglist, [[maybe_unused]] Byte next,
                                    [[maybe_unused]] void *vdata,
                                    [[maybe_unused]] Objid progr) {
    Var r;
    const char *lit = value_to_literal(arglist.v.list[1]);
    char *hexresult;

    r.type = TYPE_STR;
    hexresult = (char *)hash_bytes_sha512(lit, strlen(lit));
    r.v.str = str_dup(hexresult);
    free(hexresult);
    free_var(arglist);
    return make_var_pack(r);
}

void register_foomods() {
    register_function("urandom", 1, 1, bf_urandom, TYPE_INT);
    register_function("build_info", 0, 0, bf_build_info);
    register_function("string_hash_sha256", 1, 1, bf_string_hash_sha256,
                      TYPE_STR);
    register_function("string_hash_sha384", 1, 1, bf_string_hash_sha384,
                      TYPE_STR);
    register_function("string_hash_sha512", 1, 1, bf_string_hash_sha512,
                      TYPE_STR);
    register_function("value_hash_sha256", 1, 1, bf_value_hash_sha256,
                      TYPE_ANY);
    register_function("value_hash_sha384", 1, 1, bf_value_hash_sha384,
                      TYPE_ANY);
    register_function("value_hash_sha512", 1, 1, bf_value_hash_sha512,
                      TYPE_ANY);
    register_function("binary_hash_sha256", 1, 1, bf_binary_hash_sha256,
                      TYPE_STR);
    register_function("binary_hash_sha384", 1, 1, bf_binary_hash_sha384,
                      TYPE_STR);
    register_function("binary_hash_sha512", 1, 1, bf_binary_hash_sha512,
                      TYPE_STR);
}
