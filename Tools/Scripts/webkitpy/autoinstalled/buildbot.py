# Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

import webkitscmpy
import rapidfuzz

AutoInstall.install('markupsafe')
AutoInstall.install('jinja2')

AutoInstall.install(Package('attrs', Version(21, 3), aliases=['attr']))
AutoInstall.install(Package('constantly', Version(23, 10, 4)))
AutoInstall.install(Package('dateutil', Version(2, 8, 1), pypi_name='python-dateutil', wheel=True))
AutoInstall.install(Package('future', Version(0, 18, 2), aliases=['libfuturize', 'libpasteurize', 'past']))
AutoInstall.install(Package('pbr', Version(5, 9, 0)))
AutoInstall.install(Package('lz4', Version(4, 4, 4)))
AutoInstall.install(Package('jwt', Version(2, 10, 1), pypi_name='PyJWT'))

AutoInstall.install(Package('autobahn', Version(24, 4, 2), wheel=False))
AutoInstall.install(Package('automat', Version(25, 4, 16), pypi_name='Automat'))
AutoInstall.install(Package('hamcrest', Version(2, 0, 3), pypi_name='PyHamcrest'))
AutoInstall.install(Package('mock', Version(5, 2, 0)))
AutoInstall.install(Package('pyyaml', Version(6, 0, 2)))
AutoInstall.install(Package('sqlalchemy', Version(2, 0, 41), pypi_name='SQLAlchemy'))
AutoInstall.install(Package('sqlalchemy_migrate', Version(0, 13, 0), pypi_name='sqlalchemy-migrate'))
AutoInstall.install(Package('txaio', Version(23, 1, 1)))
AutoInstall.install(Package('treq', Version(25, 5, 0)))
AutoInstall.install(Package('alembic', Version(1, 14, 0)))
AutoInstall.install(Package('Mako', Version(1, 3, 10)))
AutoInstall.install(Package('croniter', Version(6, 0, 0)))
AutoInstall.install(Package('pytz', Version(2025, 2)))

AutoInstall.install(Package('buildbot', Version(4, 3, 0)))
AutoInstall.install(Package('buildbot_worker', Version(4, 3, 0), pypi_name='buildbot-worker'))


sys.modules[__name__] = __import__('buildbot')
