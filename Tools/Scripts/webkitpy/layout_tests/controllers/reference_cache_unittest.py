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
from webkitpy.layout_tests.controllers.reference_cache import ReferenceCache
from webkitpy.port.test import TestPort


class FakeDriver:
    def __init__(self, host, flags=None):
        self.host = host
        self.flags = flags or []

    def cmd_line(self, pixel_tests, per_test_args):
        return ['WebKitTestRunner'] + self.flags


class ReferenceCacheFileTest(unittest.TestCase):

    def setUp(self):
        self.port = TestPort(MockHost())
        self.filesystem = self.port.host.filesystem
        self.path = ReferenceCache.hashes_path(self.port, FakeDriver(self.port.host))
        self.today = ReferenceCache.today()

    def _write(self, hashes):
        self.filesystem.maybe_make_directory(self.filesystem.dirname(self.path))
        self.filesystem.write_text_file(self.path, json.dumps(hashes))

    def _saved(self):
        return json.loads(self.filesystem.read_text_file(self.path))

    def _cache(self):
        return ReferenceCache(self.port, FakeDriver(self.port.host))

    def test_saving_keeps_hashes_other_workers_saved(self):
        self._write({'kept': ['1', self.today], 'changed': ['2', self.today]})
        cache = self._cache()
        self._write({'kept': ['1', self.today], 'changed': ['2', self.today], 'other': ['5', self.today]})
        cache.remember('changed', '3')
        cache.remember('new', '4')
        cache.save()
        self.assertEqual(self._saved(), {'kept': ['1', self.today], 'changed': ['3', self.today], 'new': ['4', self.today], 'other': ['5', self.today]})

    def test_saving_drops_hashes_unused_for_a_while(self):
        self._write({'stale': ['1', self.today - ReferenceCache.LIFETIME_DAYS - 1]})
        cache = self._cache()
        cache.remember('new', '2')
        cache.save()
        self.assertEqual(self._saved(), {'new': ['2', self.today]})

    def test_forgotten_hashes_are_removed(self):
        self._write({'kept': ['1', self.today], 'forgotten': ['2', self.today]})
        cache = self._cache()
        cache.forget('forgotten')
        self.assertIsNone(cache.image_hash('forgotten'))
        cache.save()
        self.assertEqual(self._saved(), {'kept': ['1', self.today]})

    def test_hashes_are_read_back(self):
        cache = self._cache()
        cache.remember('key', '1')
        cache.save()
        self.assertEqual(self._cache().image_hash('key'), '1')

    def test_nothing_is_written_when_nothing_changed(self):
        self._cache().save()
        self.assertFalse(self.filesystem.exists(self.path))

    def test_unused_files_of_other_configurations_are_removed(self):
        other = self.filesystem.join(self.filesystem.dirname(self.path), 'other.json')
        self.filesystem.maybe_make_directory(self.filesystem.dirname(self.path))
        self.filesystem.write_text_file(other, '{}')
        self.filesystem.write_text_file(other + '.lock', '')
        cache = self._cache()
        cache.remember('new', '4')
        cache.save()
        self.assertTrue(self.filesystem.exists(self.path))
        self.assertFalse(self.filesystem.exists(other))
        self.assertFalse(self.filesystem.exists(other + '.lock'))

    def test_unreadable_hashes_are_ignored(self):
        for contents in ('not json', 'null', '[]', '{"key": "not a list"}'):
            self.filesystem.maybe_make_directory(self.filesystem.dirname(self.path))
            self.filesystem.write_text_file(self.path, contents)
            self.assertIsNone(self._cache().image_hash('key'), contents)

    def test_failure_to_save_is_not_fatal(self):
        cache = self._cache()
        cache.remember('new', '4')

        def refuse(path, contents):
            raise OSError('read-only')

        self.filesystem.write_text_file = refuse
        cache.save()

    def test_port_rendering_configuration_gets_its_own_file(self):
        paths = set()
        for configuration in ([], ['wayland', 'gpu']):
            self.port.reference_hashes_configuration = lambda: configuration
            paths.add(ReferenceCache.hashes_path(self.port, FakeDriver(self.port.host)))
        self.assertEqual(len(paths), 2)

    def test_driver_flags_get_their_own_file(self):
        self.assertNotEqual(self.path, ReferenceCache.hashes_path(self.port, FakeDriver(self.port.host, ['--site-isolation-enabled-by-default'])))


class ReferenceCacheKeyTest(unittest.TestCase):

    def _key(self, files, reference='cache/ref.html'):
        port = TestPort(MockHost())
        for name, contents in files.items():
            directory = port.host.filesystem.join(port.layout_tests_dir(), port.host.filesystem.dirname(name))
            port.host.filesystem.maybe_make_directory(directory)
            port.host.filesystem.write_binary_file(port.host.filesystem.join(port.layout_tests_dir(), name), contents)
        return ReferenceCache(port, FakeDriver(port.host)).key(port.host.filesystem.join(port.layout_tests_dir(), reference))

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
