# Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

from webkitpy.port.config import apple_additions
from webkitpy.port.embedded_simulator import EmbeddedSimulatorPort
from webkitpy.port.watch import WatchPort
from webkitpy.xcode.device_type import DeviceType

_log = logging.getLogger(__name__)


class WatchSimulatorPort(EmbeddedSimulatorPort, WatchPort):
    port_name = 'watchos-simulator'

    ARCHITECTURES = ['i386', 'arm64_32']
    DEFAULT_ARCHITECTURE = 'i386'

    DEFAULT_DEVICE_TYPES = [DeviceType(hardware_family='Apple Watch', hardware_type='Series 9 - 45mm')]
    SDK = apple_additions().get_sdk('watchsimulator') if apple_additions() else 'watchsimulator'

    def architecture(self):
        result = self.get_option('architecture') or self.host.platform.architecture()
        if result in ('arm64', 'arm64e'):
            return 'arm64_32'
        return self.DEFAULT_ARCHITECTURE

    def operating_system(self):
        return 'watchos-simulator'
