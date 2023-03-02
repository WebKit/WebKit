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

import time

from webkitcorepy import string_utils

from webkitpy.port import Port, Driver, DriverOutput
from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.common.system.crashlogs import CrashLogs
from webkitpy.common.version_name_map import PUBLIC_TABLE, VersionNameMap
from webkitpy.port.image_diff import ImageDiffResult


# This sets basic expectations for a test. Each individual expectation
# can be overridden by a keyword argument in TestList.add().
class TestInstance(object):
    def __init__(self, name):
        self.name = name
        self.base = name[(name.rfind("/") + 1):name.rfind(".")]
        self.crash = False
        self.web_process_crash = False
        self.exception = False
        self.hang = False
        self.keyboard = False
        self.error = ''
        self.timeout = False
        self.is_reftest = False
        self.is_wpt_crash_test = False

        # The values of each field are treated as raw byte strings. They
        # will be converted to unicode strings where appropriate using
        # FileSystem.read_text_file().
        self.actual_text = self.base + '-txt'
        self.actual_checksum = self.base + '-checksum'

        # We add the '\x8a' for the image file to prevent the value from
        # being treated as UTF-8 (the character is invalid)
        self.actual_image = self.base + '\x8a' + '-png' + 'tEXtchecksum\x00' + self.actual_checksum

        self.expected_text = self.actual_text
        self.expected_image = self.actual_image

        self.actual_audio = None
        self.expected_audio = None


# This is an in-memory list of tests, what we want them to produce, and
# what we want to claim are the expected results.
class TestList(object):
    def __init__(self):
        self.tests = {}

    def add(self, name, **kwargs):
        test = TestInstance(name)
        for key, value in kwargs.items():
            test.__dict__[key] = value
        self.tests[name] = test

    def add_reftest(self, name, reference_name, same_image, **kwargs):
        self.add(name, actual_checksum='xxx', actual_image='XXX', is_reftest=True)
        if same_image:
            self.add(reference_name, actual_checksum='xxx', actual_image='XXX', is_reftest=True)
        else:
            self.add(reference_name, actual_checksum='yyy', actual_image='YYY', is_reftest=True)

        if kwargs:
            test = self.tests[name]
            for key, value in kwargs.items():
                test.__dict__[key] = value

    def keys(self):
        return self.tests.keys()

    def __contains__(self, item):
        return item in self.tests

    def __getitem__(self, item):
        return self.tests[item]


#
# These numbers may need to be updated whenever we add or delete tests.
#
TOTAL_TESTS = 84
TOTAL_SKIPS = 11
TOTAL_RETRIES = 13

UNEXPECTED_PASSES = 6
UNEXPECTED_FAILURES = 19


def unit_test_list():
    silent_audio = b"RIFF2\x00\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x22\x56\x00\x00\x44\xAC\x00\x00\x02\x00\x10\x00data\x0E\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    silent_audio_with_single_bit_difference = b"RIFF2\x00\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x22\x56\x00\x00\x44\xAC\x00\x00\x02\x00\x10\x00data\x0E\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    audio2 = b"RIFF2\x00\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x22\x56\x00\x00\x44\xAC\x00\x00\x02\x00\x10\x00data\x0E\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    tests = TestList()
    tests.add('failures/expected/crash.html', crash=True)
    tests.add('failures/expected/exception.html', exception=True)
    tests.add('failures/expected/timeout.html', timeout=True)
    tests.add('failures/expected/hang.html', hang=True)
    tests.add('failures/expected/missing_text.html', expected_text=None)
    tests.add('failures/expected/leak.html', leak=True)
    tests.add('failures/expected/flaky-leak.html', leak=True)
    tests.add('failures/expected/image.html',
              actual_image='image_fail-pngtEXtchecksum\x00checksum_fail',
              expected_image='image-pngtEXtchecksum\x00checksum-png')
    tests.add('failures/expected/pixel-fail.html',
              actual_image='image_fail-pngtEXtchecksum\x00checksum_fail',
              expected_image='image-pngtEXtchecksum\x00checksum-png')
    tests.add('failures/expected/image_checksum.html',
              actual_checksum='image_checksum_fail-checksum',
              actual_image='image_checksum_fail-png')
    tests.add('failures/expected/audio.html',
              actual_audio=silent_audio, expected_audio=audio2,
              actual_text=None, expected_text=None,
              actual_image=None, expected_image=None,
              actual_checksum=None)
    tests.add('failures/expected/keyboard.html', keyboard=True)
    tests.add('failures/expected/missing_check.html',
              expected_image='missing_check-png')
    tests.add('failures/expected/missing_image.html', expected_image=None)
    tests.add('failures/expected/missing_audio.html', expected_audio=None,
              actual_text=None, expected_text=None,
              actual_image=None, expected_image=None,
              actual_checksum=None)
    tests.add('failures/expected/missing_text.html', expected_text=None)
    tests.add('failures/expected/newlines_leading.html',
              expected_text="\nfoo\n", actual_text="foo\n")
    tests.add('failures/expected/newlines_trailing.html',
              expected_text="foo\n\n", actual_text="foo\n")
    tests.add('failures/expected/newlines_with_excess_CR.html',
              expected_text="foo\r\r\r\n", actual_text="foo\n")
    tests.add('failures/expected/text.html', actual_text='text_fail-png')
    tests.add('failures/expected/skip_text.html', actual_text='text diff')
    tests.add('failures/flaky/text.html')
    tests.add('failures/unexpected/missing_text.html', expected_text=None)
    tests.add('failures/unexpected/missing_check.html', expected_image='missing-check-png')
    tests.add('failures/unexpected/missing_image.html', expected_image=None)
    tests.add('failures/unexpected/missing_render_tree_dump.html', actual_text="""layer at (0,0) size 800x600
  RenderView at (0,0) size 800x600
layer at (0,0) size 800x34
  RenderBlock {HTML} at (0,0) size 800x34
    RenderBody {BODY} at (8,8) size 784x18
      RenderText {#text} at (0,0) size 133x18
        text run at (0,0) width 133: "This is an image test!"
""", expected_text=None)
    tests.add('failures/unexpected/crash.html', crash=True)
    tests.add('failures/unexpected/crash-with-stderr.html', crash=True,
              error="mock-std-error-output")
    tests.add('failures/unexpected/web-process-crash-with-stderr.html', web_process_crash=True,
              error="mock-std-error-output")
    tests.add('failures/unexpected/pass.html')
    tests.add('failures/unexpected/text-checksum.html',
              actual_text='text-checksum_fail-txt',
              actual_checksum='text-checksum_fail-checksum')
    tests.add('failures/unexpected/text-image-checksum.html',
              actual_text='text-image-checksum_fail-txt',
              actual_image='text-image-checksum_fail-pngtEXtchecksum\x00checksum_fail',
              actual_checksum='text-image-checksum_fail-checksum')
    tests.add('failures/unexpected/text-image-missing.html',
              actual_text='text-image-checksum_fail-txt',
              expected_image=None)
    tests.add('failures/unexpected/checksum-with-matching-image.html',
              actual_checksum='text-image-checksum_fail-checksum')
    tests.add('failures/unexpected/skip_pass.html')
    tests.add('failures/unexpected/text.html', actual_text='text_fail-txt')
    tests.add('failures/unexpected/timeout.html', timeout=True)
    tests.add('failures/unexpected/leak.html', leak=True)
    tests.add('http/tests/passes/text.html')
    tests.add('http/tests/passes/image.html')
    tests.add('http/tests/ssl/text.html')
    tests.add('passes/args.html')
    tests.add('passes/error.html', error='stuff going to stderr')
    tests.add('passes/image.html')
    tests.add('passes/audio.html',
              actual_audio=silent_audio, expected_audio=silent_audio,
              actual_text=None, expected_text=None,
              actual_image=None, expected_image=None,
              actual_checksum=None)
    tests.add('passes/audio-tolerance.html',
              actual_audio=silent_audio_with_single_bit_difference, expected_audio=silent_audio,
              actual_text=None, expected_text=None,
              actual_image=None, expected_image=None,
              actual_checksum=None)
    tests.add('passes/platform_image.html')
    tests.add('passes/checksum_in_image.html',
              expected_image='tEXtchecksum\x00checksum_in_image-checksum')
    tests.add('passes/skipped/skip.html')

    # Note that here the checksums don't match but the images do, so this test passes "unexpectedly".
    # See https://bugs.webkit.org/show_bug.cgi?id=69444 .
    tests.add('failures/unexpected/checksum.html', actual_checksum='checksum_fail-checksum')

    # Text output files contain "\r\n" on Windows.  This may be
    # helpfully filtered to "\r\r\n" by our Python/Cygwin tooling.
    tests.add('passes/text.html',
              expected_text='\nfoo\n\n', actual_text='\nfoo\r\n\r\r\n')

    # For reftests.
    tests.add_reftest('passes/reftest.html', 'passes/reftest-expected.html', same_image=True)
    tests.add_reftest('passes/mismatch.html', 'passes/mismatch-expected-mismatch.html', same_image=False)
    tests.add_reftest('passes/svgreftest.svg', 'passes/svgreftest-expected.svg', same_image=True)
    tests.add_reftest('passes/xhtreftest.xht', 'passes/xhtreftest-expected.html', same_image=True)
    tests.add_reftest('passes/phpreftest.php', 'passes/phpreftest-expected-mismatch.svg', same_image=False)
    tests.add_reftest('failures/expected/reftest.html', 'failures/expected/reftest-expected.html', same_image=False)
    tests.add_reftest('failures/expected/leaky-reftest.html', 'failures/expected/leaky-reftest-expected.html', same_image=False, leak=True)
    tests.add_reftest('failures/expected/mismatch.html', 'failures/expected/mismatch-expected-mismatch.html', same_image=True)
    tests.add_reftest('failures/unexpected/reftest.html', 'failures/unexpected/reftest-expected.html', same_image=False)
    tests.add_reftest('failures/unexpected/mismatch.html', 'failures/unexpected/mismatch-expected-mismatch.html', same_image=True)
    tests.add('failures/unexpected/reftest-nopixel.html', actual_checksum=None, actual_image=None, is_reftest=True)
    tests.add('failures/unexpected/reftest-nopixel-expected.html', actual_checksum=None, actual_image=None, is_reftest=True)
    # FIXME: Add a reftest which crashes.

    tests.add('websocket/tests/passes/text.html')

    # For testing test are properly included from platform directories.
    tests.add('platform/test-mac-leopard/passes/platform-specific-test.html')
    tests.add('platform/test-mac-leopard/platform-specific-dir/platform-specific-test.html')
    tests.add('platform/test-mac-leopard/http/test.html')
    tests.add('platform/test-win-7sp0/http/test.html')

    # For --no-http tests, test that platform specific HTTP tests are properly skipped.
    tests.add('platform/test-snow-leopard/http/test.html')
    tests.add('platform/test-snow-leopard/websocket/test.html')

    # For testing if perf tests are running in a locked shard.
    tests.add('perf/foo/test.html')
    tests.add('perf/foo/test-ref.html')

    # For testing --pixel-test-directories.
    tests.add('failures/unexpected/pixeldir/image_in_pixeldir.html',
        actual_image='image_in_pixeldir-pngtEXtchecksum\x00checksum_fail',
        expected_image='image_in_pixeldir-pngtEXtchecksum\x00checksum-png')
    tests.add('failures/unexpected/image_not_in_pixeldir.html',
        actual_image='image_not_in_pixeldir-pngtEXtchecksum\x00checksum_fail',
        expected_image='image_not_in_pixeldir-pngtEXtchecksum\x00checksum-png')

    tests.add('corner-cases/ews/directory-skipped/failure.html', expected_text='ok-txt', actual_text='text_fail-txt')
    tests.add('corner-cases/ews/directory-skipped/timeout.html', timeout=True)
    tests.add('corner-cases/ews/directory-flaky/failure.html', expected_text='ok-txt', actual_text='text_fail-txt')
    tests.add('corner-cases/ews/directory-flaky/timeout.html', timeout=True)

    tests.add('imported/w3c/web-platform-tests/some/new.html',
        expected_text=None, actual_text='ok', actual_image=None, actual_checksum=None)
    tests.add('imported/w3c/web-platform-tests/some/test-pass-crash.html',
        expected_text=None, actual_text='some output', actual_image=None, actual_checksum=None, is_wpt_crash_test=True)
    tests.add('imported/w3c/web-platform-tests/some/test-timeout-crash.html',
        expected_text=None, actual_text=None, actual_image=None, actual_checksum=None, timeout=True, is_wpt_crash_test=True)
    tests.add('imported/w3c/web-platform-tests/some/test-crash-crash.html',
        expected_text=None, actual_text=None, actual_image=None, actual_checksum=None, is_wpt_crash_test=True, crash=True, error='mock-crash-stderr')

    tests.add('imported/w3c/web-platform-tests/crashtests/pass.html',
        expected_text=None, actual_text='ok', actual_image=None, actual_checksum=None, is_wpt_crash_test=True)
    tests.add('imported/w3c/web-platform-tests/crashtests/timeout.html',
        expected_text=None, actual_text=None, actual_image=None, actual_checksum=None, timeout=True, is_wpt_crash_test=True)
    tests.add('imported/w3c/web-platform-tests/crashtests/crash.html',
        expected_text=None, actual_text=None, actual_image=None, actual_checksum=None, crash=True, is_wpt_crash_test=True)
    tests.add('imported/w3c/web-platform-tests/crashtests/dir/test.html',
        expected_text=None, actual_text='ok', actual_image=None, actual_checksum=None, is_wpt_crash_test=True)

    tests.add('variant/variant.any.html')

    return tests


# Here we use a non-standard location for the layout tests, to ensure that
# this works. The path contains a '.' in the name because we've seen bugs
# related to this before.

LAYOUT_TEST_DIR = '/test.checkout/LayoutTests'
PERF_TEST_DIR = '/test.checkout/PerformanceTests'
TEST_DIR = '/mock-checkout'


# Here we synthesize an in-memory filesystem from the test list
# in order to fully control the test output and to demonstrate that
# we don't need a real filesystem to run the tests.
def add_unit_tests_to_mock_filesystem(filesystem):
    # Add the test_expectations file.
    filesystem.maybe_make_directory(LAYOUT_TEST_DIR + '/platform/test')
    if not filesystem.exists(LAYOUT_TEST_DIR + '/platform/test/TestExpectations'):
        filesystem.write_text_file(LAYOUT_TEST_DIR + '/platform/test/TestExpectations', """
Bug(test) failures/expected/crash.html [ Crash ]
Bug(test) failures/expected/leak.html [ Leak ]
Bug(test) failures/expected/flaky-leak.html [ Failure Leak ]
Bug(test) failures/expected/leaky-reftest.html [ ImageOnlyFailure Leak ]
Bug(test) failures/expected/image.html [ ImageOnlyFailure ]
Bug(test) failures/expected/pixel-fail.html [ ImageOnlyFailure ]
Bug(test) failures/expected/audio.html [ Failure ]
Bug(test) failures/expected/image_checksum.html [ ImageOnlyFailure ]
Bug(test) failures/expected/mismatch.html [ ImageOnlyFailure ]
Bug(test) failures/expected/missing_check.html [ Missing Pass ]
Bug(test) failures/expected/missing_image.html [ Missing Pass ]
Bug(test) failures/expected/missing_audio.html [ Missing Pass ]
Bug(test) failures/expected/missing_text.html [ Missing Pass ]
Bug(test) failures/expected/newlines_leading.html [ Failure ]
Bug(test) failures/expected/newlines_trailing.html [ Failure ]
Bug(test) failures/expected/newlines_with_excess_CR.html [ Failure ]
Bug(test) failures/expected/reftest.html [ ImageOnlyFailure ]
Bug(test) failures/expected/text.html [ Failure ]
Bug(test) failures/expected/timeout.html [ Timeout ]
Bug(test) failures/expected/hang.html [ WontFix ]
Bug(test) failures/expected/keyboard.html [ WontFix ]
Bug(test) failures/expected/exception.html [ WontFix ]
Bug(test) failures/unexpected/pass.html [ Failure ]
Bug(test) passes/skipped/skip.html [ Skip ]
Bug(test) corner-cases/ews/directory-skipped [ Skip ]
Bug(test) corner-cases/ews/directory-flaky [ Pass Timeout Failure ]
""")
    w3c_resources_path = LAYOUT_TEST_DIR + '/imported/w3c/resources/'
    if not filesystem.exists(w3c_resources_path + 'resource-files.json'):
        filesystem.maybe_make_directory(w3c_resources_path)
        filesystem.write_text_file(w3c_resources_path + 'resource-files.json', '{"directories": [], "files": []}')

    # FIXME: This test was only being ignored because of missing a leading '/'.
    # Fixing the typo causes several tests to assert, so disabling the test entirely.
    # Add in a file should be ignored by port.find_test_files().
    #files[LAYOUT_TEST_DIR + '/userscripts/resources/iframe.html'] = 'iframe'

    def add_file(test, suffix, contents):
        dirname = filesystem.join(LAYOUT_TEST_DIR, test.name[0:test.name.rfind('/')])
        base = test.base
        filesystem.maybe_make_directory(dirname)

        path = filesystem.join(dirname, base + suffix)
        if contents is None:
            if filesystem.exists(path):
                filesystem.remove(path)
        else:
            filesystem.write_binary_file(path, contents)

    # Add each test and the expected output, if any.
    test_list = unit_test_list()
    for test in test_list.tests.values():
        add_file(test, test.name[test.name.rfind('.'):], '')
        if test.is_reftest:
            continue
        if test.is_wpt_crash_test:
            continue
        if test.actual_audio:
            add_file(test, '-expected.wav', test.expected_audio)
            continue
        add_file(test, '-expected.txt', test.expected_text)
        add_file(test, '-expected.png', test.expected_image)


def add_checkout_information_json_to_mock_filesystem(filesystem):
    if not filesystem.exists(TEST_DIR):
        filesystem.maybe_make_directory(TEST_DIR)
    filesystem.write_text_file(TEST_DIR + '/checkout_information.json', '{ "branch": "trunk", "id": "2738499" }')


class TestPort(Port):
    port_name = 'test'
    default_port_name = 'test-mac-leopard'

    """Test implementation of the Port interface."""
    ALL_BASELINE_VARIANTS = (
        'test-linux-x86_64',
        'test-mac-snowleopard', 'test-mac-leopard',
        'test-win-vista', 'test-win-7sp0', 'test-win-xp',
    )

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name == 'test':
            return TestPort.default_port_name
        return port_name

    def __init__(self, host, port_name=None, **kwargs):
        Port.__init__(self, host, port_name or TestPort.default_port_name, **kwargs)
        self._tests = unit_test_list()
        self._flakes = set()
        self._expectations_path = LAYOUT_TEST_DIR + '/platform/test/TestExpectations'
        self._results_directory = None

        self._operating_system = self._name.split('-')[1]
        if self._operating_system == 'linux':
            self._os_version = None
        else:
            self._os_version = VersionNameMap.map(self.host.platform).from_name(self._name.split('-')[2])[1]

    def version_name(self):
        if self._os_version is None:
            return None
        return VersionNameMap.map(self.host.platform).to_name(self._os_version, platform=self._operating_system, table=PUBLIC_TABLE)

    def default_pixel_tests(self):
        return True

    def _path_to_driver(self):
        # This routine shouldn't normally be called, but it is called by
        # the mock_drt Driver. We return something, but make sure it's useless.
        return 'MOCK _path_to_driver'

    def baseline_search_path(self, **kwargs):
        search_paths = {
            'test-mac-snowleopard': ['test-mac-snowleopard'],
            'test-mac-leopard': ['test-mac-leopard', 'test-mac-snowleopard'],
            'test-win-7sp0': ['test-win-7sp0'],
            'test-win-vista': ['test-win-vista', 'test-win-7sp0'],
            'test-win-xp': ['test-win-xp', 'test-win-vista', 'test-win-7sp0'],
            'test-linux-x86_64': ['test-linux'],
        }
        return [self._webkit_baseline_path(d) for d in search_paths[self.name()]]

    def default_child_processes(self, **kwargs):
        return 1

    def check_build(self):
        return True

    def check_sys_deps(self):
        return True

    def default_configuration(self):
        return 'Release'

    def diff_image(self, expected_contents, actual_contents, tolerance=None):
        expected_contents = string_utils.encode(expected_contents)
        actual_contents = string_utils.encode(actual_contents)
        diffed = actual_contents != expected_contents
        if not actual_contents and not expected_contents:
            return ImageDiffResult(passed=True, diff_image=None, difference=0, tolerance=tolerance or 0)

        if not actual_contents or not expected_contents:
            return ImageDiffResult(passed=False, diff_image=b'', difference=0, tolerance=tolerance or 0)

        if b'ref' in expected_contents:
            assert tolerance == 0
        if diffed:
            return ImageDiffResult(
                passed=False,
                diff_image="< {}\n---\n> {}\n".format(
                    string_utils.decode(expected_contents, target_type=str),
                    string_utils.decode(actual_contents, target_type=str),
                ),
                difference=1,
                tolerance=tolerance or 0,
                fuzzy_data={'max_difference': 10, 'total_pixels': 20})

        return ImageDiffResult(passed=True, diff_image=None, difference=0, tolerance=tolerance or 0, fuzzy_data={'max_difference': 0, 'total_pixels': 0})

    def layout_tests_dir(self):
        return self._filesystem.abspath(LAYOUT_TEST_DIR)

    def perf_tests_dir(self):
        return self._filesystem.abspath(PERF_TEST_DIR)

    def webkit_base(self):
        return '/test.checkout'

    def skipped_layout_tests(self, device_type):
        return super(TestPort, self).skipped_layout_tests(device_type=device_type) | {
            "failures/expected/skip_text.html",
            "failures/unexpected/skip_pass.html",
        }

    def name(self):
        return self._name

    def operating_system(self):
        return self._operating_system

    def default_results_directory(self):
        return '/tmp/layout-test-results'

    def setup_test_run(self, device_type=None):
        pass

    def _driver_class(self):
        return TestDriver

    def path_to_crash_logs(self):
        return self.results_directory()

    def start_http_server(self, additional_dirs=None):
        pass

    def start_websocket_server(self):
        pass

    def stop_http_server(self):
        pass

    def stop_websocket_server(self):
        pass

    def start_web_platform_test_server(self, additional_dirs=None, number_of_servers=None):
        pass

    def stop_web_platform_test_server(self):
        pass

    def web_platform_test_server_doc_root(self):
        return 'imported/w3c/web-platform-tests/'

    def web_platform_test_server_base_http_url(self, localhost_only=False):
        return "http://localhost:8800/"

    def web_platform_test_server_base_https_url(self, localhost_only=False):
        return "https://localhost:8800/"

    def web_platform_test_server_base_h2_url(self, localhost_only=False):
        return "https://localhost:9000/"

    def _path_to_lighttpd(self):
        return "/usr/sbin/lighttpd"

    def _path_to_lighttpd_modules(self):
        return "/usr/lib/lighttpd"

    def _path_to_apache(self):
        return "/usr/sbin/httpd"

    def _path_to_apache_config_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf', 'httpd.conf')

    def path_to_test_expectations_file(self):
        return self._expectations_path

    def all_test_configurations(self):
        """Returns a sequence of the TestConfigurations the port supports."""
        # By default, we assume we want to test every graphics type in
        # every configuration on every system.
        test_configurations = []
        for version, architecture in self._all_systems():
            for build_type in self._all_build_types():
                test_configurations.append(TestConfiguration(
                    version=version,
                    architecture=architecture,
                    build_type=build_type))
        return test_configurations

    def _all_systems(self):
        return (('leopard', 'x86'),
                ('snowleopard', 'x86'),
                ('xp', 'x86'),
                ('vista', 'x86'),
                ('7sp0', 'x86'),
                ('lucid', 'x86'),
                ('lucid', 'x86_64'))

    def _all_build_types(self):
        return ('debug', 'release')

    def configuration_specifier_macros(self):
        """To avoid surprises when introducing new macros, these are intentionally fixed in time."""
        return {'mac': ['leopard', 'snowleopard'], 'win': ['xp', 'vista', '7sp0'], 'linux': ['lucid']}

    def all_baseline_variants(self):
        return self.ALL_BASELINE_VARIANTS


class TestDriver(Driver):
    """Test/Dummy implementation of the DumpRenderTree interface."""
    next_pid = 1

    def __init__(self, *args, **kwargs):
        super(TestDriver, self).__init__(*args, **kwargs)
        self.started = False
        self.pid = 0

    def cmd_line(self, pixel_tests, per_test_args):
        pixel_tests_flag = '-p' if pixel_tests else ''
        return [self._port._path_to_driver()] + [pixel_tests_flag] + self._port.get_option('additional_drt_flag', []) + per_test_args

    def run_test(self, test_input, stop_when_done):
        if not self.started:
            self.started = True
            self.pid = TestDriver.next_pid
            TestDriver.next_pid += 1

        start_time = time.time()
        test_name = test_input.test_name
        test_args = test_input.args or []
        test = self._port._tests[test_name]
        if test.keyboard:
            raise KeyboardInterrupt()
        if test.exception:
            raise ValueError('exception from ' + test_name)
        if test.hang:
            time.sleep((float(test_input.timeout) * 4) / 1000.0 + 1.0)  # The 1.0 comes from thread_padding_sec in layout_test_runnery.

        audio = None
        actual_text = test.actual_text

        if 'flaky' in test_name and not test_name in self._port._flakes:
            self._port._flakes.add(test_name)
            actual_text = 'flaky text failure'

        if actual_text and test_args and test_name == 'passes/args.html':
            actual_text = actual_text + ' ' + ' '.join(test_args)

        if test.actual_audio:
            audio = test.actual_audio
        crashed_process_name = None
        crashed_pid = None
        if test.crash:
            crashed_process_name = self._port.driver_name()
            crashed_pid = 1
        elif test.web_process_crash:
            crashed_process_name = 'WebProcess'
            crashed_pid = 2

        crash_log = ''
        if crashed_process_name:
            crash_logs = CrashLogs(self._port.host, self._port.path_to_crash_logs())
            crash_log = crash_logs.find_newest_log(crashed_process_name, None) or ''

        if stop_when_done:
            self.stop()

        if test.actual_checksum == test_input.image_hash:
            image = None
        else:
            image = test.actual_image
        return DriverOutput(actual_text, image, test.actual_checksum, audio,
            crash=test.crash or test.web_process_crash, crashed_process_name=crashed_process_name,
            crashed_pid=crashed_pid, crash_log=crash_log,
            test_time=time.time() - start_time, timeout=test.timeout, error=test.error, pid=self.pid)

    def do_post_tests_work(self):
        if not self._port.get_option('world_leaks'):
            return None

        test_world_leaks_output = """TEST: file:///test.checkout/LayoutTests/failures/expected/leak.html
ABANDONED DOCUMENT: file:///test.checkout/LayoutTests/failures/expected/leak.html
TEST: file:///test.checkout/LayoutTests/failures/unexpected/leak.html
ABANDONED DOCUMENT: file:///test.checkout/LayoutTests/failures/expected/flaky-leak.html
TEST: file:///test.checkout/LayoutTests/failures/unexpected/flaky-leak.html
ABANDONED DOCUMENT: file:///test.checkout/LayoutTests/failures/expected/leak.html
TEST: file:///test.checkout/LayoutTests/failures/unexpected/leak.html
ABANDONED DOCUMENT: file:///test.checkout/LayoutTests/failures/expected/leak-subframe.html
TEST: file:///test.checkout/LayoutTests/failures/expected/leaky-reftest.html
ABANDONED DOCUMENT: file:///test.checkout/LayoutTests/failures/expected/leaky-reftest.html"""
        return self._parse_world_leaks_output(test_world_leaks_output)

    def stop(self):
        self.started = False
