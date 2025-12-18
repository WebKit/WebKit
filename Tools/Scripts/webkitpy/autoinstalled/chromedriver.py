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

import os
import platform
import requests
import shutil

from webkitscmpy import Package, Version


class ChromeDriverPackage(Package):
    URL = 'https://storage.googleapis.com/chrome-for-testing-public/'
    VERSIONS_URL = 'https://googlechromelabs.github.io/chrome-for-testing/last-known-good-versions.json'

    def __init__(self, version=None):
        super(ChromeDriverPackage, self).__init__('chromedriver', version=version, custom_install=True)
        self._platform = None

    def archives(self):
        if self.version is None:
            try:
                response = requests.get(self.VERSIONS_URL)
            except requests.RequestException:
                return []

            if response.status_code != 200:
                return []
            version = Version.from_string(response.json()['channels']['Stable']['version'])
        else:
            version = self.version

        os_name = platform.system()
        machine = platform.machine()
        if os_name == 'Linux':
            self._platform = 'linux64'
        elif os_name == 'Darwin':
            self._platform = 'mac-arm64' if machine == 'arm64' else 'mac-x64'
        else:
            return []

        return [Package.Archive(
            self.name,
            link='{}{}/{}/chromedriver-{}.zip'.format(self.URL, version, self._platform, self._platform),
            version=version,
            extension='zip',
        )]

    def do_custom_install(self, unpack_location):
        # The chromedriver binary is inside a chromedriver-{platform} subdirectory
        source = os.path.join(unpack_location, 'chromedriver-{}'.format(self._platform), self.name)
        shutil.copy(source, self.location)
        os.chmod(self.location, 0o755)


package = ChromeDriverPackage(version=Version(143, 0, 7499, 42))
package.install()
executable = package.location
