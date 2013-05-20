#!/usr/bin/env python

# Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above
#    copyright notice, this list of conditions and the following
#    disclaimer.
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials
#    provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
# OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
# THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

import optparse
import shutil
import tempfile
import unittest2 as unittest

from webkitpy.common.host import Host
from webkitpy.common.system.executive_mock import MockExecutive2, ScriptError
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.w3c.test_importer import TestImporter


class TestImporterTest(unittest.TestCase):

    def test_import_dir_with_no_tests_and_no_hg(self):
        # FIXME: Use MockHosts instead.
        host = Host()
        host.executive = MockExecutive2(exception=OSError())

        importer = TestImporter(host, None, optparse.Values({"overwrite": False}))
        importer.source_directory = importer.path_from_webkit_root("Tools", "Scripts", "webkitpy", "w3c")
        importer.destination_directory = tempfile.mkdtemp(prefix='csswg')

        oc = OutputCapture()
        oc.capture_output()
        try:
            importer.do_import()
        finally:
            oc.restore_output()
            shutil.rmtree(importer.destination_directory, ignore_errors=True)

    def test_import_dir_with_no_tests(self):
        # FIXME: Use MockHosts instead.
        host = Host()
        host.executive = MockExecutive2(exception=ScriptError("abort: no repository found in '/Volumes/Source/src/wk/Tools/Scripts/webkitpy/w3c' (.hg not found)!"))

        importer = TestImporter(host, None, optparse.Values({"overwrite": False}))
        importer.source_directory = importer.path_from_webkit_root("Tools", "Scripts", "webkitpy", "w3c")
        importer.destination_directory = tempfile.mkdtemp(prefix='csswg')

        oc = OutputCapture()
        oc.capture_output()
        try:
            importer.do_import()
        finally:
            oc.restore_output()
            shutil.rmtree(importer.destination_directory, ignore_errors=True)

    # FIXME: Need more tests, but need to add a mock filesystem w/ sample data.
