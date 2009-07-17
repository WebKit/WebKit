# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Command line completion for common commands used in WebKit development.
#
# Set-up:
#   Add a line like this to your .bashrc:
#     source /path/to/WebKitCode/WebKitTools/Scripts/webkit-tools-completion.sh

__bugzilla-tool_generate_reply()
{
    COMPREPLY=( $(compgen -W "$1" -- "${COMP_WORDS[COMP_CWORD]}") )
}

_bugzilla-tool_complete()
{
    local command current_command="${COMP_WORDS[1]}"
    case "$current_command" in
        -h|--help)
            command="help";
            ;;
        *)
            command="$current_command"
            ;;
    esac

    if [ $COMP_CWORD -eq 1 ]; then
        __bugzilla-tool_generate_reply "--help bugs-to-commit patches-to-commit reviewed-patches apply-patches land-diff land-patches commit-message obsolete-attachments post-diff post-commits"
        return
    fi

    case "$command" in
        apply-patches)
            __bugzilla-tool_generate_reply "--no-update --force-clean --no-clean --local-commit"
            return
            ;;
        land-diff)
            __bugzilla-tool_generate_reply "-r --reviewer --no-close --no-build --no-test"
            return
            ;;
        land-patches)
            __bugzilla-tool_generate_reply "--force-clean --no-clean --no-build --no-test"
            return
            ;;
        commit-message|obsolete-attachments)
            return
            ;;
        post-diff)
            __bugzilla-tool_generate_reply "--no-obsolete --no-review -m --description"
            return
            ;;
        post-commits)
            __bugzilla-tool_generate_reply "-b --bug-id= --no-comment --no-obsolete --no-review"
            return
            ;;
    esac
}

complete -F _bugzilla-tool_complete bugzilla-tool
complete -W "-c --continue --no-continue -f --fix-merged -h --help -w --warnings --no-warnings" resolve-ChangeLogs
complete -W "--bug -d --dif --git-commit --git-index --git-reviewer -h --help -o --open --no-update --update" prepare-ChangeLog
complete -W "--debug -clean --help -h" build-webkit
complete -W "--add-platform-exceptions --complex-text -c --configuration -g --guard-malloc --help -h -i --ignore-tests --no-http --no-launch-safari --no-new-test-results --http --launch-safari --new-test-results -l --leaks -p --pixel-tests --tolerance --platform --port -q --quiet --reset-results -o --results-directory --random --reverse --root --no-sample-on-timeout --sample-on-timeout -1 --singly --skipped --slowest --strict --no-strip-editing-callbacks --strip-editing-callbacks -t --threaded --timeout --valgrind -v --verbose -m --merge-leak-depth --use-remote-links-to-tests" run-webkit-tests
