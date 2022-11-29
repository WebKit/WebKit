# Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import sys


def _maybe_add_webkit_python_library_paths():
    # Hopefully we're beside webkit*py libraries, otherwise webkit*py will need to be installed.
    libraries_path = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(__file__))))
    for library in ['webkitcorepy', 'webkitscmpy', 'webkitflaskpy']:
        library_path = os.path.join(libraries_path, library)
        if os.path.isdir(library_path) and os.path.isdir(os.path.join(library_path, library)) and library_path not in sys.path:
            sys.path.insert(0, library_path)


_maybe_add_webkit_python_library_paths()

try:
    from webkitcorepy import AutoInstall, Package, Version
except ImportError:
    raise ImportError(
        "'webkitcorepy' could not be found on your Python path.\n" +
        "You are not running from a WebKit checkout.\n" +
        "Please install webkitcorepy with `pip install webkitcorepy --extra-index-url <package index URL>`"
    )

version = Version(0, 7, 7)

import webkitflaskpy

from reporelaypy.checkout import Checkout
from reporelaypy.database import Database
from reporelaypy.checkoutroute import CheckoutRoute, Redirector
from reporelaypy.hooks import HookProcessor, HookReceiver

AutoInstall.register(Package('fakeredis', Version(1, 5, 2)))
AutoInstall.register(Package('hiredis', Version(1, 1, 0)))
AutoInstall.register(Package('lupa', Version(1, 13)))
AutoInstall.register(Package('redis', Version(3, 5, 3)))
AutoInstall.register(Package('sortedcontainers', Version(2, 4, 0)))

name = 'reporelaypy'
