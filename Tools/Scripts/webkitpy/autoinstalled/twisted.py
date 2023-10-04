# Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

import platform
import sys

from webkitscmpy import AutoInstall, Package, Version

AutoInstall.install(Package('constantly', Version(15, 1, 0), pypi_name='constantly'))

if sys.version_info >= (3, 0):
    AutoInstall.install(Package('hyperlink', Version(21, 0, 0), pypi_name='hyperlink'))
    AutoInstall.install(Package('incremental', Version(21, 3, 0), pypi_name='incremental'))

    if sys.version_info >= (3, 11):
        AutoInstall.install(Package('twisted', Version(23, 8, 0), pypi_name='Twisted', implicit_deps=['pyparsing']))
        AutoInstall.install(Package('pyOpenSSL', Version(23, 2, 0)))
    else:
        AutoInstall.install(Package('twisted', Version(20, 3, 0), pypi_name='Twisted', implicit_deps=['pyparsing']))
        AutoInstall.install(Package('pyOpenSSL', Version(20, 0, 0)))

    # There are no prebuilt binaries for arm-32 of 'bcrypt' and building it requires cargo/rust
    # Since this dep is not really needed for the current arm-32 bots we skip it instead of
    # adding the overhead of a cargo/rust toolchain into the yocto-based image the bots run.
    if not (platform.machine().startswith('arm') and platform.architecture()[0] == '32bit'):
        AutoInstall.install(Package('bcrypt', Version(4), wheel=True))
    AutoInstall.install(Package('pycparser', Version(2, 21), wheel=True))

    from twisted.protocols.tls import TLSMemoryBIOFactory
    from twisted.python import threadpool
else:
    AutoInstall.install(Package('hyperlink', Version(17, 3, 0), pypi_name='hyperlink'))
    AutoInstall.install(Package('incremental', Version(17, 5, 0), pypi_name='incremental'))
    AutoInstall.install(Package('twisted', Version(17, 5, 0), pypi_name='Twisted', implicit_deps=['pyparsing']))

    AutoInstall.install(Package('pyOpenSSL', Version(17, 2, 0)))
    AutoInstall.install(Package('pycparser', Version(2, 18)))

sys.modules[__name__] = __import__('twisted')
