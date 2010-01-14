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

"""Does WebKit-lint on text files.

This module shares error count, filter setting, output setting, etc. with cpp_style.
"""

import codecs
import os.path
import sys

import cpp_style


def process_file_data(filename, lines, error):
    """Performs lint check for text on the specified lines.

    It reports errors to the given error function.
    """
    lines = (['// adjust line numbers to make the first line 1.'] + lines)

    # FIXME: share with cpp_style.check_style()
    for line_number, line in enumerate(lines):
        if '\t' in line:
            error(filename, line_number, 'whitespace/tab', 5, 'Line contains tab character.')


def process_file(filename, error):
    """Performs lint check for text on a single file."""
    if (not can_handle(filename)):
        sys.stderr.write('Ignoring %s; not a supported file\n' % filename)
        return

    # FIXME: share code with cpp_style.process_file().
    try:
        # Do not support for filename='-'. cpp_style handles it.
        lines = codecs.open(filename, 'r', 'utf8', 'replace').read().split('\n')
    except IOError:
        sys.stderr.write("Skipping input '%s': Can't open for reading\n" % filename)
        return
    process_file_data(filename, lines, error)


def can_handle(filename):
    """Checks if this module supports the specified file type.

    Args:
      filename: A filename. It may contain directory names.
    """
    return ("ChangeLog" in filename
            or "WebKitTools/Scripts/" in filename
            or os.path.splitext(filename)[1] in ('.css', '.html', '.idl', '.js', '.mm', '.php', '.pm', '.py', '.txt')) and not "LayoutTests/" in filename
