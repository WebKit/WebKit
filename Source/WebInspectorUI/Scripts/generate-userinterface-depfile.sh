#!/bin/sh
#
# Writes the dependency file for the "Copy User Interface Resources" phase,
# the input list Xcode checks to decide whether that phase needs to rerun.
#
# Lists directories as well as files: a directory's mtime catches add/remove/rename,
# a file's only catches in-place edits (on APFS a dir's mtime doesn't move on those).
# Don't narrow this to files.

set -e

depfile="$1"
if [ -z "${depfile}" ]; then
    echo "usage: $(basename "$0") <output-depfile>" >&2
    exit 1
fi

mkdir -p "$(dirname "${depfile}")"

tmp="$(mktemp "${depfile}.XXXXXX")"
trap 'rm -f "${tmp}"' EXIT

# Emit Make depfile format: a "dependencies:" target, one prerequisite per line.
find "${SRCROOT}/UserInterface" -name '.*' -prune -o -print \
    | LC_ALL=C sort \
    | awk '
        BEGIN { printf "dependencies:" }
        { gsub(/[ \\]/, "\\\\&"); printf " \\\n    %s", $0 }
        END { printf "\n" }
    ' > "${tmp}"

mv "${tmp}" "${depfile}"
