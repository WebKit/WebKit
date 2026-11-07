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

import hashlib
import json
import logging
import re
import time

from urllib.parse import urlparse

from webkitcorepy import Timeout

_log = logging.getLogger(__name__)


class ReferenceCache(object):
    """Image hashes of reftest references, kept between runs in one file per driver configuration."""

    # URLs a reference may load, which go into its cache key with their contents.
    SUBRESOURCE_RE = re.compile(r"""(?:\b(?:src|href|data|poster|background)\s*=\s*["']?|url\(\s*["']?|@import\s+["'])([^"'\s>)]+)""", re.IGNORECASE)
    LIFETIME_DAYS = 30

    def __init__(self, port, driver):
        self._port = port
        self._filesystem = port.host.filesystem
        self._path = self.hashes_path(port, driver)
        self._read_hashes = self._read(self._path)
        self._hashes = dict(self._read_hashes)

    # Each driver configuration gets its own file, so a flag or device that changes rendering never shares hashes.
    @staticmethod
    def hashes_path(port, driver):
        configuration = json.dumps([port.name(), str(driver.host.device_type), driver.cmd_line(True, []), port.reference_hashes_configuration()])
        name = '{}-{}.json'.format(port.name(), hashlib.sha1(configuration.encode('utf-8')).hexdigest()[:16])
        return port.host.filesystem.join(port.reference_hashes_directory(), name)

    @staticmethod
    def today():
        return int(time.time() // (24 * 60 * 60))

    def key(self, reference_filename):
        """Returns the key for the reference and everything it loads, or None when it can't be cached."""
        if '?' in reference_filename or '#' in reference_filename:
            return None
        files = self._files_to_hash(reference_filename, set())
        if files is None:
            return None
        directory = self._filesystem.dirname(reference_filename)
        digest = hashlib.sha1()
        for path, contents in sorted((self._filesystem.relpath(path, directory) if path != reference_filename else '', contents) for path, contents in files):
            digest.update(path.encode('utf-8') + b'\0' + contents + b'\0')
        # WebKitTestRunner and DumpRenderTree scale pages whose path contains /hidpi-.
        name = self._filesystem.basename(reference_filename)
        scale = 'hidpi-3x-' if name.startswith('hidpi-3x-') else 'hidpi-' if name.startswith('hidpi-') else ''
        return '{}/{}*{} {}'.format(self._port.relative_test_filename(directory), scale, self._filesystem.splitext(name)[1], digest.hexdigest())

    def image_hash(self, key):
        return self._hashes.get(key, [None])[0] or None

    def remember(self, key, image_hash):
        self._hashes[key] = [image_hash, self.today()]

    def forget(self, key):
        self._hashes[key] = ['', self.today()]

    def save(self):
        """Merges this run's hashes into the file, dropping forgotten hashes and those nobody has used for a while, then other configurations' unused files."""
        # A --run-singly test thread that timed out can still be writing to the dictionary.
        hashes = dict(self._hashes)
        changed = {key: value for key, value in hashes.items() if self._read_hashes.get(key) != value}
        if not changed:
            return

        oldest = self.today() - self.LIFETIME_DAYS
        try:
            self._filesystem.maybe_make_directory(self._filesystem.dirname(self._path))
            with self._port.host.make_file_lock(self._path + '.lock'):
                saved = self._read(self._path)
                saved.update(changed)
                saved = {key: value for key, value in saved.items() if value[0] and value[1] >= oldest}
                self._filesystem.write_text_file(self._path + '.tmp', json.dumps(saved, indent=4, sort_keys=True))
                self._filesystem.move(self._path + '.tmp', self._path)
        except (OSError, Timeout.Exception) as error:
            _log.warning(u'Could not save reference hashes to {}: {}'.format(self._path, error))
            return

        for other in self._filesystem.glob(self._filesystem.join(self._filesystem.dirname(self._path), '*.json')):
            try:
                if other != self._path and self._filesystem.mtime(other) < oldest * 24 * 60 * 60:
                    self._filesystem.remove(other)
                    self._filesystem.remove(other + '.lock')
            except OSError:
                pass

    def _read(self, path):
        if not self._filesystem.exists(path):
            return {}
        try:
            hashes = json.loads(self._filesystem.read_text_file(path))
        except (OSError, ValueError) as error:
            _log.warning(u'Ignoring reference hashes in {}: {}'.format(path, error))
            return {}
        if not isinstance(hashes, dict):
            _log.warning(u'Ignoring reference hashes in {}: not an object'.format(path))
            return {}
        return {key: value for key, value in hashes.items() if isinstance(value, list) and len(value) == 2 and isinstance(value[0], str) and isinstance(value[1], int)}

    def _files_to_hash(self, path, seen):
        """Returns the reference's file and everything it loads, or None when the reference could load something else."""
        if path in seen:
            return []
        seen.add(path)
        if '.sub.' in self._filesystem.basename(path) or not self._filesystem.isfile(path):
            return None
        contents = self._filesystem.read_binary_file(path)
        files = [(path, contents)]
        directory_headers = self._filesystem.join(self._filesystem.dirname(path), '__dir__')
        for headers in (path + '.headers', path + '.sub.headers', directory_headers + '.headers', directory_headers + '.sub.headers'):
            if self._filesystem.isfile(headers):
                files.append((headers, self._filesystem.read_binary_file(headers)))
        if self._filesystem.splitext(path)[1].lower() not in ('.html', '.htm', '.xht', '.xhtml', '.xml', '.svg', '.css'):
            return files
        text = contents.decode('utf-8', 'replace')
        if '<script' in text.lower() or 'srcset' in text.lower():
            return None
        for url in self.SUBRESOURCE_RE.findall(text):
            subresource = self._resolve_subresource(path, url)
            if subresource is False:
                continue
            subresource_files = None if subresource is None else self._files_to_hash(subresource, seen)
            if subresource_files is None:
                return None
            files.extend(subresource_files)
        return files

    def _resolve_subresource(self, path, url):
        """Returns the file a reference loads for url, False if it loads nothing, or None if what it loads can't be known."""
        if not url or url.startswith(('#', 'data:', 'mailto:', 'about:')):
            return False
        if '?' in url:
            return None
        host = urlparse(url).hostname if re.match(r'^[a-z][a-z0-9+.-]*:', url, re.IGNORECASE) else ''
        if host is None or host in ('localhost', '127.0.0.1') or host.endswith(('.localhost', 'web-platform.test')):
            return None
        if host:
            return False
        url = url.split('#')[0]
        if not url.startswith('/'):
            return self._filesystem.normpath(self._filesystem.join(self._filesystem.dirname(path), url))
        test_path = self._port.relative_test_filename(path)
        wpt_root = self._port.web_platform_test_server_doc_root()
        for directory, root in ((wpt_root, wpt_root), ('http/wpt/', wpt_root), ('http/tests/', 'http/tests/')):
            if test_path.startswith(directory):
                return self._filesystem.join(self._port.layout_tests_dir(), root, url[1:])
        return None
