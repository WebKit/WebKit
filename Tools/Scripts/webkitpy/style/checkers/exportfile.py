# Copyright (C) 2014 University of Szeged
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

"""Call Tools/Scripts/sort-export-file with --dry-run for checking style"""

import os
import subprocess
import os.path
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.webkit_finder import WebKitFinder


class ExportFileChecker(object):

    categories = set(['list/order'])

    def __init__(self, file_path, handle_style_error):
        self._filesystem = FileSystem()
        self._webkit_base = WebKitFinder(self._filesystem).webkit_base()
        self._file_path = file_path
        self._handle_style_error = handle_style_error
        self._handle_style_error.turn_off_line_filtering()

    def check(self, inline=None):
        os.chdir(self._webkit_base)
        retcode = subprocess.call(["Tools/Scripts/sort-export-file", self._file_path, "--verbose", "--dry-run"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if retcode == 1:
            self._handle_style_error(0, 'list/order', 5, "Parse error during processing %s, use Tools/Scripts/sort-export-files for details" % self._file_path)
        elif retcode == 2:
            self._handle_style_error(0, 'list/order', 5, "%s should be sorted, use Tools/Scripts/sort-export-file script" % self._file_path)
        elif retcode != 0:
            self._handle_style_error(0, 'list/order', 5, "Unexpected error during processing %s, please file a bug report against Tools/Scripts/sort-export-file" % self._file_path)
