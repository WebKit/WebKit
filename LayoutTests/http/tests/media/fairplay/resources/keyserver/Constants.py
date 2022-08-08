#!/usr/bin/env python3

# Copyright (C) 2022 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from enum import Enum, IntEnum, IntFlag
from typing import Final


class SKDServerAESEncType(Enum):
    Encrypt = 0
    Decrypt = 1


class SKDServerAESEncMode(Enum):
    CBC = 0
    ECB = 1


class SKDHDCPInformation(IntEnum):
    NotRequired = 0xEF72894CA7895B78
    Type0 = 0x40791AC78BD5C571
    Type1 = 0x285A0863BBA8E1D3


class SKDClientRefTimePlayback(IntEnum):
    CurrentlyPlayingCKNotRequired = 0xa5d6739e
    FirstPlaybackCKRequired = 0xf4dee5a2
    CurrentlyPlayingCKRequired = 0x4f834330
    PlaybackSessionStopped = 0x5991bf20


class SKDLeaseRentalType(IntEnum):
    Lease = 0x1a4bde7e
    Duration = 0x3dfe45a0
    LeaseAndDuration = 0x27b59bde
    Persistence = 0x3df2d9fb
    PersistenceAndDuration = 0x18f06048


class SKDStreamingIndicatorValue(IntEnum):
    AirPlay = 0xabb0256a31843974
    AVAdapter = 0x5f9c8132b59f2fde


class SKDCapabilityFlags(IntFlag):
    HDCPEnforcement = 1 << 0
    OfflineKey = 1 << 1
    SecureInvalidation = 1 << 2
    OfflineKeyTLLVv2 = 1 << 3


class Constants:
    # Standard sizes
    PS_AES128_KEY_SZ: Final = 16
    PS_AES128_IV_SZ: Final = 16
