# Copyright (C) 2013 University of Szeged
# This module is a refactoring from gtk.py, Copyright (C) 2013 Igalia S.L
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

import unittest2 as unittest
import os

from webkitpy.port.linux_get_crash_log import GDBCrashLogGenerator
from webkitpy.common.system.filesystem_mock import MockFileSystem


class GDBCrashLogGeneratorTest(unittest.TestCase):

    def test_generate_crash_log(self):
        generator = GDBCrashLogGenerator('DumpRenderTree', 28529, newer_than=None, filesystem=MockFileSystem({'/path/to/coredumps': ''}), path_to_driver=None)

        core_directory = os.environ.get('WEBKIT_CORE_DUMPS_DIRECTORY', '/path/to/coredumps')
        core_pattern = os.path.join(core_directory, "core-pid_%p-_-process_%e")
        mock_empty_crash_log = """\
crash log for DumpRenderTree (pid 28529):

Coredump core-pid_28529-_-process_DumpRenderTree not found. To enable crash logs:

- run this command as super-user: echo "%(core_pattern)s" > /proc/sys/kernel/core_pattern
- enable core dumps: ulimit -c unlimited
- set the WEBKIT_CORE_DUMPS_DIRECTORY environment variable: export WEBKIT_CORE_DUMPS_DIRECTORY=%(core_directory)s


STDERR: <empty>""" % locals()

        def _mock_gdb_output(self, coredump_path):
            return (mock_empty_crash_log, [])

        generator._get_gdb_output = _mock_gdb_output
        stderr, log = generator.generate_crash_log(None, None)
        self.assertEqual(stderr, None)
        self.assertMultiLineEqual(log, mock_empty_crash_log)

        generator.newer_than = 0.0
        self.assertEqual(stderr, None)
        self.assertMultiLineEqual(log, mock_empty_crash_log)
