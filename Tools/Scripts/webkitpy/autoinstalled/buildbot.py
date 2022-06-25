# Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

from webkitscmpy import AutoInstall, Package, Version

AutoInstall.register(Package('autobahn', Version(20, 7, 1)))
AutoInstall.register(Package('buildbot', Version(2, 10, 1)))
AutoInstall.register(Package('dateutil', Version(2, 8, 1), pypi_name='python-dateutil'))
AutoInstall.register(Package('jinja2', Version(2, 11, 2), pypi_name='Jinja2'))
AutoInstall.register(Package('jwt', Version(1, 7, 1), pypi_name='PyJWT'))
AutoInstall.register(Package('pyyaml', Version(5, 3, 1), pypi_name='PyYAML'))
AutoInstall.register(Package('sqlalchemy', Version(1, 3, 20), pypi_name='SQLAlchemy'))
AutoInstall.register(Package('sqlalchemy-migrate', Version(0, 13, 0)))
AutoInstall.register(Package('twisted', Version(20, 3, 0), pypi_name='Twisted'))
AutoInstall.register(Package('txaio', Version(20, 4, 1)))

sys.modules[__name__] = __import__('buildbot')
