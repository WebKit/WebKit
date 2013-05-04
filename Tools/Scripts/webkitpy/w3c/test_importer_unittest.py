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
import os
import unittest2 as unittest

from webkitpy.w3c import test_converter
from webkitpy.w3c.test_importer import TestImporter

WEBKIT_ROOT = __file__.split(os.path.sep + 'Tools')[0]


class TestImporterTest(unittest.TestCase):

    def test_ImportDirWithNoTests(self):
        """ Tests do_import() with a directory that contains no tests """
        dir_with_no_tests = os.path.join(os.path.sep, test_converter.WEBKIT_ROOT,
                                                      test_converter.SOURCE_SUBDIR,
                                                      test_converter.WEBCORE_SUBDIR,
                                                      test_converter.CSS_SUBDIR)

        importer = TestImporter(dir_with_no_tests, None)
        importer.do_import()


    # TODO: Need more tests, but what to use for valid source directory? Make it on the fly
    #       and clean it up afterward?
