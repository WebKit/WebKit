# Copyright (C) 2026 Apple Inc. All rights reserved.
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

import contextlib
import unittest
from types import SimpleNamespace
from unittest import mock

from webkitcorepy import StringIO, OutputCapture

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests import run_webkit_tests
from webkitpy.layout_tests.controllers.layout_test_runner import Worker
from webkitpy.port import test


@contextlib.contextmanager
def recording_run(test_names, extra_args=None, extra_files=None):
    args = [
        '--platform',
        'test',
        '--no-new-test-results',
        '--no-retry-failures',
        '--child-processes',
        '1',
    ]
    args.extend(extra_args or [])
    args.extend(test_names)
    options, parsed_args = run_webkit_tests.parse_args(args)

    host = MockHost()
    test.add_unit_tests_to_mock_filesystem(host.filesystem)
    fs = host.filesystem
    for name, contents in (extra_files or {}).items():
        fs.write_text_file(fs.join(test.LAYOUT_TEST_DIR, name), contents)
    port = test.TestPort(host, options=options)

    # test_name -> the URL or path it was dispatched with, in call order. A
    # dict of lists (not a Mock's call_args_list) because
    # _run_self_comparison_test mutates one DriverInput across both of its
    # run_test calls, and call_args stores arguments by reference — a value
    # read post-hoc would reflect the later, not the current, call. Keyed per
    # test_name, with a list per key, so a test dispatched more than once (a
    # self-comparison test, or a reftest and its reference sharing a name)
    # keeps every dispatch instead of one clobbering another.
    events = {}
    real_run_test = test.TestDriver.run_test

    def record(self, test_input, stop_when_done):
        command = self._command_from_driver_input(test_input)
        # CommandTokenizer (TestCommand.cpp) splits the whole command on "'",
        # so the first token is pathOrURL and everything after is flags
        # (order-independent, including '--absolutePath') — noise here.
        events.setdefault(test_input.test_name, []).append(command.split("'")[0])
        return real_run_test(self, test_input, stop_when_done)

    with contextlib.ExitStack() as stack:
        stack.enter_context(
            mock.patch.object(port.host.port_factory, 'get', return_value=port)
        )
        stack.enter_context(
            mock.patch.object(
                test.TestDriver, 'run_test', autospec=True, side_effect=record
            )
        )
        # TestInput (unlike DriverInput) is a frozen attr.s, so it's safe to
        # read straight off call_args_list post-hoc instead of recording
        # derived values eagerly into `events`. wraps= can't be used here:
        # under autospec, _setup_func reads mock.return_value (auto-vivifying
        # it) before _mock_delegate is wired up, so _execute_mock_call's
        # return_value check fires and short-circuits before it ever reaches
        # the wraps branch — a real CPython mock bug, present in 3.9 through
        # at least 3.14. side_effect is checked earlier in that dispatch, so
        # it isn't affected, and calls are recorded unconditionally either way.
        # autospec=True is still required (not just for signature-checking):
        # Worker.run_test is patched on the class, called via an instance, and
        # only autospec's generated funcopy function is real enough to get
        # normal bound-method self-binding — a bare Mock has no __get__.
        worker_run_test = stack.enter_context(
            mock.patch.object(Worker, 'run_test', autospec=True, side_effect=Worker.run_test)
        )
        # Patched on the *instance*, so wraps=getattr(port, name) is already
        # bound — no autospec/self-binding concerns, unlike Worker.run_test.
        servers = {
            name: stack.enter_context(
                mock.patch.object(port, name, wraps=getattr(port, name))
            )
            for name in (
                'start_http_server',
                'start_websocket_server',
                'start_web_platform_test_server',
            )
        }
        with OutputCapture():
            run_webkit_tests.run(port, options, parsed_args, logging_stream=StringIO())
        classifications = [
            call.args[1] for call in worker_run_test.call_args_list
        ]
        yield SimpleNamespace(
            port=port, events=events, classifications=classifications, servers=servers
        )


def assert_servers_started(servers, test):
    for key, needed in (
        ('start_http_server', test.needs_http_server),
        ('start_websocket_server', test.needs_websocket_server),
        ('start_web_platform_test_server', test.needs_wpt_server),
    ):
        if needed:
            servers[key].assert_called_once()
        else:
            servers[key].assert_not_called()


harness = '<script src="/resources/testharness.js"></script>'


class UrlAndServerIntegrationTest(unittest.TestCase):
    def _assert_dispatch(self, path, command, content):
        with recording_run([path], extra_files={path: content}) as run:
            d = run.port.layout_tests_dir()

            self.assertEqual(len(run.classifications), 1, 'Only a single test should be run')
            classified_test = run.classifications[0].test
            self.assertEqual(classified_test.test_path, path, 'The single test run should match the path of the test case')

            with self.subTest('Driver command'):
                self.assertEqual(
                    run.events,
                    {path: [command.format(d=d)]},
                )

            with self.subTest('Servers started'):
                assert_servers_started(run.servers, classified_test)

            return classified_test

    def test_dispatch_basic(self):
        for path, command in [
            ('passes/text.html', '{d}/passes/text.html'),
            # Classified as a websocket test via the "websocket" substring, but since the
            # websocket server only starts when http or wpt also starts (see
            # layout_test_runner.py:121), and this test needs neither, no
            # server actually starts.
            ('websocket/tests/passes/text.html', '{d}/websocket/tests/passes/text.html'),
            # Same "websocket/" directory match, but nested under
            # http/tests/ this time, so the websocket server does piggyback
            # on the http server actually starting.
            (
                'http/tests/websocket/passes/text.html',
                'http://127.0.0.1:8000/websocket/passes/text.html',
            ),
            ('http/tests/passes/text.html', 'http://127.0.0.1:8000/passes/text.html'),
            ('http/tests/ssl/text.html', 'https://127.0.0.1:8443/ssl/text.html'),
            # Injected paths need only a filesystem write, thanks to
            # TestList.__getitem__ synthesizing a TestInstance for any name
            # not in the canned unit_test_list().
            ('fast/brand/new.html', '{d}/fast/brand/new.html'),
            # media/ is aliased to /media-resources by Apache, but that alias
            # does not make it a test root; the test loads from disk.
            ('media/brand-new.html', '{d}/media/brand-new.html'),
            ('ipc/brand-new.html', '{d}/ipc/brand-new.html'),
            # Classified as needing HTTP but deliberately loaded from disk —
            # the one case here where an http(s):// URL is NOT implied by
            # needing http.
            ('http/tests/local/foo.html', '{d}/http/tests/local/foo.html'),
            ('http/tests/ssl/brand-new.html', 'https://127.0.0.1:8443/ssl/brand-new.html'),
            ('http/tests/foo/bar/deep.html', 'http://127.0.0.1:8000/foo/bar/deep.html'),
            ('http/tests/ssl/foo/bar/deep.html', 'https://127.0.0.1:8443/ssl/foo/bar/deep.html'),
            # .https. is a filename flag independent of being under ssl/ — it
            # applies to any HTTP test, not just the ssl/ directory convention.
            (
                'http/tests/security/foo.https.html',
                'https://127.0.0.1:8443/security/foo.https.html',
            ),
        ]:
            with self.subTest(name=path):
                self._assert_dispatch(path, command, '')

    def test_dispatch_wpt(self):
        # TODO: add .www./.serviceworker./.serviceworker-module. cases (bug 318491).
        for path, command in [
            ('imported/w3c/web-platform-tests/some/new.html', 'http://localhost:8800/some/new.html'),
            ('http/wpt/foo/bar.html', 'http://localhost:8800/WebKit/foo/bar.html'),
            ('http/wpt/foo/bar/baz.html', 'http://localhost:8800/WebKit/foo/bar/baz.html'),
            (
                'imported/w3c/web-platform-tests/some/fresh.https.html',
                'https://localhost:8800/some/fresh.https.html',
            ),
            ('imported/w3c/web-platform-tests/foo/bar/deep.html', 'http://localhost:8800/foo/bar/deep.html'),
            # .h2. is a filename flag, not a directory, and routes to a
            # distinct port (9000).
            (
                'imported/w3c/web-platform-tests/some/new.h2.html',
                'https://localhost:9000/some/new.h2.html',
            ),
            # _wpt_path_to_uri checks h2 before https/serviceworker: when a
            # filename sets both flags, h2 wins and https is not separately
            # observable.
            (
                'imported/w3c/web-platform-tests/some/new.h2.https.html',
                'https://localhost:9000/some/new.h2.https.html',
            ),
            # A WebSocket-looking path under a WPT root is served by the WPT
            # server; assert_servers_started checks that whether the
            # websocket server starts as well follows Test.
            (
                'imported/w3c/web-platform-tests/websocket/foo.html',
                'http://localhost:8800/websocket/foo.html',
            ),
        ]:
            with self.subTest(name=path):
                self._assert_dispatch(path, command, harness)

    def test_dispatch_near_miss_paths(self):
        # Directory names that look like they should match a prefix check
        # but shouldn't — both revisions agree on all of these.
        for path, command in [
            # A near-miss directory name must not be swept up by the
            # "http/tests" prefix match (startswith is anchored with a
            # trailing '/').
            ('httpfoo/tests/text.html', '{d}/httpfoo/tests/text.html'),
            # "http/tests" broken up by an inserted directory: not an
            # anchored prefix match, and the characters "http/test" don't
            # appear contiguously either.
            ('http/foo/tests/text.html', '{d}/http/foo/tests/text.html'),
            (
                'imported/w3c/foo/web-platform-tests/text.html',
                '{d}/imported/w3c/foo/web-platform-tests/text.html',
            ),
            # Contains the substring "websocket", so it is classified as a
            # websocket test, but that only starts the websocket server when
            # http or wpt also starts; this path needs neither, so no server
            # starts at all — same as origin/main.
            ('passes/text.html?websockets', '{d}/passes/text.html?websockets'),
        ]:
            with self.subTest(name=path):
                self._assert_dispatch(path, command, '')

    def test_dispatch_known_divergences(self):
        # Paths whose classification differs from origin/main's (see the
        # comment on each) — all still resolve to a plain file:// URL, so the
        # divergence has to be asserted explicitly rather than observed via
        # the dispatched command.
        for path, command, servers in [
            # origin/main's is_http_test is `"http/test" in trimmed_path` —
            # an unanchored substring match on "http/test" (singular), so it
            # also fires on "http/testing/" or anywhere else those
            # characters appear. HEAD's routing table anchors on
            # trimmed_path.startswith(prefix + "/"), so a path that merely
            # contains "http/test" without being rooted there is correctly
            # classified as needing no server. On origin/main this
            # classifies as needing the http server.
            ('weird/http/testing/foo.html', '{d}/weird/http/testing/foo.html', (False, False, False)),
            # Same divergence, but with the exact "http/tests" directory
            # nested under an unrelated parent instead of a near-miss name.
            (
                'xxx/http/tests/passes/text.html',
                '{d}/xxx/http/tests/passes/text.html',
                (False, False, False),
            ),
            # Same shape of divergence: origin/main's is_wpt_test is
            # `IMPORTED_WPT_DIR + "/" in trimmed_path` — an unanchored
            # substring check — while HEAD anchors on
            # trimmed_path.startswith(prefix + "/"). On origin/main this
            # classifies as needing the wpt server.
            (
                'weird/imported/w3c/web-platform-tests/foo.html',
                '{d}/weird/imported/w3c/web-platform-tests/foo.html',
                (False, False, False),
            ),
        ]:
            with self.subTest(name=path):
                classified_test = self._assert_dispatch(path, command, '')
                self.assertEqual(
                    (
                        classified_test.needs_http_server,
                        classified_test.needs_websocket_server,
                        classified_test.needs_wpt_server,
                    ),
                    servers,
                )

    def test_single_reference_http_reftest(self):
        with recording_run(
            ['http/tests/reftest/a.html'],
            extra_files={
                'http/tests/reftest/a.html': '',
                'http/tests/reftest/a-expected.html': '',
            },
        ) as run:
            self.assertEqual(
                run.events,
                {
                    'http/tests/reftest/a.html': ['http://127.0.0.1:8000/reftest/a.html'],
                    'http/tests/reftest/a-expected.html': ['http://127.0.0.1:8000/reftest/a-expected.html'],
                },
            )

    def test_reftest_variant_suffix_survives_into_url(self):
        # A '?variant' explicitly requested on the command line (non-WPT tests
        # have no intrinsic variants of their own) is carried verbatim onto
        # both the test's and its reference's URL, even though the file on
        # disk has no such suffix.
        with recording_run(
            ['http/tests/reftest/a.html?var'],
            extra_files={
                'http/tests/reftest/a.html': '',
                'http/tests/reftest/a-expected.html': '',
            },
        ) as run:
            self.assertEqual(
                run.events,
                {
                    'http/tests/reftest/a.html?var': ['http://127.0.0.1:8000/reftest/a.html?var'],
                    'http/tests/reftest/a-expected.html?var': ['http://127.0.0.1:8000/reftest/a-expected.html?var'],
                },
            )

    def test_cross_origin_iframe_loads_file_test_over_http_root_alias(self):
        with recording_run(
            ['passes/text.html'],
            extra_args=['--load-in-cross-origin-iframe'],
        ) as run:
            self.assertEqual(
                run.events,
                {
                    'passes/text.html': ['http://127.0.0.1:8000/root/passes/text.html'],
                },
            )
            run.servers['start_http_server'].assert_called_once_with({'root': '.'})

    def test_cross_origin_iframe_loads_aliased_dir_test_over_http_root_alias(self):
        with recording_run(
            ['media/brand-new.html'],
            extra_args=['--load-in-cross-origin-iframe'],
            extra_files={'media/brand-new.html': ''},
        ) as run:
            self.assertEqual(
                run.events,
                {
                    'media/brand-new.html': ['http://127.0.0.1:8000/root/media/brand-new.html'],
                },
            )

    def test_cross_origin_iframe_keeps_http_local_test_at_http_root(self):
        # http/tests/local is normally loaded from file://, but a cross-origin
        # frame needs HTTP, so it is served at the Apache root like any other
        # http/tests test (not under /root/http/tests/).
        with recording_run(
            ['http/tests/local/foo.html'],
            extra_args=['--load-in-cross-origin-iframe'],
            extra_files={'http/tests/local/foo.html': ''},
        ) as run:
            self.assertEqual(
                run.events,
                {
                    'http/tests/local/foo.html': ['http://127.0.0.1:8000/local/foo.html'],
                },
            )
