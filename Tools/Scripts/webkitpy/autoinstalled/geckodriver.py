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


class GeckoDriverPackage(Package):
    URL = 'https://api.github.com/repos/mozilla/geckodriver/releases'

    def __init__(self, version=None):
        super(GeckoDriverPackage, self).__init__('geckodriver', version=version, custom_install=True)

    def archives(self):
        try:
            response = requests.get(self.URL)
        except requests.RequestException:
            return []

        if response.status_code != 200:
            return []

        os_name = platform.system()
        if os_name == 'Darwin':
            os_tag = 'macos'
        else:
            os_tag = '{}{}'.format(os_name.lower(), platform.machine()[-2:])

        archives = []
        for candidate in reversed(response.json()):
            try:
                version = Version.from_string(candidate.get('tag_name').split('v')[-1])
            except ValueError:
                continue

            for asset in candidate.get('assets', []):
                url = asset.get('browser_download_url')
                if not url:
                    continue
                if os_tag not in url:
                    continue

                extension = 'zip' if url.endswith('.zip') else 'tar.gz'
                if not url.endswith(extension):
                    continue

                if self.version and version not in self.version:
                    continue

                archives.append(
                    Package.Archive(
                        self.name,
                        link=url,
                        version=version,
                        extension=extension,
                    )
                )

        return archives

    def do_custom_install(self, unpack_location):
        # The geckodriver binary is directly in the archive root
        source = os.path.join(unpack_location, self.name)
        shutil.copy(source, self.location)
        os.chmod(self.location, 0o755)


package = GeckoDriverPackage(version=Version(0, 36, 0))
package.install()
executable = package.location
