#!/usr/bin/env python
# Copyright (C) 2010 Google Inc. All rights reserved.
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
#     * Neither the Google name nor the names of its
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

"""Dummy Port implementation used for testing."""
from __future__ import with_statement

import time

from webkitpy.common.system import filesystem_mock

from webkitpy.layout_tests.layout_package import test_output

import base


# This sets basic expectations for a test. Each individual expectation
# can be overridden by a keyword argument in TestList.add().
class TestInstance:
    def __init__(self, name):
        self.name = name
        self.base = name[(name.rfind("/") + 1):name.rfind(".html")]
        self.crash = False
        self.exception = False
        self.hang = False
        self.keyboard = False
        self.error = ''
        self.timeout = False
        self.actual_text = self.base + '-txt\n'
        self.actual_checksum = self.base + '-checksum\n'
        self.actual_image = self.base + '-png\n'
        self.expected_text = self.actual_text
        self.expected_checksum = self.actual_checksum
        self.expected_image = self.actual_image


# This is an in-memory list of tests, what we want them to produce, and
# what we want to claim are the expected results.
class TestList:
    def __init__(self):
        self.tests = {}

    def add(self, name, **kwargs):
        test = TestInstance(name)
        for key, value in kwargs.items():
            test.__dict__[key] = value
        self.tests[name] = test

    def keys(self):
        return self.tests.keys()

    def __contains__(self, item):
        return item in self.tests

    def __getitem__(self, item):
        return self.tests[item]


def unit_test_list():
    tests = TestList()
    tests.add('failures/expected/checksum.html',
                actual_checksum='checksum_fail-checksum')
    tests.add('failures/expected/crash.html', crash=True)
    tests.add('failures/expected/exception.html', exception=True)
    tests.add('failures/expected/timeout.html', timeout=True)
    tests.add('failures/expected/hang.html', hang=True)
    tests.add('failures/expected/missing_text.html',
                expected_text=None)
    tests.add('failures/expected/image.html',
                actual_image='image_fail-png',
                expected_image='image-png')
    tests.add('failures/expected/image_checksum.html',
                actual_checksum='image_checksum_fail-checksum',
                actual_image='image_checksum_fail-png')
    tests.add('failures/expected/keyboard.html',
                keyboard=True)
    tests.add('failures/expected/missing_check.html',
                expected_checksum=None)
    tests.add('failures/expected/missing_image.html',
                expected_image=None)
    tests.add('failures/expected/missing_text.html',
                expected_text=None)
    tests.add('failures/expected/newlines_leading.html',
                expected_text="\nfoo\n",
                actual_text="foo\n")
    tests.add('failures/expected/newlines_trailing.html',
                expected_text="foo\n\n",
                actual_text="foo\n")
    tests.add('failures/expected/newlines_with_excess_CR.html',
                expected_text="foo\r\r\r\n",
                actual_text="foo\n")
    tests.add('failures/expected/text.html',
                actual_text='text_fail-png')
    tests.add('failures/unexpected/crash.html', crash=True)
    tests.add('failures/unexpected/text-image-checksum.html',
                actual_text='text-image-checksum_fail-txt',
                actual_checksum='text-image-checksum_fail-checksum')
    tests.add('failures/unexpected/timeout.html', timeout=True)
    tests.add('http/tests/passes/text.html')
    tests.add('http/tests/ssl/text.html')
    tests.add('passes/error.html', error='stuff going to stderr')
    tests.add('passes/image.html')
    tests.add('passes/platform_image.html')
    # Text output files contain "\r\n" on Windows.  This may be
    # helpfully filtered to "\r\r\n" by our Python/Cygwin tooling.
    tests.add('passes/text.html',
                expected_text='\nfoo\n\n',
                actual_text='\nfoo\r\n\r\r\n')
    tests.add('websocket/tests/passes/text.html')
    return tests


# Here we use a non-standard location for the layout tests, to ensure that
# this works. The path contains a '.' in the name because we've seen bugs
# related to this before.

LAYOUT_TEST_DIR = '/test.checkout/LayoutTests'


# Here we synthesize an in-memory filesystem from the test list
# in order to fully control the test output and to demonstrate that
# we don't need a real filesystem to run the tests.

def unit_test_filesystem(files=None):
    """Return the FileSystem object used by the unit tests."""
    test_list = unit_test_list()
    files = files or {}

    def add_file(files, test, suffix, contents):
        dirname = test.name[0:test.name.rfind('/')]
        base = test.base
        path = LAYOUT_TEST_DIR + '/' + dirname + '/' + base + suffix
        files[path] = contents

    # Add each test and the expected output, if any.
    for test in test_list.tests.values():
        add_file(files, test, '.html', '')
        add_file(files, test, '-expected.txt', test.expected_text)
        add_file(files, test, '-expected.checksum', test.expected_checksum)
        add_file(files, test, '-expected.png', test.expected_image)

    # Add the test_expectations file.
    files[LAYOUT_TEST_DIR + '/platform/test/test_expectations.txt'] = """
WONTFIX : failures/expected/checksum.html = IMAGE
WONTFIX : failures/expected/crash.html = CRASH
// This one actually passes because the checksums will match.
WONTFIX : failures/expected/image.html = PASS
WONTFIX : failures/expected/image_checksum.html = IMAGE
WONTFIX : failures/expected/missing_check.html = MISSING PASS
WONTFIX : failures/expected/missing_image.html = MISSING PASS
WONTFIX : failures/expected/missing_text.html = MISSING PASS
WONTFIX : failures/expected/newlines_leading.html = TEXT
WONTFIX : failures/expected/newlines_trailing.html = TEXT
WONTFIX : failures/expected/newlines_with_excess_CR.html = TEXT
WONTFIX : failures/expected/text.html = TEXT
WONTFIX : failures/expected/timeout.html = TIMEOUT
WONTFIX SKIP : failures/expected/hang.html = TIMEOUT
WONTFIX SKIP : failures/expected/keyboard.html = CRASH
WONTFIX SKIP : failures/expected/exception.html = CRASH
"""

    fs = filesystem_mock.MockFileSystem(files)
    fs._tests = test_list
    return fs


class TestPort(base.Port):
    """Test implementation of the Port interface."""

    def __init__(self, **kwargs):
        # FIXME: what happens if we're not passed in the test filesystem
        # and the tests don't match what's in the filesystem?
        #
        # We'll leave as is for now to avoid unnecessary dependencies while
        # converting all of the unit tests over to using
        # unit_test_filesystem(). If things get out of sync the tests should
        # fail in fairly obvious ways. Eventually we want to just do:
        #
        # assert kwargs['filesystem']._tests
        # self._tests = kwargs['filesystem']._tests

        if 'filesystem' not in kwargs or kwargs['filesystem'] is None:
            kwargs['filesystem'] = unit_test_filesystem()
            self._tests = kwargs['filesystem']._tests
        else:
            self._tests = unit_test_list()

        kwargs.setdefault('port_name', 'test')
        base.Port.__init__(self, **kwargs)

    def baseline_path(self):
        return self._filesystem.join(self.layout_tests_dir(), 'platform',
                                     self.name() + self.version())

    def baseline_search_path(self):
        return [self.baseline_path()]

    def check_build(self, needs_http):
        return True

    def diff_image(self, expected_contents, actual_contents,
                   diff_filename=None):
        diffed = actual_contents != expected_contents
        if diffed and diff_filename:
            self._filesystem.write_text_file(diff_filename,
                "< %s\n---\n> %s\n" % (expected_contents, actual_contents))
        return diffed

    def layout_tests_dir(self):
        return LAYOUT_TEST_DIR

    def name(self):
        return self._name

    def _path_to_wdiff(self):
        return None

    def results_directory(self):
        return '/tmp/' + self.get_option('results_directory')

    def setup_test_run(self):
        pass

    def create_driver(self, worker_number):
        return TestDriver(self, worker_number)

    def start_http_server(self):
        pass

    def start_websocket_server(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def test_base_platform_names(self):
        return ('mac', 'win')

    def test_expectations(self):
        return self._filesystem.read_text_file(LAYOUT_TEST_DIR + '/platform/test/test_expectations.txt')

    def test_platform_name(self):
        return 'mac'

    def test_platform_names(self):
        return self.test_base_platform_names()

    def test_platform_name_to_name(self, test_platform_name):
        return test_platform_name

    def version(self):
        return ''


class TestDriver(base.Driver):
    """Test/Dummy implementation of the DumpRenderTree interface."""

    def __init__(self, port, worker_number):
        self._port = port

    def cmd_line(self):
        return ['None']

    def poll(self):
        return True

    def run_test(self, test_input):
        start_time = time.time()
        test_name = self._port.relative_test_filename(test_input.filename)
        test = self._port._tests[test_name]
        if test.keyboard:
            raise KeyboardInterrupt
        if test.exception:
            raise ValueError('exception from ' + test_name)
        if test.hang:
            time.sleep((float(test_input.timeout) * 4) / 1000.0)
        return test_output.TestOutput(test.actual_text, test.actual_image,
                                      test.actual_checksum, test.crash,
                                      time.time() - start_time, test.timeout,
                                      test.error)

    def start(self):
        pass

    def stop(self):
        pass
