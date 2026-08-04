execute_process(
    COMMAND git rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Get the current branch name
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
file(WRITE "${TARGET_HEADER_PATH}" "
#ifndef GITINFO_H
#define GITINFO_H

#define GIT_COMMIT_HASH \"${GIT_COMMIT_HASH}\"
#define GIT_BRANCH      \"${GIT_BRANCH}\"

#endif
")

