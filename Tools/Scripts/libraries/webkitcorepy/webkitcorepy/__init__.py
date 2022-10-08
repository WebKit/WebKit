# Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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

import logging
import platform
import sys

from logging import NullHandler

log = logging.getLogger('webkitcorepy')
log.addHandler(NullHandler())

from webkitcorepy.version import Version
from webkitcorepy.string_utils import BytesIO, StringIO, UnicodeIO, unicode
from webkitcorepy.timeout import Timeout
from webkitcorepy.subprocess_utils import TimeoutExpired, CompletedProcess, run
from webkitcorepy.output_capture import LoggerCapture, OutputCapture, OutputDuplicate
from webkitcorepy.task_pool import TaskPool
from webkitcorepy.terminal import Terminal
from webkitcorepy.environment import Environment
from webkitcorepy.credentials import credentials, delete_credentials
from webkitcorepy.measure_time import MeasureTime
from webkitcorepy.nested_fuzzy_dict import NestedFuzzyDict
from webkitcorepy.call_by_need import CallByNeed
from webkitcorepy.editor import Editor
from webkitcorepy.file_lock import FileLock

version = Version(0, 13, 16)

from webkitcorepy.autoinstall import Package, AutoInstall
if sys.version_info > (3, 0):
    AutoInstall.register(Package('mock', Version(4)))
    AutoInstall.register(Package('setuptools', Version(56, 0, 0)))
else:
    AutoInstall.register(Package('mock', Version(3, 0, 5)))
    AutoInstall.register(Package('setuptools', Version(44, 1, 1)))
    if platform.system() == 'Windows':
        AutoInstall.register(Package('win_inet_pton', Version(1, 1, 0), pypi_name='win-inet-pton'))

AutoInstall.register(Package('certifi', Version(2020, 6, 20)))
AutoInstall.register(Package('chardet', Version(3, 0, 4)))
AutoInstall.register(Package('dateutil', Version(2, 8, 1), pypi_name='python-dateutil'))
AutoInstall.register(Package('entrypoints', Version(0, 3, 0)))
AutoInstall.register(Package('funcsigs', Version(1, 0, 2)))
AutoInstall.register(Package('idna', Version(2, 10)))

if sys.version_info > (3, 0):
    AutoInstall.register(Package('packaging', Version(21, 3)))
else:
    AutoInstall.register(Package('packaging', Version(20, 4)))

AutoInstall.register(Package('pyparsing', Version(2, 4, 7)))
AutoInstall.register(Package('requests', Version(2, 24)))
AutoInstall.register(Package('setuptools_scm', Version(5, 0, 2), pypi_name='setuptools-scm'))
AutoInstall.register(Package('socks', Version(1, 7, 1), pypi_name='PySocks'))
AutoInstall.register(Package('six', Version(1, 15, 0)))
AutoInstall.register(Package('tblib', Version(1, 7, 0)))
AutoInstall.register(Package('urllib3', Version(1, 25, 10)))
AutoInstall.register(Package('wheel', Version(0, 35, 1)))
AutoInstall.register(Package('whichcraft', Version(0, 6, 1)))

if sys.version_info >= (3, 6):
    if sys.platform == 'linux':
        AutoInstall.register(Package('jeepney', Version(0, 7, 1)))
        AutoInstall.register(Package('cffi', Version(1, 15, 0)))
        AutoInstall.register(Package('cryptography', Version(36, 0, 1), wheel=True, implicit_deps=['cffi']))
        AutoInstall.register(Package('secretstorage', Version(3, 3, 1)))
    AutoInstall.register(Package('keyring', Version(23, 2, 1)))
else:
    AutoInstall.register(Package('keyring', Version(18, 0, 1)))

name = 'webkitcorepy'
