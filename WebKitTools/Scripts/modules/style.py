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

"""Front end of some style-checker modules."""

# FIXME: Move more code from cpp_style to here.
# check-webkit-style should not refer cpp_style directly.

import os.path

import modules.cpp_style as cpp_style
import modules.text_style as text_style
from modules.diff_parser import DiffParser


def use_webkit_styles():
    """Configures this module for WebKit style."""
    cpp_style._DEFAULT_FILTER_RULES = cpp_style._WEBKIT_FILTER_RULES


def parse_arguments(args, additional_flags=[], display_help=False):
    """Parses the command line arguments.

    See cpp_style.parse_arguments() for details.
    """
    return cpp_style.parse_arguments(args, additional_flags, display_help)


def process_file(filename):
    """Checks style for the specified file.

    If the specified filename is '-', applies cpp_style to the standard input.
    """
    if cpp_style.can_handle(filename) or filename == '-':
        cpp_style.process_file(filename)
    elif text_style.can_handle(filename):
        text_style.process_file(filename)


def process_patch(patch_string):
    """Does lint on a single patch.

    Args:
      patch_string: A string of a patch.
    """
    patch = DiffParser(patch_string.splitlines())
    for filename, diff in patch.files.iteritems():
        file_extension = os.path.splitext(filename)[1]
        line_numbers = set()

        def error_for_patch(filename, line_number, category, confidence, message):
            """Wrapper function of cpp_style.error for patches.

            This function outputs errors only if the line number
            corresponds to lines which are modified or added.
            """
            if not line_numbers:
                for line in diff.lines:
                    # When deleted line is not set, it means that
                    # the line is newly added.
                    if not line[0]:
                        line_numbers.add(line[1])

            if line_number in line_numbers:
                cpp_style.error(filename, line_number, category, confidence, message)

        if cpp_style.can_handle(filename):
            cpp_style.process_file(filename, error=error_for_patch)
        elif text_style.can_handle(filename):
            text_style.process_file(filename, error=error_for_patch)


def error_count():
    """Returns the total error count."""
    return cpp_style.error_count()
