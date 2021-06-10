#!/usr/bin/env python

#  Copyright 2017 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import os
import shutil
import tempfile
import textwrap
import unittest

import PRESUBMIT
# pylint: disable=line-too-long
from presubmit_test_mocks import MockInputApi, MockOutputApi, MockFile, MockChange


class CheckBugEntryFieldTest(unittest.TestCase):
    def testCommitMessageBugEntryWithNoError(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.change = MockChange([], ['webrtc:1234'])
        errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(0, len(errors))

    def testCommitMessageBugEntryReturnError(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.change = MockChange([], ['webrtc:1234', 'webrtc=4321'])
        errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(('Bogus Bug entry: webrtc=4321. Please specify'
                          ' the issue tracker prefix and the issue number,'
                          ' separated by a colon, e.g. webrtc:123 or'
                          ' chromium:12345.'), str(errors[0]))

    def testCommitMessageBugEntryWithoutPrefix(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.change = MockChange([], ['1234'])
        errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(1, len(errors))
        self.assertEqual(('Bug entry requires issue tracker prefix, '
                          'e.g. webrtc:1234'), str(errors[0]))

    def testCommitMessageBugEntryIsNone(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.change = MockChange([], ['None'])
        errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(0, len(errors))

    def testCommitMessageBugEntrySupportInternalBugReference(self):
        mock_input_api = MockInputApi()
        mock_output_api = MockOutputApi()
        mock_input_api.change.BUG = 'b/12345'
        errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(0, len(errors))
        mock_input_api.change.BUG = 'b/12345, webrtc:1234'
        errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                      mock_output_api)
        self.assertEqual(0, len(errors))


class CheckNewlineAtTheEndOfProtoFilesTest(unittest.TestCase):
    def setUp(self):
        self.tmp_dir = tempfile.mkdtemp()
        self.proto_file_path = os.path.join(self.tmp_dir, 'foo.proto')
        self.input_api = MockInputApi()
        self.output_api = MockOutputApi()

    def tearDown(self):
        shutil.rmtree(self.tmp_dir, ignore_errors=True)

    def testErrorIfProtoFileDoesNotEndWithNewline(self):
        self._GenerateProtoWithoutNewlineAtTheEnd()
        self.input_api.files = [MockFile(self.proto_file_path)]
        errors = PRESUBMIT.CheckNewlineAtTheEndOfProtoFiles(
            self.input_api, self.output_api, lambda x: True)
        self.assertEqual(1, len(errors))
        self.assertEqual(
            'File %s must end with exactly one newline.' %
            self.proto_file_path, str(errors[0]))

    def testNoErrorIfProtoFileEndsWithNewline(self):
        self._GenerateProtoWithNewlineAtTheEnd()
        self.input_api.files = [MockFile(self.proto_file_path)]
        errors = PRESUBMIT.CheckNewlineAtTheEndOfProtoFiles(
            self.input_api, self.output_api, lambda x: True)
        self.assertEqual(0, len(errors))

    def _GenerateProtoWithNewlineAtTheEnd(self):
        with open(self.proto_file_path, 'w') as f:
            f.write(
                textwrap.dedent("""
        syntax = "proto2";
        option optimize_for = LITE_RUNTIME;
        package webrtc.audioproc;
      """))

    def _GenerateProtoWithoutNewlineAtTheEnd(self):
        with open(self.proto_file_path, 'w') as f:
            f.write(
                textwrap.dedent("""
        syntax = "proto2";
        option optimize_for = LITE_RUNTIME;
        package webrtc.audioproc;"""))


class CheckNoMixingSourcesTest(unittest.TestCase):
    def setUp(self):
        self.tmp_dir = tempfile.mkdtemp()
        self.file_path = os.path.join(self.tmp_dir, 'BUILD.gn')
        self.input_api = MockInputApi()
        self.output_api = MockOutputApi()

    def tearDown(self):
        shutil.rmtree(self.tmp_dir, ignore_errors=True)

    def testErrorIfCAndCppAreMixed(self):
        self._AssertNumberOfErrorsWithSources(1, ['foo.c', 'bar.cc', 'bar.h'])

    def testErrorIfCAndObjCAreMixed(self):
        self._AssertNumberOfErrorsWithSources(1, ['foo.c', 'bar.m', 'bar.h'])

    def testErrorIfCAndObjCppAreMixed(self):
        self._AssertNumberOfErrorsWithSources(1, ['foo.c', 'bar.mm', 'bar.h'])

    def testErrorIfCppAndObjCAreMixed(self):
        self._AssertNumberOfErrorsWithSources(1, ['foo.cc', 'bar.m', 'bar.h'])

    def testErrorIfCppAndObjCppAreMixed(self):
        self._AssertNumberOfErrorsWithSources(1, ['foo.cc', 'bar.mm', 'bar.h'])

    def testNoErrorIfOnlyC(self):
        self._AssertNumberOfErrorsWithSources(0, ['foo.c', 'bar.c', 'bar.h'])

    def testNoErrorIfOnlyCpp(self):
        self._AssertNumberOfErrorsWithSources(0, ['foo.cc', 'bar.cc', 'bar.h'])

    def testNoErrorIfOnlyObjC(self):
        self._AssertNumberOfErrorsWithSources(0, ['foo.m', 'bar.m', 'bar.h'])

    def testNoErrorIfOnlyObjCpp(self):
        self._AssertNumberOfErrorsWithSources(0, ['foo.mm', 'bar.mm', 'bar.h'])

    def testNoErrorIfObjCAndObjCppAreMixed(self):
        self._AssertNumberOfErrorsWithSources(0, ['foo.m', 'bar.mm', 'bar.h'])

    def testNoErrorIfSourcesAreInExclusiveIfBranches(self):
        self._GenerateBuildFile(
            textwrap.dedent("""
      rtc_library("bar_foo") {
        if (is_win) {
          sources = [
            "bar.cc",
          ],
        }
        if (is_ios) {
          sources = [
            "bar.mm",
          ],
        }
      }
      rtc_library("foo_bar") {
        if (is_win) {
          sources = [
            "foo.cc",
          ],
        }
        if (is_ios) {
          sources = [
            "foo.mm",
          ],
        }
      }
    """))
        self.input_api.files = [MockFile(self.file_path)]
        errors = PRESUBMIT.CheckNoMixingSources(self.input_api,
                                                [MockFile(self.file_path)],
                                                self.output_api)
        self.assertEqual(0, len(errors))

    def testErrorIfSourcesAreNotInExclusiveIfBranches(self):
        self._GenerateBuildFile(
            textwrap.dedent("""
      rtc_library("bar_foo") {
        if (is_win) {
          sources = [
            "bar.cc",
          ],
        }
        if (foo_bar) {
          sources += [
            "bar.mm",
          ],
        }
      }
      rtc_library("foo_bar") {
        if (is_win) {
          sources = [
            "foo.cc",
          ],
        }
        if (foo_bar) {
          sources += [
            "foo.mm",
          ],
        }
        if (is_ios) {
          sources = [
            "bar.m",
            "bar.c",
          ],
        }
      }
    """))
        self.input_api.files = [MockFile(self.file_path)]
        errors = PRESUBMIT.CheckNoMixingSources(self.input_api,
                                                [MockFile(self.file_path)],
                                                self.output_api)
        self.assertEqual(1, len(errors))
        self.assertTrue('bar.cc' in str(errors[0]))
        self.assertTrue('bar.mm' in str(errors[0]))
        self.assertTrue('foo.cc' in str(errors[0]))
        self.assertTrue('foo.mm' in str(errors[0]))
        self.assertTrue('bar.m' in str(errors[0]))
        self.assertTrue('bar.c' in str(errors[0]))

    def _AssertNumberOfErrorsWithSources(self, number_of_errors, sources):
        assert len(
            sources) == 3, 'This function accepts a list of 3 source files'
        self._GenerateBuildFile(
            textwrap.dedent("""
      rtc_static_library("bar_foo") {
        sources = [
          "%s",
          "%s",
          "%s",
        ],
      }
      rtc_library("foo_bar") {
        sources = [
          "%s",
          "%s",
          "%s",
        ],
      }
    """ % (tuple(sources) * 2)))
        self.input_api.files = [MockFile(self.file_path)]
        errors = PRESUBMIT.CheckNoMixingSources(self.input_api,
                                                [MockFile(self.file_path)],
                                                self.output_api)
        self.assertEqual(number_of_errors, len(errors))
        if number_of_errors == 1:
            for source in sources:
                if not source.endswith('.h'):
                    self.assertTrue(source in str(errors[0]))

    def _GenerateBuildFile(self, content):
        with open(self.file_path, 'w') as f:
            f.write(content)


if __name__ == '__main__':
    unittest.main()
