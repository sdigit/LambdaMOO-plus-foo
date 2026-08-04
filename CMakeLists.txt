cmake_minimum_required(VERSION 3.23)
project(LambdaMOO-plus-foo C)

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckFunctionExists)
include(CheckLibraryExists)

check_include_file("sys/filio.h" HAVE_FILIO_H)

check_symbol_exists(strftime "time.h" HAVE_STRFTIME)
check_struct_has_member("struct tm" tm_zone "time.h" HAVE_TM_ZONE)
check_symbol_exists(tzname "time.h" HAVE_TZNAME)

check_symbol_exists(matherr "math.h" HAVE_MATHERR)

check_symbol_exists(random "stdlib.h" HAVE_RANDOM)
check_symbol_exists(lrand48 "stdlib.h" HAVE_LRAND48)

check_symbol_exists(waitpid "sys/types.h;sys/wait.h" HAVE_WAITPID)
check_symbol_exists(wait3 "sys/types.h;sys/wait.h" HAVE_WAIT3)
check_symbol_exists(wait2 "sys/types.h;sys/wait.h" HAVE_WAIT2)

check_symbol_exists(sigemptyset "signal.h" HAVE_SIGEMPTYSET)
check_symbol_exists(sigsetmask "signal.h" HAVE_SIGSETMASK)
check_symbol_exists(sigrelse "signal.h" HAVE_SIGRELSE)

unset(HAVE_CRYPT)
check_function_exists(crypt HAVE_CRYPT_IN_LIBC)
if(HAVE_CRYPT_IN_LIBC)
    set(HAVE_CRYPT TRUE)
else()
    check_library_exists(crypt crypt "" HAVE_CRYPT_IN_LIBCRYPT)
    if(HAVE_CRYPT_IN_LIBCRYPT)
        set(HAVE_CRYPT TRUE)
        set(MOO_CRYPT_LIBRARY crypt)
    else()
        check_library_exists(cryptd crypt "" HAVE_CRYPT_IN_LIBCRYPTD)
        if(HAVE_CRYPT_IN_LIBCRYPTD)
            set(HAVE_CRYPT TRUE)
            set(MOO_CRYPT_LIBRARY cryptd)
        endif()
    endif()
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config.h"
    @ONLY
)

# tokens.h (hand-maintained, not generated) does `#include "y.tab.h"`, and
# most .c files pull in tokens.h transitively. That means the bison step
# below must run, and its header must be named exactly "y.tab.h", before
# ANYTHING in the moo target compiles -- not just before parser.c/keywords.c.
#
# Bison note: when -o is given alongside -d, the header name is derived
# from the -o basename (parser.c -> parser.h), NOT y.tab.h. Force the
# header name explicitly with --defines so this doesn't silently break
# on a bison invoked as "yacc".
find_program(YACC_EXECUTABLE NAMES bison REQUIRED)
find_program(GPERF_EXECUTABLE NAMES gperf REQUIRED)

set(GENERATED_PARSER_C  "${CMAKE_CURRENT_BINARY_DIR}/parser.c")
set(GENERATED_YTAB_H    "${CMAKE_CURRENT_BINARY_DIR}/y.tab.h")
set(GENERATED_KEYWORDS_C "${CMAKE_CURRENT_BINARY_DIR}/keywords.c")

add_custom_command(
    OUTPUT ${GENERATED_PARSER_C} ${GENERATED_YTAB_H}
    COMMAND ${YACC_EXECUTABLE} -d "${CMAKE_CURRENT_SOURCE_DIR}/parser.y"
            -o ${GENERATED_PARSER_C} --defines=${GENERATED_YTAB_H}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/parser.y"
    COMMENT "Generating parser.c and y.tab.h"
    VERBATIM
)

add_custom_command(
    OUTPUT ${GENERATED_KEYWORDS_C}
    COMMAND ${GPERF_EXECUTABLE} -aCIptT -k1,3,\$ "${CMAKE_CURRENT_SOURCE_DIR}/keywords.gperf" --output-file=${GENERATED_KEYWORDS_C}
    # Not a real gperf input, but keywords.c's *compile* needs y.tab.h to
    # exist (via tokens.h), so we order this after it, mirroring the old
    # `keywords.c: keywords.gperf parser.o` Makefile.in hack.
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/keywords.gperf" ${GENERATED_YTAB_H}
    COMMENT "Generating keywords.c"
    VERBATIM
)
add_executable(moo
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ast.h"
    ast.c
    code_gen.c
    db_file.c
    db_io.c
    db_objects.c
    db_properties.c
    db_verbs.c
    decompile.c
    disassemble.c
    eval_env.c
    eval_vm.c
    exceptions.c
    execute.c
    extensions.c
    foomods.c
    functions.c
    ${GENERATED_KEYWORDS_C}
    list.c
    log.c
    match.c
    md5.c
    name_lookup.c
    network.c
    numbers.c
    objects.c
    parse_cmd.c
    ${GENERATED_PARSER_C}
    pattern.c
    program.c
    property.c
    quota.c
    regexpr.c
    server.c
    storage.c
    streams.c
    str_intern.c
    sym_table.c
    tasks.c
    timers.c
    unparse.c
    utils.c
    verbs.c
    version.c 
)
# Listing GENERATED_YTAB_H as an (otherwise-unused) source is the standard
# CMake trick to force a target-level dependency on a generated header that
# no .c file lists as an explicit input -- it's only ever reached via the
# tokens.h quote-include, which CMake's dependency scanner won't see on its
# own for a hand-rolled custom command.
target_sources(moo PRIVATE ${GENERATED_YTAB_H})

target_include_directories(moo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_BINARY_DIR}"
)
target_link_libraries(moo PUBLIC m)
if(MOO_CRYPT_LIBRARY)
    target_link_libraries(moo PUBLIC ${MOO_CRYPT_LIBRARY})
endif()

target_compile_options(moo PRIVATE
    $<$<CONFIG:Debug>: -O0 -g -Wall -Wextra >
    $<$<CONFIG:Build>: -O3 >
    $<$<CONFIG:Release>: -O3 >
)
