cmake_minimum_required(VERSION 3.23)
project(LambdaMOO-plus-foo)
add_custom_command(
    OUTPUT keywords.c
    COMMAND gperf -aCIptT -k1,3,\$ "${CMAKE_CURRENT_SOURCE_DIR}/keywords.gperf" --output-file=keywords.c
    COMMENT "Generating keywords.c"
    VERBATIM
)
add_custom_command(
    OUTPUT parser.c
    COMMAND yacc -d ${CMAKE_CURRENT_SOURCE_DIR}/parser.y -o parser.c
    COMMENT "Generating parser.c"
    VERBATIM
)
add_executable(moo
    ast.h
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
    keywords.c
    list.c
    log.c
    match.c
    md5.c
    name_lookup.c
    network.c
    numbers.c
    objects.c
    parse_cmd.c
    parser.c
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
set_source_files_properties(keywords.c PROPERTIES GENERATED TRUE)
set_source_files_properties(parser.c PROPERTIES GENERATED TRUE)
target_include_directories(moo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(moo PUBLIC m)
target_link_libraries(moo PUBLIC crypt)
