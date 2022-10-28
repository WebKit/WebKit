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

import logging
import os
import sys

log = logging.getLogger('webkitbugspy')


def _maybe_add_library_path(path):
    # Hopefully we're beside the required library, otherwise that library will need to be installed.
    libraries_path = os.path.dirname(os.path.dirname(os.path.abspath(os.path.dirname(__file__))))
    library_path = os.path.join(libraries_path, path)
    if os.path.isdir(library_path) and os.path.isdir(os.path.join(library_path, path)) and library_path not in sys.path:
        sys.path.insert(0, library_path)


_maybe_add_library_path('webkitcorepy')

try:
    from webkitcorepy import AutoInstall, Package, Version
except ImportError:
    raise ImportError(
        "'webkitcorepy' could not be found on your Python path.\n" +
        "You are not running from a WebKit checkout.\n" +
        "Please install webkitcorepy with `pip install webkitcorepy --extra-index-url <package index URL>`"
    )

version = Version(0, 8, 4)

from .user import User
from .issue import Issue
from .tracker import Tracker

name = 'webkitbugspy'
