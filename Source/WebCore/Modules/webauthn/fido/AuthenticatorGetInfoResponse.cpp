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

static String toString(WebCore::AuthenticatorTransport transport)
{
    switch (transport) {
    case WebCore::AuthenticatorTransport::Usb:
        return WebCore::authenticatorTransportUsb;
        break;
    case WebCore::AuthenticatorTransport::Nfc:
        return WebCore::authenticatorTransportNfc;
        break;
    case WebCore::AuthenticatorTransport::Ble:
        return WebCore::authenticatorTransportBle;
        break;
    case WebCore::AuthenticatorTransport::Internal:
        return WebCore::authenticatorTransportInternal;
        break;
    case WebCore::AuthenticatorTransport::Cable:
        return WebCore::authenticatorTransportCable;
    case WebCore::AuthenticatorTransport::Hybrid:
        return WebCore::authenticatorTransportHybrid;
    case WebCore::AuthenticatorTransport::SmartCard:
        return WebCore::authenticatorTransportSmartCard;
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return nullString();
}

Vector<uint8_t> encodeAsCBOR(const AuthenticatorGetInfoResponse& response)
{
    using namespace cbor;

    CBORValue::MapValue deviceInfoMap;

    CBORValue::ArrayValue versionArray;
    for (const auto& version : response.versions())
        versionArray.append(version == ProtocolVersion::kCtap ? kCtap2Version : kU2fVersion);
    deviceInfoMap.emplace(CBORValue(1), CBORValue(WTFMove(versionArray)));

    if (response.extensions())
        deviceInfoMap.emplace(CBORValue(2), toArrayValue(*response.extensions()));

    deviceInfoMap.emplace(CBORValue(3), CBORValue(response.aaguid()));
    deviceInfoMap.emplace(CBORValue(4), convertToCBOR(response.options()));

    if (response.maxMsgSize())
        deviceInfoMap.emplace(CBORValue(5), CBORValue(static_cast<int64_t>(*response.maxMsgSize())));

    if (response.pinProtocol())
        deviceInfoMap.emplace(CBORValue(6), toArrayValue(*response.pinProtocol()));
    
    if (response.transports()) {
        auto transports = *response.transports();
        deviceInfoMap.emplace(CBORValue(7), toArrayValue(transports.map(toString)));
    }

    auto encodedBytes = CBORWriter::write(CBORValue(WTFMove(deviceInfoMap)));
    ASSERT(encodedBytes);
    return *encodedBytes;
}

} // namespace fido

#endif // ENABLE(WEB_AUTHN)
