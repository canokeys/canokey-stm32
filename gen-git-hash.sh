#!/bin/bash
echo -n '#define GIT_COMMIT_HASH ' >git_hash.c
git describe --always --tags --long --abbrev=8 --dirty >>git_hash.c || echo non-git-repo >>git_hash.c
(cat <<-EOF
#define XSTR(s) #s
#define XSTR_MACRO(s) XSTR(s)
const char *fw_git_version = XSTR_MACRO(GIT_COMMIT_HASH);
EOF
) >>git_hash.c
