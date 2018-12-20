/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "InitDataRegistry.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "ISOProtectionSystemSpecificHeaderBox.h"
#include <JavaScriptCore/DataView.h>
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include <wtf/JSONValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>


namespace WebCore {

namespace {
    const uint32_t kCencMaxBoxSize = 64 * KB;
    // ContentEncKeyID has this EBML code [47][E2] in WebM,
    // as per spec the size of the ContentEncKeyID is encoded on 16 bits.
    // https://matroska.org/technical/specs/index.html#ContentEncKeyID/
    const uint32_t kWebMMaxContentEncKeyIDSize = 64 * KB; // 2^16
    const uint32_t kKeyIdsMinKeyIdSizeInBytes = 1;
    const uint32_t kKeyIdsMaxKeyIdSizeInBytes = 512;
}

static Optional<Vector<Ref<SharedBuffer>>> extractKeyIDsKeyids(const SharedBuffer& buffer)
{
    // 1. Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/keyids.html#format
    if (buffer.size() > std::numeric_limits<unsigned>::max())
        return WTF::nullopt;
    String json { buffer.data(), static_cast<unsigned>(buffer.size()) };

    RefPtr<JSON::Value> value;
    if (!JSON::Value::parseJSON(json, value))
        return WTF::nullopt;

    RefPtr<JSON::Object> object;
    if (!value->asObject(object))
        return WTF::nullopt;

    RefPtr<JSON::Array> kidsArray;
    if (!object->getArray("kids", kidsArray))
        return WTF::nullopt;

    Vector<Ref<SharedBuffer>> keyIDs;
    for (auto& value : *kidsArray) {
        String keyID;
        if (!value->asString(keyID))
            continue;

        Vector<char> keyIDData;
        if (!WTF::base64URLDecode(keyID, { keyIDData }))
            continue;

        if (keyIDData.size() < kKeyIdsMinKeyIdSizeInBytes || keyIDData.size() > kKeyIdsMaxKeyIdSizeInBytes)
            return WTF::nullopt;

        Ref<SharedBuffer> keyIDBuffer = SharedBuffer::create(WTFMove(keyIDData));
        keyIDs.append(WTFMove(keyIDBuffer));
    }

    return keyIDs;
}

static RefPtr<SharedBuffer> sanitizeKeyids(const SharedBuffer& buffer)
{
    // 1. Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/keyids.html#format
    auto keyIDBuffer = extractKeyIDsKeyids(buffer);
    if (!keyIDBuffer)
        return nullptr;

    auto object = JSON::Object::create();
    auto kidsArray = JSON::Array::create();
    for (auto& buffer : keyIDBuffer.value())
        kidsArray->pushString(WTF::base64URLEncode(buffer->data(), buffer->size()));
    object->setArray("kids", WTFMove(kidsArray));

    CString jsonData = object->toJSONString().utf8();
    return SharedBuffer::create(jsonData.data(), jsonData.length());
}

static Optional<Vector<Ref<SharedBuffer>>> extractKeyIDsCenc(const SharedBuffer& buffer)
{
    // 4. Common SystemID and PSSH Box Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/cenc.html#common-system
    if (buffer.size() >= kCencMaxBoxSize)
        return WTF::nullopt;

    unsigned offset = 0;
    Vector<Ref<SharedBuffer>> keyIDs;

    auto view = JSC::DataView::create(buffer.tryCreateArrayBuffer(), offset, buffer.size());
    while (auto optionalBoxType = ISOBox::peekBox(view, offset)) {
        auto& boxTypeName = optionalBoxType.value().first;
        auto& boxSize = optionalBoxType.value().second;

        if (boxTypeName != ISOProtectionSystemSpecificHeaderBox::boxTypeName() || boxSize > buffer.size())
            return WTF::nullopt;

        ISOProtectionSystemSpecificHeaderBox psshBox;
        if (!psshBox.read(view, offset))
            return WTF::nullopt;

        for (auto& value : psshBox.keyIDs())
            keyIDs.append(SharedBuffer::create(WTFMove(value)));
    }

    return keyIDs;
}

static RefPtr<SharedBuffer> sanitizeCenc(const SharedBuffer& buffer)
{
    // 4. Common SystemID and PSSH Box Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/cenc.html#common-system
    if (!extractKeyIDsCenc(buffer))
        return nullptr;

    return buffer.copy();
}

static RefPtr<SharedBuffer> sanitizeWebM(const SharedBuffer& buffer)
{
    // Check if the buffer is a valid WebM initData.
    // The WebM initData is the ContentEncKeyID, so should be less than kWebMMaxContentEncKeyIDSize.
    if (buffer.isEmpty() || buffer.size() > kWebMMaxContentEncKeyIDSize)
        return nullptr;

    return buffer.copy();
}

static Optional<Vector<Ref<SharedBuffer>>> extractKeyIDsWebM(const SharedBuffer& buffer)
{
    Vector<Ref<SharedBuffer>> keyIDs;
    RefPtr<SharedBuffer> sanitizedBuffer = sanitizeWebM(buffer);
    if (!sanitizedBuffer)
        return WTF::nullopt;

    // 1. Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/webm.html#format
    keyIDs.append(sanitizedBuffer.releaseNonNull());
    return keyIDs;
}

InitDataRegistry& InitDataRegistry::shared()
{
    static NeverDestroyed<InitDataRegistry> registry;
    return registry.get();
}

InitDataRegistry::InitDataRegistry()
{
    registerInitDataType("keyids", { sanitizeKeyids, extractKeyIDsKeyids });
    registerInitDataType("cenc", { sanitizeCenc, extractKeyIDsCenc });
    registerInitDataType("webm", { sanitizeWebM, extractKeyIDsWebM });
}

InitDataRegistry::~InitDataRegistry() = default;

RefPtr<SharedBuffer> InitDataRegistry::sanitizeInitData(const AtomicString& initDataType, const SharedBuffer& buffer)
{
    auto iter = m_types.find(initDataType);
    if (iter == m_types.end() || !iter->value.sanitizeInitData)
        return nullptr;
    return iter->value.sanitizeInitData(buffer);
}

Optional<Vector<Ref<SharedBuffer>>> InitDataRegistry::extractKeyIDs(const AtomicString& initDataType, const SharedBuffer& buffer)
{
    auto iter = m_types.find(initDataType);
    if (iter == m_types.end() || !iter->value.sanitizeInitData)
        return WTF::nullopt;
    return iter->value.extractKeyIDs(buffer);
}

void InitDataRegistry::registerInitDataType(const AtomicString& initDataType, InitDataTypeCallbacks&& callbacks)
{
    ASSERT(!m_types.contains(initDataType));
    m_types.set(initDataType, WTFMove(callbacks));
}

}

#endif // ENABLE(ENCRYPTED_MEDIA)
