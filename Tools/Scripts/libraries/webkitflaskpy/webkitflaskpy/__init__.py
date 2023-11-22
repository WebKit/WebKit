# Copyright (C) 2020 Apple Inc. All rights reserved.
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


def _maybe_add_webkitcorepy_path():
    # Hopefully we're beside webkitcorepy, otherwise webkitcorepy will need to be installed.
    libraries_path = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(__file__))))
    webkitcorepy_path = os.path.join(libraries_path, 'webkitcorepy')
    if os.path.isdir(webkitcorepy_path) and os.path.isdir(os.path.join(webkitcorepy_path, 'webkitcorepy')) and webkitcorepy_path not in sys.path:
        sys.path.insert(0, webkitcorepy_path)


_maybe_add_webkitcorepy_path()

try:
    from webkitcorepy import AutoInstall, Package, Version
except ImportError:
    raise ImportError(
        "'webkitcorepy' could not be found on your Python path.\n" +
        "You are not running from a WebKit checkout.\n" +
        "Please install webkitcorepy with `pip install webkitcorepy --extra-index-url <package index URL>`"
    )

version = Version(0, 6, 0)

AutoInstall.register(Package('click', Version(7, 1, 2)))
AutoInstall.register(Package('flask', Version(1, 1, 2)))
AutoInstall.register(Package('hiredis', Version(1, 1, 0)))
AutoInstall.register(Package('itsdangerous', Version(1, 1, 0)))
AutoInstall.register(Package('jinja2', Version(2, 11, 3)))
if sys.version_info > (3, 10):
    AutoInstall.register(Package('lupa', Version(2, 0)))
else:
    AutoInstall.register(Package('lupa', Version(1, 13)))
AutoInstall.register(Package('markupsafe', Version(1, 1, 1)))
AutoInstall.register(Package('redis', Version(3, 5, 3)))
AutoInstall.register(Package('sortedcontainers', Version(2, 4, 0)))
AutoInstall.register(Package('werkzeug', Version(1, 0, 1)))

if sys.version_info > (3, 0):
    AutoInstall.register(Package('fakeredis', Version(1, 5, 2)))
else:
    AutoInstall.register(Package('fakeredis', Version(1, 1, 1)))

from webkitflaskpy.authed_blueprint import AuthedBlueprint  # noqa: E402
from webkitflaskpy.response import Response
from webkitflaskpy.mock_app import mock_app
from webkitflaskpy.database import Database
from webkitflaskpy.ipc_manager import IPCManager

name = 'webkitflaskpy'
