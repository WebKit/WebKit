# Copyright (C) 2018 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import re


def load(filesystem, path):
    """Load PEM file and return PEM object"""
    return Pem(filesystem.read_text_file(path))


class BadFormatError(Exception):
    """Bad format error"""
    pass


class Pem(object):
    """
    Container for certificate related information.
    Each section in PEM file can be accessible by get().
    e.g.
    pem = pemfile.load(filesystem, "/path/to/sample.pem")
    assert pem.certificate.startswith("-----BEGIN CERTIFICATE-----")
    """

    def __init__(self, content):
        self._contents = _parse_pem_format(content)
        if not self._contents:
            raise BadFormatError("Cannot find any sections in this file.")

    def get(self, kind):
        """Return requested information or None if not found."""
        items = self.get_all(kind)
        if not items:
            raise KeyError("{} is not in this PEM".format(kind))
        return items[0]

    def get_all(self, kind):
        """Return all matching requested information"""
        return [content for (key, content) in self._contents if key == kind]

    @property
    def certificate(self):
        """Return certificate"""
        return self.get(CERTIFICATE)

    @property
    def private_key(self):
        """Return private key"""
        return self.get(PRIVATE_KEY)

    @property
    def csr(self):
        """Return certificate request"""
        return self.get(CERTIFICATE_REQUEST)

    @property
    def certificate_request(self):
        """Alias for csr()"""
        return self.csr

    @property
    def certificate_signing_request(self):
        """Alias for csr()"""
        return self.csr


MARKER = "-----"
BEGIN_MARKER = "BEGIN "
END_MARKER = "END"

CERTIFICATE_REQUEST = "CERTIFICATE REQUEST"
PRIVATE_KEY = "PRIVATE KEY"
CERTIFICATE = "CERTIFICATE"

BEGIN_PATTERN = re.compile("^{}BEGIN (.+){}$".format(MARKER, MARKER))


def _parse_pem_format(content):
    lines = re.split('\r\n?|\n', content)

    def find_begin(lines):
        """
        Find first matching BEGIN marker.
        @returns found key and rest of lines | None
        """
        while lines:
            matched = BEGIN_PATTERN.match(lines[0])
            if matched:
                return (matched.group(1), lines)
            lines = lines[1:]
        return None

    def find_end(kind, lines):
        """
        Find END marker.
        @returns key, found contents and rest of lines.
        @raise BadFormatError
        """
        end_marker = "{}END {}{}".format(MARKER, kind, MARKER)

        try:
            index = lines.index(end_marker)
        except ValueError, e:
            raise BadFormatError("Cannot find section end: {}".format(end_marker))

        return kind, "\n".join(lines[0:index + 1]) + "\n", lines[index + 1:]

    def sections(lines):
        """Section Generator"""
        while lines:
            result = find_begin(lines)
            if not result:
                break
            key, body, lines = find_end(*result)
            yield (key, body)

    return [section for section in sections(lines)]
