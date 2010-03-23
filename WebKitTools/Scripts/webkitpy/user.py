# Copyright (c) 2009, Google Inc. All rights reserved.
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

import os
import readline
import shlex
import subprocess
import webbrowser

class User(object):
    @staticmethod
    def prompt(message, repeat=1, raw_input=raw_input):
        response = None
        while (repeat and not response):
            repeat -= 1
            response = raw_input(message)
        return response

    def edit(self, files):
        editor = os.environ.get("EDITOR") or "vi"
        args = shlex.split(editor)
        subprocess.call(args + files)

    def page(self, message):
        pager = os.environ.get("PAGER") or "less"
        try:
            child_process = subprocess.Popen([pager], stdin=subprocess.PIPE)
            child_process.communicate(input=message)
        except IOError, e:
            pass

    def confirm(self, message=None):
        if not message:
            message = "Continue?"
        response = raw_input("%s [Y/n]: " % message)
        return not response or response.lower() == "y"

    def open_url(self, url):
        webbrowser.open(url)
