# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

from optparse import make_option


class Options(object):
    architecture = make_option("--architecture", action="store", type="string", dest="architecture", default=None, help="Target architecture")
    blocks = make_option("--blocks", action="store", type="string", dest="blocks", default=None, help="Bug number which the created bug blocks.")
    build = make_option("--build", action="store_true", dest="build", default=False, help="Build and run run-webkit-tests before committing.")
    build_style = make_option("--build-style", action="store", dest="build_style", default=None, help="Whether to build debug, release, or both.")
    cc = make_option("--cc", action="store", type="string", dest="cc", help="Comma-separated list of email addresses to carbon-copy.")
    cc_radar = make_option("--cc-radar", action="store_true", dest="cc_radar", default=False, help="Add webkit-bug-importer/Radar to the CC list.")
    check_style = make_option("--ignore-style", action="store_false", dest="check_style", default=True, help="Don't check to see if the patch has proper style before uploading.")
    check_style_filter = make_option("--check-style-filter", action="store", type="string", dest="check_style_filter", default=None, help="Filter style-checker rules (see check-webkit-style --help).")
    clean = make_option("--no-clean", action="store_false", dest="clean", default=True, help="Don't check if the working directory is clean before applying patches")
    component = make_option("--component", action="store", type="string", dest="component", help="Component for the new bug.")
    confirm = make_option("--no-confirm", action="store_false", dest="confirm", default=True, help="Skip confirmation steps.")
    force_clean = make_option("--force-clean", action="store_true", dest="force_clean", default=False, help="Clean working directory before applying patches (removes local changes and commits)")
    git_commit = make_option("-g", "--git-commit", action="store", dest="git_commit", help="Operate on a local commit. If a range, the commits are squashed into one. <ref>.... includes the working copy changes. UPSTREAM can be used for the upstream/tracking branch.")
    group = make_option("--group", action="store", dest="group", default=None, help="")
    non_interactive = make_option("--non-interactive", action="store_true", dest="non_interactive", default=False, help="Never prompt the user, fail as fast as possible.")
    parent_command = make_option("--parent-command", action="store", dest="parent_command", default=None, help="(Internal) The command that spawned this instance.")
    quiet = make_option("--quiet", action="store_true", dest="quiet", default=False, help="Produce less console output.")
    sort_xcode_project = make_option("--no-sort-xcode-project", action="store_false", dest="sort_xcode_project", default=True, help="Don't sort modified xcode projects.")
    update = make_option("--no-update", action="store_false", dest="update", default=True, help="Don't update the working directory.")
    format_cpp_files = make_option("--no-format-cpp-files", action="store_false", dest="format_cpp_files", default=True, help="Skip formatting of C/C++ files.")
