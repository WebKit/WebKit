#!/bin/sh

USAGE="<commit range> <commit-ish> [commit-ish...]"
LONG_USAGE="COMMIT RANGE
    A commit range is usually specified as <older commitish>..<newer commitish>

COMMIT-ISH
    A commit-ish is anything which can recursively be resolved to a commit; see 'gitglossary(7)'.

EXAMPLE
    git is-in-range HEAD~4..HEAD~2 HEAD~5 HEAD~4 HEAD~3 HEAD~2 HEAD~1
    < HEAD~5
    = HEAD~4
    = HEAD~3
    = HEAD~2
    > HEAD~1
"
SUBDIRECTORY_OK="1"

source "$(git --exec-path)/git-sh-setup"

function _is_in_range() {
    range_start="$1"
    range_end="$2"

    target_commit="$( git rev-parse --verify --quiet $3 )"
    if [[ -z "$target_commit" ]]; then
        die "Commit '$1' is not a valid commit"
    fi

    target_commit_array=($target_commit)
    if [[ "${#target_commit_array[@]}" -ne 1 ]]; then
        die "Commit '$1' does not specify a single commit"
    fi

    # Note that the provided range is inclusive, so decrement the range_end by one to make it exclusive.
    is_ancestor_of_1=$( git merge-base --is-ancestor "$target_commit" "$range_start" ; echo $? )
    is_ancestor_of_2=$( git merge-base --is-ancestor "$target_commit" "$range_end~" ; echo $? )

    if [[ "$is_ancestor_of_1" -ne "$is_ancestor_of_2" ]]; then
        echo "="
    elif [[ "$is_ancestor_of_1" -ne 0 ]]; then
        echo ">"
    else
        echo "<"
    fi
}

if [[ $# -ge "2" ]]; then
    range_commits="$( git rev-parse --verify --quiet $1 )"
    if [[ -z "$range_commits" ]]; then
        die "Range '$2' is not a valid range"
    fi

    range_commits_array=($range_commits)
    if [[ "${#range_commits_array[@]}" != 2 ]]; then
        die "Range '$2' does not specify a range"
    fi

    for target_commit in "${@:2}"; do
        echo $(_is_in_range "${range_commits_array[0]#^}" "${range_commits_array[1]#^}" "$target_commit") "$target_commit"
    done
else
    usage
fi
