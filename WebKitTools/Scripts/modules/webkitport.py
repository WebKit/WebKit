# Copyright (C) 2009, Google Inc. All rights reserved.
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
#
# WebKit's Python module for understanding the various ports

import os

from optparse import make_option

class WebKitPort():
    # We might need to pass scm into this function for scm.checkout_root
    @classmethod
    def script_path(cls, script_name):
        return os.path.join("WebKitTools", "Scripts", script_name)

    @staticmethod
    def port_options():
        return [
            make_option("--port", action="store", dest="port", default=None, help="Specify a port (e.g., mac, qt, gtk, ...)."),
        ]

    @staticmethod
    def port(options):
        if options.port == "mac":
            return MacPort
        if options.port == "qt":
            return QtPort
        # FIXME: We should default to WinPort on Windows.
        return MacPort

    @classmethod
    def name(cls):
        raise NotImplementedError, "subclasses must implement"

    @classmethod
    def flag(cls):
        raise NotImplementedError, "subclasses must implement"

    @classmethod
    def run_webkit_tests_command(cls):
        return [cls.script_path("run-webkit-tests")]

    @classmethod
    def build_webkit_command(cls):
        return [cls.script_path("build-webkit")]


class MacPort(WebKitPort):
    @classmethod
    def name(cls):
        return "Mac"

    @classmethod
    def flag(cls):
        return "--port=mac"


class QtPort(WebKitPort):
    @classmethod
    def name(cls):
        return "Qt"

    @classmethod
    def flag(cls):
        return "--port=qt"

    @classmethod
    def build_webkit_command(cls):
        command = WebKitPort.build_webkit_command()
        command.append("--qt")
        return command
