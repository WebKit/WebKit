# Copyright (C) 2020 Apple Inc. All rights reserved.
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
import requests

from webkitscmpy import Package, Version


class ChromeDriverPackage(Package):
    URL = 'https://chromedriver.storage.googleapis.com/'

    def __init__(self, version=None):
        super(ChromeDriverPackage, self).__init__('chromedriver', version=version)

    def archives(self):
        if self.version is not None:
            try:
                response = requests.get(self.URL + 'LATEST_RELEASE')
            except requests.RequestException:
                return []

            if response.status_code != 200:
                return []
            version = Version.from_string(response.text.rstrip())
        else:
            version = self.version

        os_name = platform.system()
        if os_name == 'Linux':
            os = 'linux{}'.format('64' if platform.machine()[-2:] == '64' else '')
        elif os_name == 'Darwin':
            os = 'mac64'
        else:
            return []

        return [Package.Archive(
            self.name,
            link='{}{}/chromedriver_{}.zip'.format(self.URL, version, os),
            version=version,
            extension='zip',
        )]


package = ChromeDriverPackage(version=Version(86, 0, 4240, 22))
package.install()
executable = package.location
