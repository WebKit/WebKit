# Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import logging

from webkitpy.port.ios import IOSPort

_log = logging.getLogger(__name__)


class IOSDevicePort(IOSPort):
    port_name = 'ios-device'

    ARCHITECTURES = ['armv7', 'armv7s', 'arm64']
    DEFAULT_ARCHITECTURE = 'arm64'
    VERSION_FALLBACK_ORDER = ['ios-7', 'ios-8', 'ios-9', 'ios-10']

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name == cls.port_name:
            iphoneos_sdk_version = host.platform.xcode_sdk_version('iphoneos')
            if not iphoneos_sdk_version:
                raise Exception("Please install the iOS SDK.")
            major_version_number = iphoneos_sdk_version.split('.')[0]
            port_name = port_name + '-' + major_version_number
        return port_name

    # Despite their names, these flags do not actually get passed all the way down to webkit-build.
    def _build_driver_flags(self):
        return ['--sdk', 'iphoneos'] + (['ARCHS=%s' % self.architecture()] if self.architecture() else [])

    def operating_system(self):
        return 'ios-device'
