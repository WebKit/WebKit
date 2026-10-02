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
    """What a port may ask of its DEVICE_MANAGER about devices becoming ready.

    A manager which hands devices to workers as each one finishes coming up overrides these. One which returns devices
    already able to run tests inherits them unchanged, and the port then addresses its devices by position instead of
    claiming them."""

    DEVICE_QUEUE = None
    READY_DEVICES = []
    PENDING_DEVICES = []

    @classmethod
    def begin_provisioning(cls, timeout=None, slots_per_device=1):
        """Starts waiting on every initialized device, so each can be handed over as it becomes ready."""
        return None

    @classmethod
    def advance_provisioning(cls):
        """Hands over any device that has finished coming up. Never blocks."""
        return None

    @classmethod
    def wait_for_first_ready_device(cls, timeout=None):
        """Blocks until one device can be handed to a worker, and returns whether any can."""
        return bool(cls.READY_DEVICES)

    @classmethod
    def offer_ready_devices(cls, slots_per_device=None):
        """Offers every ready device to a pool of workers which has not claimed them yet."""
        return None

    @classmethod
    def claim_device(cls, wait_timeout):
        """A device for this worker, and whether this worker is the one to set it up."""
        return None, False

    @classmethod
    def block_on_ready(cls, devices=None, timeout=None):
        """Waits until devices can run tests, which happens later than them coming up."""
        return None

    @classmethod
    def expects_more_devices(cls):
        """Whether a device is still on its way."""
        return False

    @classmethod
    def has_ready_device(cls):
        return bool(cls.READY_DEVICES)

    @classmethod
    def end_provisioning(cls):
        """Forgets a run's devices. A device torn down by one run is not ready for the next."""
        return None

    @classmethod
    def provisioning_state(cls):
        """What a worker needs in order to claim a device."""
        return None

    @classmethod
    def adopt_provisioning_state(cls, state):
        return None
