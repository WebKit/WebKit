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

import sys
import webkitscmpy

from webkitcorepy import AutoInstall, Package, Version
from webkitpy.autoinstalled import twisted

AutoInstall.install(Package('attrs', Version(21, 4, 0)))
AutoInstall.install(Package('constantly', Version(15, 1, 0)))
AutoInstall.install(Package('dateutil', Version(2, 8, 1), pypi_name='python-dateutil'))
AutoInstall.install(Package('future', Version(0, 18, 2)))
AutoInstall.install(Package('jinja2', Version(2, 11, 3), pypi_name='Jinja2'))
AutoInstall.install(Package('pbr', Version(5, 9, 0)))
AutoInstall.install(Package('lz4', Version(4, 3, 2)))
AutoInstall.install(Package('PyJWT', Version(1, 7, 1), pypi_name='PyJWT', aliases=['jwt']))
AutoInstall.install(Package('pyyaml', Version(5, 3, 1), pypi_name='PyYAML'))

AutoInstall.install('markupsafe')

if sys.version_info >= (3, 0):
    AutoInstall.install(Package('autobahn', Version(20, 7, 1)))
    AutoInstall.install(Package('automat', Version(20, 2, 0), pypi_name='Automat'))
    AutoInstall.install(Package('decorator', Version(5, 1, 1)))
    AutoInstall.install(Package('PyHamcrest', Version(2, 0, 3)))
    AutoInstall.install(Package('sqlalchemy', Version(1, 3, 20), pypi_name='SQLAlchemy'))
    AutoInstall.install(Package('sqlalchemy-migrate', Version(0, 13, 0)))
    AutoInstall.install(Package('sqlparse', Version(0, 4, 2)))
    AutoInstall.install(Package('txaio', Version(20, 4, 1)))
    AutoInstall.install(Package('Tempita', Version(0, 5, 2)))

    AutoInstall.install(Package('buildbot', Version(2, 10, 5)))
    AutoInstall.install(Package('buildbot-worker', Version(2, 10, 5)))

    from buildbot import statistics
else:
    AutoInstall.install(Package('autobahn', Version(17, 8, 1)))
    AutoInstall.install(Package('automat', Version(0, 6, 0), pypi_name='Automat'))
    AutoInstall.install(Package('decorator', Version(3, 4, 0)))
    AutoInstall.install(Package('sqlalchemy-migrate', Version(0, 11, 0)))
    AutoInstall.install(Package('sqlparse', Version(0, 2, 3)))
    AutoInstall.install(Package('txaio', Version(2, 8, 1)))
    AutoInstall.install(Package('Tempita', Version(0, 4, 0)))

    AutoInstall.install(Package('buildbot', Version(1, 1, 1)))
    AutoInstall.install(Package('buildbot-worker', Version(1, 1, 1)))

sys.modules[__name__] = __import__('buildbot')
