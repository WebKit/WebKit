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
#include "AuthenticationExtensionsClientInputs.h"

#if ENABLE(WEB_AUTHN)

#include "CBORReader.h"
#include "CBORWriter.h"

namespace WebCore {

std::optional<AuthenticationExtensionsClientInputs> AuthenticationExtensionsClientInputs::fromCBOR(std::span<const uint8_t> buffer)
{
    std::optional<cbor::CBORValue> decodedValue = cbor::CBORReader::read(buffer);
    if (!decodedValue || !decodedValue->isMap())
        return std::nullopt;
    AuthenticationExtensionsClientInputs clientInputs;

    const auto& decodedMap = decodedValue->getMap();
    auto it = decodedMap.find(cbor::CBORValue("appid"));
    if (it != decodedMap.end() && it->second.isString())
        clientInputs.appid = it->second.getString();
    it = decodedMap.find(cbor::CBORValue("credProps"));
    if (it != decodedMap.end() && it->second.isBool())
        clientInputs.credProps = it->second.getBool();
    it = decodedMap.find(cbor::CBORValue("largeBlob"));
    if (it != decodedMap.end() && it->second.isMap()) {
        const auto& largeBlobMap = it->second.getMap();
        AuthenticationExtensionsClientInputs::LargeBlobInputs largeBlob;
        auto largeBlobIt = largeBlobMap.find(cbor::CBORValue("support"));

        if (largeBlobIt != largeBlobMap.end() && largeBlobIt->second.isString())
            largeBlob.support = largeBlobIt->second.getString();

        largeBlobIt = largeBlobMap.find(cbor::CBORValue("read"));
        if (largeBlobIt != largeBlobMap.end() && largeBlobIt->second.isBool())
            largeBlob.read = largeBlobIt->second.getBool();

        largeBlobIt = largeBlobMap.find(cbor::CBORValue("write"));
        if (largeBlobIt != largeBlobMap.end() && largeBlobIt->second.isByteString()) {
            RefPtr<ArrayBuffer> write = ArrayBuffer::create(largeBlobIt->second.getByteString());
            largeBlob.write = BufferSource(write);
        }

        clientInputs.largeBlob = largeBlob;
    }

    return clientInputs;
}

Vector<uint8_t> AuthenticationExtensionsClientInputs::toCBOR() const
{
    cbor::CBORValue::MapValue clientInputsMap;
    if (!appid.isEmpty())
        clientInputsMap[cbor::CBORValue("appid")] = cbor::CBORValue(appid);
    if (credProps)
        clientInputsMap[cbor::CBORValue("credProps")] = cbor::CBORValue(credProps);
    if (largeBlob) {
        cbor::CBORValue::MapValue largeBlobMap;
        if (!largeBlob->support.isNull())
            largeBlobMap[cbor::CBORValue("support")] = cbor::CBORValue(largeBlob->support);

        if (largeBlob->read)
            largeBlobMap[cbor::CBORValue("read")] = cbor::CBORValue(largeBlob->read.value());

        if (largeBlob->write)
            largeBlobMap[cbor::CBORValue("write")] = cbor::CBORValue(largeBlob->write.value());

        clientInputsMap[cbor::CBORValue("largeBlob")] = cbor::CBORValue(largeBlobMap);
    }

    auto clientInputs = cbor::CBORWriter::write(cbor::CBORValue(WTFMove(clientInputsMap)));
    ASSERT(clientInputs);

    return *clientInputs;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
