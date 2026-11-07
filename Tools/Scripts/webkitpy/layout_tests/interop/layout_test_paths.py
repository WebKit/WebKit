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

"""Map the WPT tests in an Interop focus area onto imported LayoutTests paths."""

import logging
import re

_log = logging.getLogger(__name__)

IMPORTED_WPT_DIR = "imported/w3c/web-platform-tests"

# The part of a test URL which isn't part of the file name on disk.
_variant_re = re.compile(r"[?#]")


def layout_test_path_for_wpt_test(wpt_test):
    """Convert a WPT test URL into a LayoutTests-relative test path.

    e.g. "/css/css-scroll-snap/snap-area.html?basic" becomes
    "imported/w3c/web-platform-tests/css/css-scroll-snap/snap-area.html?basic".

    :param str wpt_test: a WPT test URL, or a directory of tests ending in "/"
    :return str: the test path, relative to the LayoutTests directory
    """
    return "{}/{}".format(IMPORTED_WPT_DIR, wpt_test.lstrip("/").rstrip("/"))


def imported_layout_test_paths(fs, layout_tests_dir, wpt_tests):
    """Filter WPT tests down to the ones imported into LayoutTests.

    Interop covers all of WPT, but WebKit only imports some of it, so tests
    which aren't in this checkout are of no use to run-webkit-tests.

    :param FileSystem fs: the current filesystem object
    :param str layout_tests_dir: the LayoutTests directory (see: Port.layout_tests_dir())
    :param Iterable[str] wpt_tests: WPT test URLs (see: find_wpt_tests())
    :return Tuple[List[str], List[str]]: the LayoutTests-relative paths of the
        imported tests, and the URLs of the tests which aren't imported
    """
    paths = []
    not_imported = []

    for wpt_test in wpt_tests:
        path = layout_test_path_for_wpt_test(wpt_test)
        # Variants ("?...") and fragments ("#...") aren't part of the file name.
        file_path = _variant_re.split(path, 1)[0]
        if fs.exists(fs.join(layout_tests_dir, *file_path.split("/"))):
            paths.append(path)
        else:
            not_imported.append(wpt_test)

    return paths, not_imported
