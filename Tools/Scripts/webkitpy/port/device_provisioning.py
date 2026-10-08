# Copyright (C) 2026 Apple Inc. All rights reserved.
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


class DeviceProvisioning(object):
    """A DEVICE_MANAGER which hands devices to workers as each finishes booting.

    The defaults describe a manager whose devices are ready when created, which the port addresses by position."""

    DEVICE_QUEUE = None
    READY_DEVICES = []

    @classmethod
    def create_devices(cls, requests, host=None, **kwargs):
        raise NotImplementedError

    @classmethod
    def begin_provisioning(cls, timeout, slots_per_device=1):
        raise NotImplementedError

    @classmethod
    def wait_for_first_ready_device(cls):
        raise NotImplementedError

    @classmethod
    def claim_device(cls, wait_timeout):
        raise NotImplementedError

    @classmethod
    def block_on_ready(cls, devices=None, timeout=None):
        raise NotImplementedError

    @classmethod
    def offer_ready_devices(cls):
        pass

    @classmethod
    def expects_more_devices(cls):
        return False

    @classmethod
    def is_device_present(cls, device, force_update=False):
        return True

    @classmethod
    def end_provisioning(cls):
        pass

    @classmethod
    def provisioning_state(cls):
        return None

    @classmethod
    def adopt_provisioning_state(cls, state):
        pass
