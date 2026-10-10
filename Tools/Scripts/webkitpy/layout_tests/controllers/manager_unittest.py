# Copyright (C) 2010 Google Inc. All rights reserved.
# Copyright (C) 2010 Gabor Rapcsanyi (rgabor@inf.u-szeged.hu), University of Szeged
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

"""Unit tests for manager.py."""

from collections import OrderedDict
from io import StringIO
import sys
import time
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.controllers.layout_test_finder_legacy import LayoutTestFinder
from webkitpy.layout_tests.controllers.manager import Manager
from webkitpy.layout_tests.models import test_expectations
from webkitpy.layout_tests.models.server_routing import ServerRoute, ServerType
from webkitpy.layout_tests.models.test import Reference, Test
from webkitpy.layout_tests.models.test_expectations import TestExpectations
from webkitpy.layout_tests.models.test_input import ReferenceInput
from webkitpy.layout_tests.models.test_run_results import TestRunResults
from webkitpy.port.test import LAYOUT_TEST_DIR, TestPort
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.mocktool import MockOptions
from webkitpy.xcode.device_type import DeviceType


class ManagerTest(unittest.TestCase):
    def _get_manager(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        manager = Manager(port, options=MockOptions(test_list=None, http=True, verbose=False, driver_names=list(port.DEFAULT_SUPPORTED_DRIVERS)), printer=Mock())
        return manager

    def test_look_for_new_crash_logs(self):
        host = MockHost()
        port = host.port_factory.get('test-mac-leopard')
        tests = ['failures/expected/crash.html']
        expectations = test_expectations.TestExpectations(port, tests)
        expectations.parse_all_expectations()
        run_results = TestRunResults(expectations, len(tests))
        manager = self._get_manager()
        manager._look_for_new_crash_logs(run_results, time.time())

    def test_print_expectations_for_subset(self):
        def get_test_names():
            return ['failures/expected/text.html',
                    'failures/expected/image_checksum.html',
                    'failures/expected/crash.html',
                    'failures/expected/leak.html',
                    'failures/expected/flaky-leak.html',
                    'failures/expected/missing_text.html',
                    'failures/expected/image.html',
                    'failures/expected/reftest.html',
                    'failures/expected/leaky-reftest.html',
                    'passes/text.html']

        def get_tests():
            return [Test(test) for test in get_test_names()]

        def get_expectations():
            return """
Bug(test) failures/expected/text.html [ Failure ]
Bug(test) failures/expected/crash.html [ WontFix ]
Bug(test) failures/expected/leak.html [ Leak ]
Bug(test) failures/expected/flaky-leak.html [ Failure Leak ]
Bug(test) failures/expected/missing_image.html [ Rebaseline Missing ]
Bug(test) failures/expected/image_checksum.html [ WontFix ]
Bug(test) failures/expected/image.html [ WontFix Mac ]
Bug(test) failures/expected/reftest.html [ ImageOnlyFailure ]
Bug(test) failures/expected/leaky-reftest.html [ ImageOnlyFailure Leak ]
"""

        def get_printed_expectations():
            # There are trailing whitespaces in this string, they are as intended.
            return """
Tests to run for DEVICE_TYPE (10)
failures/expected/crash.html           ['SKIP'] expectations:3 Bug(test) failures/expected/crash.html [ WontFix ]
failures/expected/flaky-leak.html      ['FAIL', 'LEAK'] expectations:5 Bug(test) failures/expected/flaky-leak.html [ Failure Leak ]
failures/expected/image.html           ['PASS']  
failures/expected/image_checksum.html  ['SKIP'] expectations:7 Bug(test) failures/expected/image_checksum.html [ WontFix ]
failures/expected/leak.html            ['LEAK'] expectations:4 Bug(test) failures/expected/leak.html [ Leak ]
failures/expected/leaky-reftest.html   ['IMAGE', 'LEAK'] expectations:10 Bug(test) failures/expected/leaky-reftest.html [ ImageOnlyFailure Leak ]
failures/expected/missing_text.html    ['PASS']  
failures/expected/reftest.html         ['IMAGE'] expectations:9 Bug(test) failures/expected/reftest.html [ ImageOnlyFailure ]
failures/expected/text.html            ['FAIL'] expectations:2 Bug(test) failures/expected/text.html [ Failure ]
passes/text.html                       ['PASS']  
"""

        def parse_exp(test_names, expectations):
            expectations_dict = OrderedDict()
            expectations_dict['expectations'] = expectations
            host = MockHost()
            port = host.port_factory.get('test-win-xp', None)
            port.expectations_dict = lambda **kwargs: expectations_dict
            exp = TestExpectations(port, test_names)
            exp.parse_all_expectations()
            return exp

        manager = self._get_manager()
        device_type = "DEVICE_TYPE"
        driver_name = manager._driver_names[0]
        manager._current_driver_name = driver_name
        manager._expectations[(driver_name, device_type)] = parse_exp(get_test_names(), get_expectations())
        test_col_width = max(len(test) for test in get_test_names()) + 1

        initial_stdout = sys.stdout
        stringIO = StringIO()
        sys.stdout = stringIO
        try:
            manager._print_expectations_for_subset(device_type, test_col_width, get_tests())
        finally:
            sys.stdout = initial_stdout

        out = stringIO.getvalue()
        self.maxDiff = None

        # Note: Buildbots compare `--print-expectations` outputs between revisions,
        # so if the output is not *exactly* as expected including whitespaces, this
        # could lead to unwanted effects, like blocking builds for a long time.
        self.assertEqual(get_printed_expectations(), out)

    def test_print_expectations_no_tests_found(self):
        # Passing a non-existent path should not raise ValueError from max() on an
        # empty sequence; it should return 0 cleanly.
        manager = self._get_manager()
        manager._options.update(repeat_each=1, iterations=1)
        device_type_list = manager._port.supported_device_types()
        manager._create_port_for_driver = Mock(return_value=manager._port)
        manager._collect_tests = Mock(return_value=({dt: [] for dt in device_type_list}, set()))
        exit_code = manager.print_expectations(['this/file/does/not/exist.html'])
        self.assertEqual(exit_code, 0)


class ComputeTestUrlFromFinderTest(unittest.TestCase):
    def setUp(self):
        host = MockHost()
        host.filesystem.write_text_file(LAYOUT_TEST_DIR + '/http/tests/local/foo.html', '')
        host.filesystem.write_text_file(LAYOUT_TEST_DIR + '/media/brand-new.html', '')
        host.filesystem.write_text_file(LAYOUT_TEST_DIR + '/http/wpt/foo/bar.html', '<script src="/resources/testharness.js"></script>')
        host.filesystem.write_text_file(LAYOUT_TEST_DIR + '/http/wpt/foo/bar.https.html', '<script src="/resources/testharness.js"></script>')
        self.port = host.port_factory.get('test-mac-leopard')
        self.finder = LayoutTestFinder(self.port, None)
        self.manager = Manager(self.port, options=MockOptions(test_list=None, http=True, verbose=False, additional_header=None, driver_names=list(self.port.DEFAULT_SUPPORTED_DRIVERS)), printer=Mock())

    def _url_for(self, test_path):
        tests = list(self.finder.find_tests_by_path([test_path]))
        self.assertEqual(len(tests), 1)
        return self.manager._compute_url_for_test_name(tests[0])

    def test_compute_url_for_test_name(self):
        d = self.port.layout_tests_dir()
        self.assertEqual(self._url_for('passes/text.html'), 'file://%s/passes/text.html' % d)
        self.assertEqual(self._url_for('http/tests/passes/text.html'), 'http://127.0.0.1:8000/passes/text.html')
        self.assertEqual(self._url_for('http/tests/ssl/text.html'), 'https://127.0.0.1:8443/ssl/text.html')
        self.assertEqual(self._url_for('http/tests/local/foo.html'), 'file://%s/http/tests/local/foo.html' % d)
        self.assertEqual(self._url_for('imported/w3c/web-platform-tests/some/new.html'), 'http://localhost:8800/some/new.html')
        self.assertEqual(self._url_for('imported/w3c/web-platform-tests/some/test-pass-crash.https.html'), 'https://localhost:8800/some/test-pass-crash.https.html')
        self.assertEqual(self._url_for('http/wpt/foo/bar.html'), 'http://localhost:8800/WebKit/foo/bar.html')
        self.assertEqual(self._url_for('http/wpt/foo/bar.https.html'), 'https://localhost:8800/WebKit/foo/bar.https.html')
        self.assertEqual(self._url_for('websocket/tests/passes/text.html'), 'file://%s/websocket/tests/passes/text.html' % d)

    def test_compute_url_for_test_name_specificity_sort(self):
        # A more-specific prefix should win over a less-specific one even if
        # the less-specific prefix appears earlier in the table. With
        # the deeper prefix stripped, the resulting rel_path (and hence URL)
        # is shorter than what the shallower prefix would have produced.
        original_routes = self.port.test_routes()
        custom_routes = [
            ServerRoute("http/tests", "/", ServerType.HTTP),
            ServerRoute("http/tests/specific", "/special/", ServerType.HTTP),
        ] + [route for route in original_routes if route.test_file_dir != "http/tests"]
        self.port.test_routes = lambda: custom_routes

        test = Test("http/tests/specific/foo.html", served_by=ServerType.HTTP)
        # The deeper prefix "http/tests/specific" wins; rel_path is "foo.html".
        # (If the shallower "http/tests" had won, the URL would have included
        # "specific/foo.html".)
        self.assertEqual(
            self.manager._compute_url_for_test_name(test),
            "http://127.0.0.1:8000/foo.html",
        )

    def _test_url_for(self, test_path, additional_header):
        tests = list(self.finder.find_tests_by_path([test_path]))
        self.assertEqual(len(tests), 1)
        self.manager._options.update(additional_header=additional_header)
        return self.manager._compute_test_url(tests[0])

    def test_run_in_cross_origin_frame_overrides_file_url(self):
        # When --additional-header includes runInCrossOriginFrame=true, a non-HTTP
        # test must be loaded over HTTP via the /root/ Apache alias so it can be
        # embedded in a cross-origin frame. This preserves the legacy behavior of
        # Driver.is_http_test() which forced the same URL transformation.
        self.assertEqual(self._test_url_for('passes/text.html', 'runInCrossOriginFrame=true'), 'http://127.0.0.1:8000/root/passes/text.html')
        self.assertEqual(self._test_url_for('media/brand-new.html', 'runInCrossOriginFrame=true'), 'http://127.0.0.1:8000/root/media/brand-new.html')

    def test_run_in_cross_origin_frame_does_not_override_http_url(self):
        # If the test is already served over HTTP/HTTPS/WPT, the natural URL must
        # be kept; only file:// URLs are overridden.
        self.assertEqual(self._test_url_for('http/tests/passes/text.html', 'runInCrossOriginFrame=true'), 'http://127.0.0.1:8000/passes/text.html')
        self.assertEqual(self._test_url_for('imported/w3c/web-platform-tests/some/new.html', 'runInCrossOriginFrame=true'), 'http://localhost:8800/some/new.html')

    def test_run_in_cross_origin_frame_serves_http_local_test_at_http_root(self):
        # http/tests/local is loaded from file:// normally, but origin/main's
        # Driver.is_http_test() checked the header before the local check, so a
        # cross-origin frame got http://127.0.0.1:8000/local/foo.html.
        self.assertEqual(self._test_url_for('http/tests/local/foo.html', 'runInCrossOriginFrame=true'), 'http://127.0.0.1:8000/local/foo.html')

    def test_no_url_override_without_runInCrossOriginFrame(self):
        # Without the runInCrossOriginFrame header, file:// URLs should pass through
        # unchanged even if some other additional header is set.
        d = self.port.layout_tests_dir()
        self.assertEqual(self._test_url_for('passes/text.html', 'useEphemeralSession=false'), 'file://%s/passes/text.html' % d)
        self.assertEqual(self._test_url_for('http/tests/local/foo.html', None), 'file://%s/http/tests/local/foo.html' % d)

    def test_run_in_cross_origin_frame_leaves_reference_urls(self):
        # Only the test is embedded in the cross-origin frame; references never
        # carried the header, so they keep their natural URL.
        d = self.port.layout_tests_dir()
        self.manager._options.update(additional_header='runInCrossOriginFrame=true')
        reference = Reference('==', self.port.host.filesystem.join(d, 'passes/text-expected.html'))
        self.assertEqual(
            self.manager._compute_reference_url(reference, Test('passes/text.html')),
            'file://%s/passes/text-expected.html' % d,
        )

    def _test_input_for(self, test):
        manager = self.manager
        manager._test_is_slow = lambda test_name, device_type=None: False
        manager._test_should_dump_jsconsolelog_in_stderr = lambda test_name, device_type=None: False
        manager._options.time_out_ms = 6000
        manager._options.slow_time_out_ms = 30000
        manager._options.pixel_tests = True
        manager._options.pixel_test_directories = None
        return manager._test_input_for_file(test, None)

    def test_test_input_has_test_and_reference_urls(self):
        d = self.port.layout_tests_dir()
        references = (
            Reference('==', self.port.host.filesystem.join(d, 'foo/ref.html')),
            Reference('!=', self.port.host.filesystem.join(d, 'foo/mismatch-ref.html')),
        )
        test_input = self._test_input_for(Test('foo/test.html', reference_files=references))

        self.assertEqual(test_input.url, 'file://%s/foo/test.html' % d)
        self.assertEqual(
            test_input.reference_inputs,
            (
                ReferenceInput(references[0], 'file://%s/foo/ref.html' % d),
                ReferenceInput(references[1], 'file://%s/foo/mismatch-ref.html' % d),
            ),
        )

    def test_test_input_without_references_has_no_reference_inputs(self):
        test_input = self._test_input_for(Test('foo/test.html'))
        self.assertEqual(test_input.reference_inputs, ())

    def test_reference_url_inherits_server_and_flags_from_test(self):
        d = self.port.layout_tests_dir()
        join = self.port.host.filesystem.join
        # The reference's own name has no .https. in it; it is served over
        # HTTPS because its parent test is.
        wpt_ref = Reference('==', join(d, 'http/wpt/foo-ref.html'))
        wpt_test = Test(
            'http/wpt/foo.https.html',
            served_by=ServerType.WPT,
            flags=frozenset({'https'}),
            reference_files=(wpt_ref,),
        )
        url = self.manager._compute_reference_url(wpt_ref, wpt_test)
        self.assertTrue(url.startswith('https://'), url)
        self.assertTrue(url.endswith('/WebKit/foo-ref.html'), url)

        http_ref = Reference('==', join(d, 'http/tests/security/foo-ref.html'))
        http_test = Test(
            'http/tests/security/foo.https.html',
            served_by=ServerType.HTTP,
            flags=frozenset({'https'}),
            reference_files=(http_ref,),
        )
        self.assertEqual(
            self.manager._compute_reference_url(http_ref, http_test),
            'https://127.0.0.1:8443/security/foo-ref.html',
        )

        plain_ref = Reference('==', join(d, 'http/tests/security/plain-ref.html'))
        plain_test = Test('http/tests/security/plain.html', served_by=ServerType.HTTP, reference_files=(plain_ref,))
        self.assertEqual(
            self.manager._compute_reference_url(plain_ref, plain_test),
            'http://127.0.0.1:8000/security/plain-ref.html',
        )

    def test_reference_url_keeps_variant(self):
        d = self.port.layout_tests_dir()
        ref = Reference('==', self.port.host.filesystem.join(d, 'http/tests/foo/ref.html?variant'))
        test = Test('http/tests/foo/test.html?variant', served_by=ServerType.HTTP, reference_files=(ref,))
        url = self.manager._compute_reference_url(ref, test)
        self.assertEqual(url, 'http://127.0.0.1:8000/foo/ref.html?variant')

    def test_wpt_url_scheme_and_server_selection(self):
        port = self.port
        http = port.web_platform_test_server_base_http_url()
        https = port.web_platform_test_server_base_https_url()
        h2 = port.web_platform_test_server_base_h2_url()
        wpt = 'imported/w3c/web-platform-tests/'
        for name, flags, base in (
            ('x/t.html', set(), http),
            ('x/t.h2.html', {'h2'}, h2),
            ('x/t.https.html', {'https'}, https),
            ('x/t.sub.html', {'sub'}, http),
            ('x/t.serviceworker.html', set(), https),
            ('x/t.serviceworker-module.html', set(), https),
            # h2 takes precedence over https.
            ('x/t.h2.https.html', {'h2', 'https'}, h2),
        ):
            with self.subTest(name=name):
                test = Test(wpt + name, served_by=ServerType.WPT, flags=frozenset(flags))
                self.assertEqual(self.manager._compute_url_for_test_name(test), base + name)

    def test_webkit_wpt_urls_use_localhost_only_bases(self):
        port = self.port
        test = Test('http/wpt/foo/t.https.html', served_by=ServerType.WPT, flags=frozenset({'https'}))
        self.assertEqual(
            self.manager._compute_url_for_test_name(test),
            port.web_platform_test_server_base_https_url(localhost_only=True) + 'WebKit/foo/t.https.html',
        )
