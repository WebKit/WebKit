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
#include "AuthenticatorSupportedOptions.h"

#if ENABLE(WEB_AUTHN)

#include "FidoConstants.h"

namespace fido {

AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::setSupportsResidentKey(bool supportsResidentKey)
{
    m_supportsResidentKey = supportsResidentKey;
    return *this;
}

AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::setUserVerificationAvailability(UserVerificationAvailability userVerificationAvailability)
{
    m_userVerificationAvailability = userVerificationAvailability;
    return *this;
}

AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::setUserPresenceRequired(bool userPresenceRequired)
{
    m_userPresenceRequired = userPresenceRequired;
    return *this;
}

AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::setClientPinAvailability(ClientPinAvailability clientPinAvailability)
{
    m_clientPinAvailability = clientPinAvailability;
    return *this;
}

AuthenticatorSupportedOptions& AuthenticatorSupportedOptions::setIsPlatformDevice(bool isPlatformDevice)
{
    m_isPlatformDevice = isPlatformDevice;
    return *this;
}

cbor::CBORValue convertToCBOR(const AuthenticatorSupportedOptions& options)
{
    using namespace cbor;

    CBORValue::MapValue optionMap;
    optionMap.emplace(CBORValue(kResidentKeyMapKey), CBORValue(options.supportsResidentKey()));
    optionMap.emplace(CBORValue(kUserPresenceMapKey), CBORValue(options.userPresenceRequired()));
    optionMap.emplace(CBORValue(kPlatformDeviceMapKey), CBORValue(options.isPlatformDevice()));

    using UvAvailability = AuthenticatorSupportedOptions::UserVerificationAvailability;

    switch (options.userVerificationAvailability()) {
    case UvAvailability::kSupportedAndConfigured:
        optionMap.emplace(CBORValue(kUserVerificationMapKey), CBORValue(true));
        break;
    case UvAvailability::kSupportedButNotConfigured:
        optionMap.emplace(CBORValue(kUserVerificationMapKey), CBORValue(false));
        break;
    case UvAvailability::kNotSupported:
        break;
    }

    using ClientPinAvailability = AuthenticatorSupportedOptions::ClientPinAvailability;

    switch (options.clientPinAvailability()) {
    case ClientPinAvailability::kSupportedAndPinSet:
        optionMap.emplace(CBORValue(kClientPinMapKey), CBORValue(true));
        break;
    case ClientPinAvailability::kSupportedButPinNotSet:
        optionMap.emplace(CBORValue(kClientPinMapKey), CBORValue(false));
        break;
    case ClientPinAvailability::kNotSupported:
        break;
    }

    return CBORValue(WTFMove(optionMap));
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
