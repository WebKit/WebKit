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

"""generic routines to convert platform-specific paths to URIs."""
import sys
import urllib


def abspath_to_uri(path, executive, platform=None):
    """Converts a platform-specific absolute path to a file: URL."""
    if platform is None:
        platform = sys.platform
    return "file:" + _escape(_convert_path(path, executive, platform))


def cygpath(path, executive):
    """Converts a cygwin path to Windows path."""
    # FIXME: this may not be correct in every situation, but forking
    # cygpath is very slow. More importantly, there is a bug in Python
    # where launching subprocesses and communicating with PIPEs (which
    # is what run_command() does) can lead to deadlocks when running in
    # multiple threads.
    if path.startswith("/cygdrive"):
        path = path[10] + ":" + path[11:]
        path = path.replace("/", "\\")
        return path
    return executive.run_command(['cygpath', '-wa', path],
                                 decode_output=False).rstrip()


def _escape(path):
    """Handle any characters in the path that should be escaped."""
    # FIXME: web browsers don't appear to blindly quote every character
    # when converting filenames to files. Instead of using urllib's default
    # rules, we allow a small list of other characters through un-escaped.
    # It's unclear if this is the best possible solution.
    return urllib.quote(path, safe='/+:')


def _convert_path(path, executive, platform):
    """Handles any os-specific path separators, mappings, etc."""
    if platform == 'win32':
        return _winpath_to_uri(path)
    if platform == 'cygwin':
        return _winpath_to_uri(cygpath(path, executive))
    return _unixypath_to_uri(path)


def _winpath_to_uri(path):
    """Converts a window absolute path to a file: URL."""
    return "///" + path.replace("\\", "/")

def _unixypath_to_uri(path):
    """Converts a unix-style path to a file: URL."""
    return "//" + path
