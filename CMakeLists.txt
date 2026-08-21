cmake_minimum_required(VERSION 3.23)
project(LambdaMOO-plus-foo C)

# Single-config generators (Makefiles, Ninja) leave CMAKE_BUILD_TYPE empty
# unless set explicitly. An empty CONFIG matches none of the
# $<CONFIG:Debug/Build/Release> branches below, which silently produced an
# unoptimized, unhardened, warning-free build. Default to Build (our
# hardened dev/CI config) and restrict/document the allowed values.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Build CACHE STRING "Build type" FORCE)
endif()
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Build Release)

set(GENERATED_GIT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/gitinfo.h")

add_custom_target(UpdateGitHeader ALL
    COMMAND ${CMAKE_COMMAND}
        -DTARGET_HEADER_PATH=${GENERATED_GIT_HEADER}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/githeader.cmake"
    BYPRODUCTS ${GENERATED_GIT_HEADER}
)

include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(CheckFunctionExists)
include(CheckLibraryExists)

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

if(NOT HAVE_CRYPT)
    message(FATAL_ERROR "You really need crypt for this code to work.")
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

# Locate GNU Bison and enforce a minimum version of 3.8.2
find_program(BISON_EXECUTABLE
    NAMES bison
    HINTS /opt/homebrew/bin /usr/local/bin /opt/local/bin
)

if(NOT BISON_EXECUTABLE)
    message(FATAL_ERROR "bison not found: GNU Bison >= 3.8.2 is required")
endif()

execute_process(
    COMMAND "${BISON_EXECUTABLE}" --version
    OUTPUT_VARIABLE BISON_VERSION_RAW
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REGEX MATCH "([0-9]+)\\.([0-9]+)(\\.([0-9]+))?" BISON_VERSION_STRING "${BISON_VERSION_RAW}")

if(NOT BISON_VERSION_STRING)
    message(FATAL_ERROR "Unable to determine bison version from: ${BISON_VERSION_RAW}")
endif()

if(BISON_VERSION_STRING VERSION_LESS "3.8.2")
    message(FATAL_ERROR "bison version ${BISON_VERSION_STRING} found, but >= 3.8.2 is required")
endif()

message(STATUS "Found bison: ${BISON_EXECUTABLE} (version ${BISON_VERSION_STRING})")

find_program(GPERF_EXECUTABLE NAMES gperf
    HINTS "/opt/homebrew/bin" "/usr/local/bin" "/opt/local/bin"
    REQUIRED
)

execute_process(
    COMMAND ${GPERF_EXECUTABLE} --version
    OUTPUT_VARIABLE GPERF_VERSION_OUTPUT
)

if(GPERF_VERSION_OUTPUT MATCHES "gperf ([0-9]+\\.[0-9]+)")
    set(GPERF_VERSION "${CMAKE_MATCH_1}")
endif()

if(GPERF_VERSION VERSION_LESS "3.1")
    message(FATAL_ERROR
        "Found ancient gperf version ${GPERF_VERSION} at ${GPERF_EXECUTABLE}.\n"
        "Please install modern GNU gperf: 'brew install gperf'"
    )
endif()

message(STATUS "Found gperf: ${GPERF_EXECUTABLE} (version ${GPERF_VERSION})")

set(GENERATED_PARSER_C  "${CMAKE_CURRENT_BINARY_DIR}/parser.c")
set(GENERATED_YTAB_H    "${CMAKE_CURRENT_BINARY_DIR}/y.tab.h")
set(GENERATED_KEYWORDS_C "${CMAKE_CURRENT_BINARY_DIR}/keywords.c")

add_custom_command(
    OUTPUT ${GENERATED_PARSER_C} ${GENERATED_YTAB_H}
    COMMAND ${BISON_EXECUTABLE} -d "${CMAKE_CURRENT_SOURCE_DIR}/templates/parser.y"
            -o ${GENERATED_PARSER_C} --defines=${GENERATED_YTAB_H}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/templates/parser.y"
    COMMENT "Generating parser.c and y.tab.h"
    VERBATIM
)

add_custom_command(
    OUTPUT ${GENERATED_KEYWORDS_C}
    COMMAND ${GPERF_EXECUTABLE} -CIptT -k1,3,\$ "${CMAKE_CURRENT_SOURCE_DIR}/templates/keywords.gperf" --output-file=${GENERATED_KEYWORDS_C}
    # Not a real gperf input, but keywords.c's *compile* needs y.tab.h to
    # exist (via tokens.h), so we order this after it, mirroring the old
    # `keywords.c: keywords.gperf parser.o` Makefile.in hack.
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/templates/keywords.gperf" ${GENERATED_YTAB_H}
    COMMENT "Generating keywords.c"
    VERBATIM
)

string(TIMESTAMP BUILT_AT "%Y%m%d-%H:%M:%S")

# LTO isn't universally available (older cross toolchains, some
# NetBSD/sparc64 base-system compilers lack a working LTO plugin), and it
# actively hurts Debug (inlining destroys stack traces). Check support and
# enable only for Build/Release, on the target rather than globally.
include(CheckIPOSupported)
check_ipo_supported(RESULT MOO_IPO_SUPPORTED OUTPUT MOO_IPO_ERROR)
if(NOT MOO_IPO_SUPPORTED)
    message(STATUS "IPO/LTO not supported by this toolchain: ${MOO_IPO_ERROR}")
endif()

add_executable(moo
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ast.h"
    src/ast.c
    src/code_gen.c
    src/db_file.c
    src/db_io.c
    src/db_objects.c
    src/db_properties.c
    src/db_verbs.c
    src/decompile.c
    src/disassemble.c
    src/eval_env.c
    src/eval_vm.c
    src/exceptions.c
    src/execute.c
    src/extensions.c
    src/foomods.c
    src/functions.c
    ${GENERATED_KEYWORDS_C}
    src/list.c
    src/log.c
    src/match.c
    src/md5.c
    src/name_lookup.c
    src/network.c
    src/numbers.c
    src/objects.c
    src/parse_cmd.c
    ${GENERATED_PARSER_C}
    src/pattern.c
    src/program.c
    src/property.c
    src/quota.c
    src/regexpr.c
    src/server.c
    src/sha256.c
    src/sha384-512.c
    src/storage.c
    src/streams.c
    src/str_intern.c
    src/sym_table.c
    src/tasks.c
    src/timers.c
    src/unparse.c
    src/utils.c
    src/verbs.c
    src/version.c
)

if(MOO_IPO_SUPPORTED)
    set_property(TARGET moo PROPERTY INTERPROCEDURAL_OPTIMIZATION_BUILD ON)
    set_property(TARGET moo PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()

add_dependencies(moo UpdateGitHeader)

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
    -Wall -Wextra
    $<$<CONFIG:Debug>: -O0 -g >
    $<$<CONFIG:Build>:
        -O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong
        -Wformat -Wformat-security
    >
    $<$<CONFIG:Release>:
        -O3 -D_FORTIFY_SOURCE=2 -fstack-protector-strong
        -Wformat -Wformat-security
    >
)

target_compile_definitions(moo PRIVATE
    BUILT_AT="${BUILT_AT}"
    BUILD_HOST_SYSTEM="${CMAKE_HOST_SYSTEM}"
    BUILD_HOST_SYSTEM_PROCESSOR="${CMAKE_HOST_SYSTEM_PROCESSOR}"
    C_COMPILER="${CMAKE_C_COMPILER}"
    C_COMPILER_ID="${CMAKE_C_COMPILER_ID}"
    C_COMPILER_VERSION="${CMAKE_C_COMPILER_VERSION}"
)

target_compile_features(moo PUBLIC c_std_23)
