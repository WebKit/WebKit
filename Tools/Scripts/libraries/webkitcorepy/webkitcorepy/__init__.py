# Copyright (C) 2020-2024 Apple Inc. All rights reserved.
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

import sys

if sys.version_info < (3, 6):
    raise ImportError("webkitcorepy requires Python 3.6 or above")

import logging
import platform

from logging import NullHandler

log = logging.getLogger('webkitcorepy')
log.addHandler(NullHandler())

from webkitcorepy.version import Version
from webkitcorepy.string_utils import BytesIO, StringIO, UnicodeIO, unicode
from webkitcorepy.timeout import Timeout
from webkitcorepy.subprocess_utils import TimeoutExpired, CompletedProcess, run, Thread
from webkitcorepy.output_capture import LoggerCapture, OutputCapture, OutputDuplicate
from webkitcorepy.task_pool import TaskPool
from webkitcorepy.timer import Timer
from webkitcorepy.terminal import Terminal
from webkitcorepy.environment import Environment
from webkitcorepy.credentials import credentials, delete_credentials
from webkitcorepy.measure_time import MeasureTime
from webkitcorepy.nested_fuzzy_dict import NestedFuzzyDict
from webkitcorepy.call_by_need import CallByNeed
from webkitcorepy.editor import Editor
from webkitcorepy.file_lock import FileLock
from webkitcorepy.null_context import NullContext
from webkitcorepy.filtered_call import filtered_call
from webkitcorepy.partial_proxy import PartialProxy

version = Version(1, 0, 1)

from webkitcorepy.autoinstall import Package, AutoInstall
if sys.version_info > (3, 0):
    AutoInstall.register(Package('mock', Version(5, 1, 0), wheel=True))
else:
    AutoInstall.register(Package('mock', Version(3, 0, 5)))
    if platform.system() == 'Windows':
        AutoInstall.register(Package('win_inet_pton', Version(1, 1, 0), pypi_name='win-inet-pton'))

if sys.version_info >= (3, 12):
    AutoInstall.register(Package('setuptools', Version(68, 1, 2)))
elif sys.version_info >= (3, 9):
    AutoInstall.register(Package('setuptools', Version(59, 8, 0)))
elif sys.version_info >= (3, 0):
    AutoInstall.register(Package('setuptools', Version(56, 0, 0)))
else:
    AutoInstall.register(Package('setuptools', Version(44, 1, 1)))

if sys.version_info >= (3, 6):
    AutoInstall.register(Package('certifi', Version(2022, 12, 7)))
else:
    AutoInstall.register(Package('certifi', Version(2021, 10, 8)))

AutoInstall.register(Package('chardet', Version(3, 0, 4)))
AutoInstall.register(Package('dateutil', Version(2, 8, 1), pypi_name='python-dateutil', wheel=True))
AutoInstall.register(Package('entrypoints', Version(0, 3, 0)))
AutoInstall.register(Package('funcsigs', Version(1, 0, 2)))
AutoInstall.register(Package('idna', Version(2, 10)))

if sys.version_info > (3, 0):
    AutoInstall.register(Package('packaging', Version(21, 3), implicit_deps=['pyparsing']))
else:
    AutoInstall.register(Package('packaging', Version(20, 4), implicit_deps=['pyparsing', 'six']))

AutoInstall.register(Package('pyparsing', Version(2, 4, 7)))

AutoInstall.register(Package('requests', Version(2, 26, 0)))

if sys.version_info >= (3, 0):
    AutoInstall.register(Package('tomli', Version(2, 0, 1), wheel=True))
    AutoInstall.register(Package('setuptools_scm', Version(6, 4, 2), pypi_name='setuptools-scm', implicit_deps=['tomli']))
else:
    AutoInstall.register(Package('setuptools_scm', Version(5, 0, 2), pypi_name='setuptools-scm'))
AutoInstall.register(Package('socks', Version(1, 7, 1), pypi_name='PySocks'))
AutoInstall.register(Package('six', Version(1, 16, 0)))
AutoInstall.register(Package('tblib', Version(1, 7, 0)))

AutoInstall.register(Package('urllib3', Version(1, 26, 17)))

AutoInstall.register(Package('wheel', Version(0, 35, 1)))
AutoInstall.register(Package('cffi', Version(1, 15, 1)))

if sys.version_info > (3, 0):
    if sys.version_info >= (3, 9):
        AutoInstall.register(Package('OpenSSL', Version(23, 2, 0), pypi_name='pyOpenSSL'))
    else:
        AutoInstall.register(Package('OpenSSL', Version(20, 0, 0), pypi_name='pyOpenSSL'))

    # There are no prebuilt binaries for arm-32 of 'cryptography' and building it requires cargo/rust
    # Since this dep is not really needed for the current arm-32 bots we skip it instead of
    # adding the overhead of a cargo/rust toolchain into the yocto-based image the bots run.
    if not (platform.machine().startswith('arm') and platform.architecture()[0] == '32bit'):
        if sys.version_info >= (3, 11):
            AutoInstall.register(Package('cryptography', Version(40, 0, 2), wheel=True, implicit_deps=['cffi', 'OpenSSL']))
        elif sys.version_info >= (3, 9):
            AutoInstall.register(Package('cryptography', Version(38, 0, 2), wheel=True, implicit_deps=['cffi', 'OpenSSL']))
        else:
            AutoInstall.register(Package('cryptography', Version(36, 0, 2), wheel=True, implicit_deps=['cffi', 'OpenSSL']))

else:
    AutoInstall.register(Package('OpenSSL', Version(17, 2, 0), pypi_name='pyOpenSSL'))

if sys.version_info >= (3, 6):
    if sys.platform == 'linux':
        AutoInstall.register(Package('jeepney', Version(0, 7, 1)))
        AutoInstall.register(Package('secretstorage', Version(3, 3, 1)))
    AutoInstall.register(Package('keyring', Version(23, 2, 1)))
else:
    AutoInstall.register(Package('keyring', Version(18, 0, 1)))

if sys.version_info < (3, 0):
    AutoInstall.register(Package('inspect2', Version(0, 1, 2)))

name = 'webkitcorepy'
