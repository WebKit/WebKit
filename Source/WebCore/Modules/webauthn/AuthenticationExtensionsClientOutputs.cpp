/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AuthenticationExtensionsClientOutputs.h"

#if ENABLE(WEB_AUTHN)

#include "CBORReader.h"
#include "CBORWriter.h"

namespace WebCore {

std::optional<AuthenticationExtensionsClientOutputs> AuthenticationExtensionsClientOutputs::fromCBOR(const Vector<uint8_t>& buffer)
{
    std::optional<cbor::CBORValue> decodedValue = cbor::CBORReader::read(buffer);
    if (!decodedValue || !decodedValue->isMap())
        return std::nullopt;
    AuthenticationExtensionsClientOutputs clientOutputs;

    const auto& decodedMap = decodedValue->getMap();
    auto it = decodedMap.find(cbor::CBORValue("appid"));
    if (it != decodedMap.end() && it->second.isBool())
        clientOutputs.appid = it->second.getBool();
    it = decodedMap.find(cbor::CBORValue("credProps"));
    if (it != decodedMap.end() && it->second.isMap()) {
        CredentialPropertiesOutput credProps;
        it = it->second.getMap().find(cbor::CBORValue("rk"));
        if (it != decodedMap.end() && it->second.isBool())
            credProps.rk = it->second.getBool();
        clientOutputs.credProps = credProps;
    }

    it = decodedMap.find(cbor::CBORValue("largeBlob"));
    if (it != decodedMap.end() && it->second.isMap()) {
        const auto& largeBlobMap = it->second.getMap();
        LargeBlobOutputs largeBlob;

        auto largeBlobIt = largeBlobMap.find(cbor::CBORValue("supported"));
        if (largeBlobIt != largeBlobMap.end() && largeBlobIt->second.isBool())
            largeBlob.supported = largeBlobIt->second.getBool();

        largeBlobIt = largeBlobMap.find(cbor::CBORValue("blob"));
        if (largeBlobIt != largeBlobMap.end() && largeBlobIt->second.isByteString()) {
            RefPtr<ArrayBuffer> blob = ArrayBuffer::create(largeBlobIt->second.getByteString());
            largeBlob.blob = WTFMove(blob);
        }

        largeBlobIt = largeBlobMap.find(cbor::CBORValue("written"));
        if (largeBlobIt != largeBlobMap.end() && largeBlobIt->second.isBool())
            largeBlob.written = largeBlobIt->second.getBool();

        clientOutputs.largeBlob = largeBlob;
    }

    return clientOutputs;
}

Vector<uint8_t> AuthenticationExtensionsClientOutputs::toCBOR() const
{
    cbor::CBORValue::MapValue clientOutputsMap;
    if (appid)
        clientOutputsMap[cbor::CBORValue("appid")] = cbor::CBORValue(*appid);
    if (credProps) {
        cbor::CBORValue::MapValue credPropsMap;
        credPropsMap[cbor::CBORValue("rk")] = cbor::CBORValue(credProps->rk);
        clientOutputsMap[cbor::CBORValue("credProps")] = cbor::CBORValue(credPropsMap);
    }

    if (largeBlob) {
        cbor::CBORValue::MapValue largeBlobMap;
        if (largeBlob->supported)
            largeBlobMap[cbor::CBORValue("supported")] = cbor::CBORValue(largeBlob->supported.value());

        if (largeBlob->blob)
            largeBlobMap[cbor::CBORValue("blob")] = cbor::CBORValue(largeBlob->blob->toVector());

        if (largeBlob->written)
            largeBlobMap[cbor::CBORValue("written")] = cbor::CBORValue(largeBlob->written.value());

        clientOutputsMap[cbor::CBORValue("largeBlob")] = cbor::CBORValue(largeBlobMap);
    }

    auto clientOutputs = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(clientOutputsMap)));
    ASSERT(clientOutputs);

    return *clientOutputs;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
