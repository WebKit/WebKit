# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Runs the web-platform-tests linter against changed WPT files."""

import logging
import os
import pathlib
import subprocess
import sys

from webkitpy.w3c.wpt_linter import WPTLinter

_log = logging.getLogger(__name__)

WPT_DIR = os.path.join('LayoutTests', 'imported', 'w3c', 'web-platform-tests')


def filter_wpt_paths(paths):
    """Filter a list of paths to only those under the imported WPT directory.

    Returns paths relative to the WPT repo root, using POSIX separators
    (which is what `./wpt lint` expects).
    """
    wpt_root = pathlib.PurePath(WPT_DIR)
    wpt_paths = []
    for path in paths:
        p = pathlib.PurePath(path)
        if p.is_relative_to(wpt_root):
            wpt_paths.append(p.relative_to(wpt_root).as_posix())
    return wpt_paths


def run_wpt_lint(checkout_root, changed_files):
    """Run the WPT linter on any changed WPT files.

    Returns the number of lint errors found.
    """
    wpt_paths = filter_wpt_paths(changed_files)
    if not wpt_paths:
        return 0

    wpt_repo_dir = os.path.join(checkout_root, WPT_DIR)
    if not os.path.isdir(wpt_repo_dir):
        _log.debug("WPT directory not found, skipping WPT lint")
        return 0

    _log.info("Running WPT linter on %d file(s)..." % len(wpt_paths))
    linter = WPTLinter(wpt_repo_dir)
    return_code, output = linter.lint(wpt_paths)

    if output:
        sys.stderr.write(output)
        if not output.endswith('\n'):
            sys.stderr.write('\n')

    if return_code != 0:
        _log.info("WPT linter found errors")

    return return_code
