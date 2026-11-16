# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
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

from __future__ import annotations

import os
import sys
import tempfile

from webkitcorepy.autoinstall import AutoInstall, Package
from webkitcorepy.version import Version

AutoInstall.register(Package('mypy', Version(1, 16, 1)))
AutoInstall.register(Package('mypy_extensions', Version(1, 1, 0)))
AutoInstall.register(Package('pathspec', Version(0, 12, 1)))
AutoInstall.register(Package('typing_extensions', Version(4, 13, 2), wheel=True))


def run_mypy(config_file: str | os.PathLike[str], package: str) -> tuple[int, str]:
    """Type-check `package` using the mypy configuration in `config_file`.

    Returns mypy's exit status and the report it printed.
    """
    import mypy.api

    cache_dir = os.path.join(AutoInstall.directory or tempfile.gettempdir(), 'mypy-cache', package)

    # When run in-process, mypy looks for installed packages and stubs on sys.path, but
    # always skips its first entry. Test runners put different directories there
    # (AutoInstall's, for instance), so give mypy a placeholder entry to skip instead.
    original_path = sys.path
    sys.path = [''] + sys.path
    try:
        stdout, stderr, status = mypy.api.run([
            '--config-file', os.fspath(config_file),
            '--cache-dir', cache_dir,
            '--pretty',
            '--no-color-output',
            '-p', package,
        ])
    finally:
        sys.path = original_path
    return status, stdout if stdout and not stdout.isspace() else stderr
