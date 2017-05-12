# Copyright (C) 2017 Apple Inc. All rights reserved.
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

from webkitpy.port.ios_device import IOSDevicePort
from webkitpy.port import ios_testcase


class IOSDeviceTest(ios_testcase.IOSTest):
    os_name = 'ios-device'
    os_version = ''
    port_name = 'ios-device'
    port_maker = IOSDevicePort

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=None, **kwargs):
        port = super(IOSDeviceTest, self).make_port(host=host, port_name=port_name, options=options, os_name=os_name, s_version=os_version, kwargs=kwargs)
        return port

    def test_operating_system(self):
        self.assertEqual('ios-device', self.make_port().operating_system())

    # FIXME: Update tests when <rdar://problem/30497991> is completed.
    def test_crashlog_path(self):
        pass

    def test_spindump(self):
        pass

    def test_sample_process(self):
        pass

    def test_sample_process_exception(self):
        pass
