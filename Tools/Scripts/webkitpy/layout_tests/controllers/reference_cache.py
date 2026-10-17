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

# URLs a reference may load, which go into its cache key with their contents.
_subresource_pattern = re.compile(r"""(?:\b(?:src|href|data|poster|background)\s*=\s*["']?|url\(\s*["']?|@import\s+["'])([^"'\s>)]+)""", re.IGNORECASE)
_lifetime_days = 30


def _resolve_subresource(port, path, url):
    """Returns the file a reference loads for url, False if it loads nothing, or None if what it loads can't be known."""
    filesystem = port.host.filesystem
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
        return filesystem.normpath(filesystem.join(filesystem.dirname(path), url))
    test_path = port.relative_test_filename(path)
    wpt_root = port.web_platform_test_server_doc_root()
    for directory, root in ((wpt_root, wpt_root), ('http/wpt/', wpt_root), ('http/tests/', 'http/tests/')):
        if test_path.startswith(directory):
            return filesystem.join(port.layout_tests_dir(), root, url[1:])
    return None


def _files_to_hash(port, path, seen):
    """Returns the reference's file and everything it loads, or None when the reference could load something else."""
    filesystem = port.host.filesystem
    if path in seen:
        return []
    seen.add(path)
    if '.sub.' in filesystem.basename(path) or not filesystem.isfile(path):
        return None
    contents = filesystem.read_binary_file(path)
    files = [(path, contents)]
    directory_headers = filesystem.join(filesystem.dirname(path), '__dir__')
    for headers in (path + '.headers', path + '.sub.headers', directory_headers + '.headers', directory_headers + '.sub.headers'):
        if filesystem.isfile(headers):
            files.append((headers, filesystem.read_binary_file(headers)))
    if filesystem.splitext(path)[1].lower() not in ('.html', '.htm', '.xht', '.xhtml', '.xml', '.svg', '.css'):
        return files
    text = contents.decode('utf-8', 'replace')
    if '<script' in text.lower() or 'srcset' in text.lower():
        return None
    for url in _subresource_pattern.findall(text):
        subresource = _resolve_subresource(port, path, url)
        if subresource is False:
            continue
        subresource_files = None if subresource is None else _files_to_hash(port, subresource, seen)
        if subresource_files is None:
            return None
        files.extend(subresource_files)
    return files


def cache_key(port, reference_filename):
    filesystem = port.host.filesystem
    if '?' in reference_filename or '#' in reference_filename:
        return None
    files = _files_to_hash(port, reference_filename, set())
    if files is None:
        return None
    directory = filesystem.dirname(reference_filename)
    digest = hashlib.sha1()
    for path, contents in sorted((filesystem.relpath(path, directory) if path != reference_filename else '', contents) for path, contents in files):
        digest.update(path.encode('utf-8') + b'\0' + contents + b'\0')
    # WebKitTestRunner and DumpRenderTree scale pages whose path contains /hidpi-.
    name = filesystem.basename(reference_filename)
    scale = 'hidpi-3x-' if name.startswith('hidpi-3x-') else 'hidpi-' if name.startswith('hidpi-') else ''
    return '{}/{}*{} {}'.format(port.relative_test_filename(directory), scale, filesystem.splitext(name)[1], digest.hexdigest())


# Hashes persist across runs, so each driver configuration gets its own file: a flag or device that changes rendering never shares hashes.
def hashes_path(port, driver):
    configuration = json.dumps([port.name(), str(driver.host.device_type), driver.cmd_line(True, []), port.reference_hashes_configuration()])
    name = '{}-{}.json'.format(port.name(), hashlib.sha1(configuration.encode('utf-8')).hexdigest()[:16])
    return port.host.filesystem.join(port.reference_hashes_directory(), name)


def today():
    return int(time.time() // (24 * 60 * 60))


def forget(hashes, key):
    """Records that key's reference has no usable hash, so writing removes the saved one."""
    hashes[key] = ['', today()]


def read_hashes(filesystem, path):
    if not filesystem.exists(path):
        return {}
    try:
        hashes = json.loads(filesystem.read_text_file(path))
    except (OSError, ValueError) as error:
        _log.warning(u'Ignoring reference hashes in {}: {}'.format(path, error))
        return {}
    if not isinstance(hashes, dict):
        _log.warning(u'Ignoring reference hashes in {}: not an object'.format(path))
        return {}
    return {key: value for key, value in hashes.items() if isinstance(value, list) and len(value) == 2 and isinstance(value[0], str) and isinstance(value[1], int)}


def write_hashes(host, path, changed):
    """Merges changed into the file, dropping forgotten hashes and those nobody has used for a while, then other configurations' unused files."""
    filesystem = host.filesystem
    oldest = today() - _lifetime_days
    try:
        filesystem.maybe_make_directory(filesystem.dirname(path))
        with host.make_file_lock(path + '.lock'):
            hashes = read_hashes(filesystem, path)
            hashes.update(changed)
            hashes = {key: value for key, value in hashes.items() if value[0] and value[1] >= oldest}
            filesystem.write_text_file(path + '.tmp', json.dumps(hashes, indent=4, sort_keys=True))
            filesystem.move(path + '.tmp', path)
    except (OSError, Timeout.Exception) as error:
        _log.warning(u'Could not save reference hashes to {}: {}'.format(path, error))
        return

    for other in filesystem.glob(filesystem.join(filesystem.dirname(path), '*.json')):
        try:
            if other != path and filesystem.mtime(other) < oldest * 24 * 60 * 60:
                filesystem.remove(other)
                filesystem.remove(other + '.lock')
        except OSError:
            pass
