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

"""Checks that C++ files and their Swift reimplementations are kept in sync."""

import logging
import os

from webkitpy.common.host import Host
from webkitpy.style.error_handlers import DefaultStyleErrorHandler


_log = logging.getLogger(__name__)

# Pairs of files which reimplement each other and must be kept in sync. When one
# member of a pair is changed in a patch, the other member is expected to change
# too. Paths are relative to the root of the repository.
PAIRED_FILES = [
    ('Source/WebKit/UIProcess/WebBackForwardList.cpp',
     'Source/WebKit/UIProcess/WebBackForwardList.swift'),
]


class SwiftAssociationChecker(object):
    """Flags patches that modify one file of a C++/Swift pair but not the other."""

    categories = set(['swift/association'])

    @staticmethod
    def check_associations(files, configuration, cwd, increment_error_count=lambda: 0, host=Host()):
        """Report an error for each paired file changed without its counterpart.

        Args:
            files: A dictionary mapping absolute file paths to the lists of lines
                modified in each file. Removed files map to None.
            configuration: A StyleProcessorConfiguration instance.
            cwd: The root of the SCM checkout, used to relativize the file paths.
            increment_error_count: Callable invoked once per reported error.
            host: The current host (for testing).
        """
        # A file is considered changed if it appears in the dictionary at all,
        # whether it was modified (maps to a list of lines) or removed (maps to None).
        changed_paths = set(os.path.relpath(abs_path, cwd) for abs_path in files)

        for first, second in PAIRED_FILES:
            for changed_path, counterpart in ((first, second), (second, first)):
                if changed_path not in changed_paths or counterpart in changed_paths:
                    continue

                absolute_path = os.path.join(cwd, changed_path)
                # Report at line 0 so the error is emitted regardless of which
                # lines of the file the patch happened to touch.
                style_error_handler = DefaultStyleErrorHandler(
                    absolute_path, configuration, increment_error_count, files.get(absolute_path))
                style_error_handler(
                    0,
                    'swift/association',
                    5,
                    '{} was modified but its counterpart {} was not. These files '
                    'reimplement each other and must be kept in sync.'.format(changed_path, counterpart))
