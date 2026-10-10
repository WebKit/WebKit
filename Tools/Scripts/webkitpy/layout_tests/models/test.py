# Copyright (C) 2021 Apple Inc. All rights reserved.
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
#     * Neither the name of Apple Inc. nor the names of its
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


import re

import attr

from webkitpy.layout_tests.models.server_routing import ServerType


@attr.s(frozen=True, slots=True)
class Reference(object):
    relation = attr.ib(type=str)  # "==" or "!="
    path = attr.ib(type=str)  # absolute filesystem path


_digit_re = re.compile(r"(\d+)")


def _natsort_key(string):
    split = _digit_re.split(string)
    split[1::2] = [(int(i), i) for i in split[1::2]]
    return split


def test_name_and_variant(test_name):
    """Splits a test name into the filename part and the variant part."""
    idx = len(test_name)
    for sep in ('?', '#'):
        pos = test_name.find(sep)
        if pos != -1 and pos < idx:
            idx = pos
    return (test_name[:idx], test_name[idx:])


def _file_path_sort_key(test_path):
    file_path = test_name_and_variant(test_path)[0]
    dirname, basename = file_path.rsplit('/', 1) if '/' in file_path else ('', file_path)
    return (_natsort_key(dirname + '/'), _natsort_key(basename))


@attr.s(frozen=True, slots=True)
class Test(object):
    """Data about a test and its expectations.

    Note that this is inherently platform specific, as expectations are platform specific."""
    test_path = attr.ib(type=str, order=_file_path_sort_key)
    file_path = attr.ib(type=str, order=False)
    expected_text_path = attr.ib(default=None, type=str, order=False)
    expected_image_path = attr.ib(default=None, type=str, order=False)
    expected_audio_path = attr.ib(default=None, type=str, order=False)
    reference_files = attr.ib(default=None, type=list, order=False)
    served_by = attr.ib(default=ServerType.FILE, type=ServerType, order=False)
    is_crash_test = attr.ib(default=False, type=bool, order=False)
    fuzzy = attr.ib(default=None, eq=False, order=False)
    _flags = attr.ib(default=frozenset(), type=frozenset, order=False)

    @file_path.default
    def _file_path_factory(self):
        return test_name_and_variant(self.test_path)[0]

    @property
    def variant(self):
        return test_name_and_variant(self.test_path)[1]

    @property
    def https(self):
        return "https" in self._flags

    @property
    def h2(self):
        return "h2" in self._flags

    @property
    def subdomain(self):
        return "sub" in self._flags

    @property
    def needs_http_server(self):
        return bool(self.served_by & ServerType.HTTP)

    @property
    def needs_websocket_server(self):
        return bool(self.served_by & ServerType.WEBSOCKET) and (self.needs_http_server or self.needs_wpt_server)

    @property
    def needs_wpt_server(self):
        return bool(self.served_by & ServerType.WPT)

    @property
    def needs_any_server(self):
        return self.needs_http_server or self.needs_websocket_server or self.needs_wpt_server
