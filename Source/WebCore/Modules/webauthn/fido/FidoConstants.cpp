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

#include "config.h"
#include "FidoConstants.h"

#if ENABLE(WEB_AUTHN)

namespace fido {
using namespace WebCore;

bool isCtapDeviceResponseCode(CtapDeviceResponseCode code)
{
    switch (code) {
    case CtapDeviceResponseCode::kSuccess:
    case CtapDeviceResponseCode::kCtap1ErrInvalidCommand:
    case CtapDeviceResponseCode::kCtap1ErrInvalidParameter:
    case CtapDeviceResponseCode::kCtap1ErrInvalidLength:
    case CtapDeviceResponseCode::kCtap1ErrInvalidSeq:
    case CtapDeviceResponseCode::kCtap1ErrTimeout:
    case CtapDeviceResponseCode::kCtap1ErrChannelBusy:
    case CtapDeviceResponseCode::kCtap1ErrLockRequired:
    case CtapDeviceResponseCode::kCtap1ErrInvalidChannel:
    case CtapDeviceResponseCode::kCtap2ErrCBORParsing:
    case CtapDeviceResponseCode::kCtap2ErrUnexpectedType:
    case CtapDeviceResponseCode::kCtap2ErrInvalidCBOR:
    case CtapDeviceResponseCode::kCtap2ErrInvalidCBORType:
    case CtapDeviceResponseCode::kCtap2ErrMissingParameter:
    case CtapDeviceResponseCode::kCtap2ErrLimitExceeded:
    case CtapDeviceResponseCode::kCtap2ErrUnsupportedExtension:
    case CtapDeviceResponseCode::kCtap2ErrTooManyElements:
    case CtapDeviceResponseCode::kCtap2ErrExtensionNotSupported:
    case CtapDeviceResponseCode::kCtap2ErrCredentialExcluded:
    case CtapDeviceResponseCode::kCtap2ErrProcesssing:
    case CtapDeviceResponseCode::kCtap2ErrInvalidCredential:
    case CtapDeviceResponseCode::kCtap2ErrUserActionPending:
    case CtapDeviceResponseCode::kCtap2ErrOperationPending:
    case CtapDeviceResponseCode::kCtap2ErrNoOperations:
    case CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithms:
    case CtapDeviceResponseCode::kCtap2ErrOperationDenied:
    case CtapDeviceResponseCode::kCtap2ErrKeyStoreFull:
    case CtapDeviceResponseCode::kCtap2ErrNotBusy:
    case CtapDeviceResponseCode::kCtap2ErrNoOperationPending:
    case CtapDeviceResponseCode::kCtap2ErrUnsupportedOption:
    case CtapDeviceResponseCode::kCtap2ErrInvalidOption:
    case CtapDeviceResponseCode::kCtap2ErrKeepAliveCancel:
    case CtapDeviceResponseCode::kCtap2ErrNoCredentials:
    case CtapDeviceResponseCode::kCtap2ErrUserActionTimeout:
    case CtapDeviceResponseCode::kCtap2ErrNotAllowed:
    case CtapDeviceResponseCode::kCtap2ErrPinInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
    case CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid:
    case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
    case CtapDeviceResponseCode::kCtap2ErrPinNotSet:
    case CtapDeviceResponseCode::kCtap2ErrPinRequired:
    case CtapDeviceResponseCode::kCtap2ErrPinPolicyViolation:
    case CtapDeviceResponseCode::kCtap2ErrPinTokenExpired:
    case CtapDeviceResponseCode::kCtap2ErrRequestTooLarge:
    case CtapDeviceResponseCode::kCtap2ErrOther:
    case CtapDeviceResponseCode::kCtap2ErrSpecLast:
    case CtapDeviceResponseCode::kCtap2ErrExtensionFirst:
    case CtapDeviceResponseCode::kCtap2ErrExtensionLast:
    case CtapDeviceResponseCode::kCtap2ErrVendorFirst:
    case CtapDeviceResponseCode::kCtap2ErrVendorLast:
        return true;
    default:
        return false;
    }
}

bool isFidoHidDeviceCommand(FidoHidDeviceCommand cmd)
{
    switch (cmd) {
    case FidoHidDeviceCommand::kMsg:
    case FidoHidDeviceCommand::kCbor:
    case FidoHidDeviceCommand::kInit:
    case FidoHidDeviceCommand::kPing:
    case FidoHidDeviceCommand::kCancel:
    case FidoHidDeviceCommand::kError:
    case FidoHidDeviceCommand::kKeepAlive:
    case FidoHidDeviceCommand::kWink:
    case FidoHidDeviceCommand::kLock:
        return true;
    default:
        return false;
    }
}

const char* publicKeyCredentialTypeToString(PublicKeyCredentialType type)
{
    switch (type) {
    case PublicKeyCredentialType::PublicKey:
        return kPublicKey;
    }
    ASSERT_NOT_REACHED();
    return kPublicKey;
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
