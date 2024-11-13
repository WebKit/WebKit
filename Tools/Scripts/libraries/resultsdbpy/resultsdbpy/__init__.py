# Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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

import sys

if sys.version_info < (3, 9):  # noqa: UP036
    raise ImportError("resultsdbpy requires Python 3.9 or above")

import os
import platform


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
        "You are not running from a complete WebKit checkout.\n" +
        "See https://github.com/WebKit/WebKit/tree/main/Tools/Scripts/libraries/webkitcorepy"
    )

version = Version(3, 1, 9)

import webkitflaskpy

AutoInstall.register(Package('aenum', Version(2, 2, 6)))
AutoInstall.register(Package('attrs', Version(21, 3, 0)))
AutoInstall.register(Package('aioredis', Version(1, 3, 1)))
AutoInstall.register(Package('async-timeout', Version(3, 0, 1)))

if sys.version_info >= (3, 9):
    AutoInstall.register(Package('boto3', Version(1, 33, 10), wheel=True))
    AutoInstall.register(Package('botocore', Version(1, 34, 5), wheel=True))
    AutoInstall.register(Package('jmespath', Version(1, 0, 1), wheel=True))
else:
    AutoInstall.register(Package('boto3', Version(1, 16, 63), wheel=True))
    AutoInstall.register(Package('botocore', Version(1, 19, 63), wheel=True))
    AutoInstall.register(Package('jmespath', Version(0, 10, 0), wheel=True))

if platform.machine() == 'arm64':
    AutoInstall.register(Package('cassandra', Version(3, 25, 0), pypi_name='cassandra-driver', slow_install=True))
else:
    AutoInstall.register(Package('cassandra', Version(3, 25, 0), pypi_name='cassandra-driver', wheel=True))
AutoInstall.register(Package('Crypto', Version(3, 10, 1), pypi_name='pycryptodome'))
AutoInstall.register(Package('fakeredis', Version(1, 5, 2)))
AutoInstall.register(Package('geomet', Version(0, 2, 1)))
AutoInstall.register(Package('gremlinpython', Version(3, 4, 6)))
AutoInstall.register(Package('isodate', Version(0, 6, 0)))
AutoInstall.register(Package('pyasn1_modules', Version(0, 2, 8), pypi_name='pyasn1-modules'))
AutoInstall.register(Package('redis', Version(3, 5, 3)))
if sys.version_info < (3, 8):
    AutoInstall.register(Package('selenium', Version(3, 141, 0)))
else:
    if sys.platform == 'linux':
        AutoInstall.register(Package('selenium', Version(4, 19, 0), wheel=True))
    else:
        AutoInstall.register(Package('selenium', Version(4, 12, 0), wheel=True))
AutoInstall.register(Package('service_identity', Version(21, 1, 0), pypi_name='service-identity'))
AutoInstall.register(Package('sortedcontainers', Version(2, 4, 0)))
AutoInstall.register(Package('tornado', Version(4, 5, 3)))
AutoInstall.register(Package('twisted', Version(21, 2, 0), pypi_name='Twisted', implicit_deps=[
    Package('incremental', Version(21, 3, 0)),
    'OpenSSL',
]))

name = 'resultsdbpy'
