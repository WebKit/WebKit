# Copyright (C) 2026 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import json
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.layout_tests.controllers import reference_cache
from webkitpy.port.test import TestPort


class ReferenceHashesFileTest(unittest.TestCase):
    path = '/mock-build/reference-hashes/test.json'

    class FakeDriver:
        def __init__(self, host, flags):
            self.host = host
            self.flags = flags

        def cmd_line(self, pixel_tests, per_test_args):
            return ['WebKitTestRunner'] + self.flags

    def test_writing_keeps_hashes_it_did_not_change(self):
        host = MockHost()
        today = reference_cache.today()
        host.filesystem.write_text_file(self.path, json.dumps({'kept': ['1', today], 'changed': ['2', today], 'stale': ['5', today - 31]}))
        reference_cache.write_hashes(host, self.path, {'changed': ['3', today], 'new': ['4', today]})
        self.assertEqual(reference_cache.read_hashes(host.filesystem, self.path), {'kept': ['1', today], 'changed': ['3', today], 'new': ['4', today]})

    def test_forgotten_hashes_are_removed(self):
        host = MockHost()
        today = reference_cache.today()
        host.filesystem.write_text_file(self.path, json.dumps({'kept': ['1', today], 'forgotten': ['2', today]}))
        hashes = {}
        reference_cache.forget(hashes, 'forgotten')
        reference_cache.write_hashes(host, self.path, hashes)
        self.assertEqual(reference_cache.read_hashes(host.filesystem, self.path), {'kept': ['1', today]})

    def test_unused_files_of_other_configurations_are_removed(self):
        host = MockHost()
        other = '/mock-build/reference-hashes/other.json'
        host.filesystem.write_text_file(other, '{}')
        host.filesystem.write_text_file(other + '.lock', '')
        reference_cache.write_hashes(host, self.path, {'new': ['4', reference_cache.today()]})
        self.assertTrue(host.filesystem.exists(self.path))
        self.assertFalse(host.filesystem.exists(other))
        self.assertFalse(host.filesystem.exists(other + '.lock'))

    def test_unreadable_hashes_are_ignored(self):
        host = MockHost()
        for contents in ('not json', 'null', '[]', '{"key": "not a list"}'):
            host.filesystem.write_text_file(self.path, contents)
            self.assertEqual(reference_cache.read_hashes(host.filesystem, self.path), {}, contents)

    def test_failure_to_write_is_not_fatal(self):
        host = MockHost()

        def refuse(path, contents):
            raise OSError('read-only')

        host.filesystem.write_text_file = refuse
        reference_cache.write_hashes(host, self.path, {'new': ['4', reference_cache.today()]})

    def test_port_rendering_configuration_gets_its_own_file(self):
        port = TestPort(MockHost())
        driver = self.FakeDriver(port.host, [])
        paths = set()
        for configuration in ([], ['wayland', 'gpu']):
            port.reference_hashes_configuration = lambda: configuration
            paths.add(reference_cache.hashes_path(port, driver))
        self.assertEqual(len(paths), 2)

    def test_driver_flags_get_their_own_file(self):
        port = TestPort(MockHost())
        self.assertNotEqual(reference_cache.hashes_path(port, self.FakeDriver(port.host, [])), reference_cache.hashes_path(port, self.FakeDriver(port.host, ['--site-isolation-enabled-by-default'])))


class ReferenceCacheKeyTest(unittest.TestCase):

    def _key(self, files, reference='cache/ref.html'):
        port = TestPort(MockHost())
        for name, contents in files.items():
            directory = port.host.filesystem.join(port.layout_tests_dir(), port.host.filesystem.dirname(name))
            port.host.filesystem.maybe_make_directory(directory)
            port.host.filesystem.write_binary_file(port.host.filesystem.join(port.layout_tests_dir(), name), contents)
        return reference_cache.cache_key(port, port.host.filesystem.join(port.layout_tests_dir(), reference))

    def test_key_changes_with_what_the_reference_loads(self):
        page = b'<link rel=stylesheet href="ref.css"><p>x</p>'
        before = self._key({'cache/ref.html': page, 'cache/ref.css': b'@import "other.css";', 'cache/other.css': b'p { background: url(img.png) }', 'cache/img.png': b'1'})
        after = self._key({'cache/ref.html': page, 'cache/ref.css': b'@import "other.css";', 'cache/other.css': b'p { background: url(img.png) }', 'cache/img.png': b'2'})
        self.assertIsNotNone(before)
        self.assertNotEqual(before, after)

    def test_root_relative_urls_resolve_in_web_platform_tests(self):
        page = b'<link rel=stylesheet href="/fonts/ahem.css"><p>x</p>'
        reference = 'imported/w3c/web-platform-tests/css/ref.html'
        before = self._key({reference: page, 'imported/w3c/web-platform-tests/fonts/ahem.css': b'a'}, reference)
        after = self._key({reference: page, 'imported/w3c/web-platform-tests/fonts/ahem.css': b'b'}, reference)
        self.assertIsNotNone(before)
        self.assertNotEqual(before, after)

    def test_poster_and_background_are_loads(self):
        for page in (b'<video poster="frame.png"></video>', b'<table background="frame.png"></table>'):
            before = self._key({'cache/ref.html': page, 'cache/frame.png': b'1'})
            after = self._key({'cache/ref.html': page, 'cache/frame.png': b'2'})
            self.assertIsNotNone(before, page)
            self.assertNotEqual(before, after, page)

    def test_key_changes_with_the_headers_served_with_the_reference(self):
        for headers in ('cache/ref.html.headers', 'cache/ref.html.sub.headers', 'cache/__dir__.headers', 'cache/__dir__.sub.headers'):
            before = self._key({'cache/ref.html': b'<p>x</p>', headers: b'Content-Type: text/html'})
            after = self._key({'cache/ref.html': b'<p>x</p>', headers: b'Content-Type: text/plain'})
            self.assertNotEqual(before, after, headers)

    def test_identical_references_in_a_directory_share_a_key(self):
        files = {'cache/a.html': b'<p>same</p>', 'cache/b.html': b'<p>same</p>'}
        self.assertEqual(self._key(files, 'cache/a.html'), self._key(files, 'cache/b.html'))

    def test_hidpi_references_get_their_own_key(self):
        files = {'cache/ref.html': b'<p>same</p>', 'cache/hidpi-ref.html': b'<p>same</p>'}
        self.assertNotEqual(self._key(files, 'cache/ref.html'), self._key(files, 'cache/hidpi-ref.html'))

    def test_metadata_links_are_not_loads(self):
        self.assertIsNotNone(self._key({'cache/ref.html': b'<link rel=help href="https://drafts.csswg.org/css-flexbox/"><p>x</p>'}))

    def test_references_that_could_load_anything_are_not_cached(self):
        for contents in (b'<script>document.write("x")</script>', b'<img src="missing.png">', b'<img srcset="a.png 1x, b.png 2x">', b'<img src="http://localhost:8000/a.png">', b'<link rel=stylesheet href="ref.css?pipe=sub">'):
            self.assertIsNone(self._key({'cache/ref.html': contents}), contents)
