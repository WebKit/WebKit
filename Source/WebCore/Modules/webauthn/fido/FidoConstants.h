// Copyright 2018 The Chromium Authors. All rights reserved.
// Copyright (C) 2018 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#if ENABLE(WEB_AUTHN)

namespace fido {

// Commands supported by CTAPHID device as specified in
// https://fidoalliance.org/specs/fido-v2.0-ps-20170927/fido-client-to-authenticator-protocol-v2.0-ps-20170927.html#ctaphid-commands
enum class FidoHidDeviceCommand : uint8_t {
    kMsg = 0x03,
    kCbor = 0x10,
    kInit = 0x06,
    kPing = 0x01,
    kCancel = 0x11,
    kError = 0x3F,
    kKeepAlive = 0x3B,
    kWink = 0x08,
    kLock = 0x04,
};

bool isFidoHidDeviceCommand(FidoHidDeviceCommand cmd);

// HID transport specific constants.
const size_t kHidPacketSize = 64;
const uint32_t kHidBroadcastChannel = 0xffffffff;
const size_t kHidInitPacketHeaderSize = 7;
const size_t kHidContinuationPacketHeader = 5;
const size_t kHidMaxPacketSize = 64;
const size_t kHidInitPacketDataSize = kHidMaxPacketSize - kHidInitPacketHeaderSize;
const size_t kHidContinuationPacketDataSize = kHidMaxPacketSize - kHidContinuationPacketHeader;

const uint8_t kHidMaxLockSeconds = 10;

// Messages are limited to an initiation packet and 128 continuation packets.
const size_t kHidMaxMessageSize = 7609;

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
