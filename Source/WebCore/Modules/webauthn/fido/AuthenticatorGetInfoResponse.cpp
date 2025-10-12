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
#include "AuthenticatorGetInfoResponse.h"

#if ENABLE(WEB_AUTHN)

#include "CBORValue.h"
#include "CBORWriter.h"
#include "WebAuthenticationConstants.h"
#include "WebAuthenticationUtils.h"

namespace fido {

template <typename Container>
cbor::CBORValue toArrayValue(const Container& container)
{
    auto value = WTF::map(container, [](auto& item) {
        return cbor::CBORValue(item);
    });
    return cbor::CBORValue(WTFMove(value));
}

AuthenticatorGetInfoResponse::AuthenticatorGetInfoResponse(StdSet<ProtocolVersion>&& versions, Vector<uint8_t>&& aaguid)
    : m_versions(WTFMove(versions))
    , m_aaguid(WTFMove(aaguid))
{
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setMaxMsgSize(uint32_t maxMsgSize)
{
    m_maxMsgSize = maxMsgSize;
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setPinProtocols(Vector<uint8_t>&& pinProtocols)
{
    m_pinProtocols = WTFMove(pinProtocols);
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setExtensions(Vector<String>&& extensions)
{
    m_extensions = WTFMove(extensions);
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setOptions(AuthenticatorSupportedOptions&& options)
{
    m_options = WTFMove(options);
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setTransports(Vector<WebCore::AuthenticatorTransport>&& transports)
{
    m_transports = WTFMove(transports);
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setRemainingDiscoverableCredentials(uint32_t remainingDiscoverableCredentials)
{
    m_remainingDiscoverableCredentials = remainingDiscoverableCredentials;
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setMinPINLength(uint32_t minPINLength)
{
    m_minPINLength = minPINLength;
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setMaxCredentialCountInList(uint32_t maxCredentialCountInList)
{
    m_maxCredentialCountInList = maxCredentialCountInList;
    return *this;
}

AuthenticatorGetInfoResponse& AuthenticatorGetInfoResponse::setMaxCredentialIDLength(uint32_t maxCredentialIDLength)
{
    m_maxCredentialIdLength = maxCredentialIDLength;
    return *this;
}

Vector<uint8_t> encodeAsCBOR(const AuthenticatorGetInfoResponse& response)
{
    using namespace cbor;

    CBORValue::MapValue deviceInfoMap;

    CBORValue::ArrayValue versionArray;
    for (const auto& version : response.versions())
        versionArray.append(toString(version));
    deviceInfoMap.emplace(CBORValue(1), CBORValue(WTFMove(versionArray)));
    deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoVersionsKey), CBORValue(WTFMove(versionArray)));

    if (response.extensions())
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoExtensionsKey), toArrayValue(*response.extensions()));

    deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoAAGUIDKey), CBORValue(response.aaguid()));
    deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoOptionsKey), convertToCBOR(response.options()));

    if (response.maxMsgSize())
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoMaxMsgSizeKey), CBORValue(static_cast<int64_t>(*response.maxMsgSize())));

    if (response.pinProtocol())
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoPinUVAuthProtocolsKey), toArrayValue(*response.pinProtocol()));
    
    if (response.transports()) {
        auto transports = *response.transports();
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoTransportsKey), toArrayValue(transports.map([](auto transport) {
            return WebCore::toString(transport);
        })));
    }

    if (response.remainingDiscoverableCredentials())
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoRemainingDiscoverableCredentialsKey), CBORValue(static_cast<int64_t>(*response.remainingDiscoverableCredentials())));

    if (auto minPINLength = response.minPINLength())
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoMinPINLengthKey), CBORValue(static_cast<int64_t>(*minPINLength)));

    if (response.maxCredentialCountInList())
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoMaxCredentialCountInListKey), CBORValue(static_cast<int64_t>(*response.maxCredentialCountInList())));
    if (response.maxCredentialIDLength())
        deviceInfoMap.emplace(CBORValue(kCtapAuthenticatorGetInfoMaxCredentialIdLengthKey), CBORValue(static_cast<int64_t>(*response.maxCredentialIDLength())));

    auto encodedBytes = CBORWriter::write(CBORValue(WTFMove(deviceInfoMap)));
    ASSERT(encodedBytes);
    return *encodedBytes;
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
