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

#include "NotImplemented.h"
#include "SharedBuffer.h"
#include <wtf/JSONValues.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>

using namespace Inspector;

namespace WebCore {

static Vector<Ref<SharedBuffer>> extractKeyIDsKeyids(const SharedBuffer& buffer)
{
    // 1. Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/keyids.html#format
    if (buffer.size() > std::numeric_limits<unsigned>::max())
        return { };
    String json { buffer.data(), static_cast<unsigned>(buffer.size()) };

    RefPtr<JSON::Value> value;
    if (!JSON::Value::parseJSON(json, value))
        return { };

    RefPtr<JSON::Object> object;
    if (!value->asObject(object))
        return { };

    RefPtr<JSON::Array> kidsArray;
    if (!object->getArray("kids", kidsArray))
        return { };

    Vector<Ref<SharedBuffer>> keyIDs;
    for (auto& value : *kidsArray) {
        String keyID;
        if (!value->asString(keyID))
            continue;

        Vector<char> keyIDData;
        if (!WTF::base64URLDecode(keyID, { keyIDData }))
            continue;

        Ref<SharedBuffer> keyIDBuffer = SharedBuffer::create(WTFMove(keyIDData));
        keyIDs.append(WTFMove(keyIDBuffer));
    }

    return keyIDs;
}

static RefPtr<SharedBuffer> sanitizeKeyids(const SharedBuffer& buffer)
{
    // 1. Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/keyids.html#format
    Vector<Ref<SharedBuffer>> keyIDBuffer = extractKeyIDsKeyids(buffer);
    if (keyIDBuffer.isEmpty())
        return nullptr;

    auto object = JSON::Object::create();
    auto kidsArray = JSON::Array::create();
    for (auto& buffer : keyIDBuffer)
        kidsArray->pushString(WTF::base64URLEncode(buffer->data(), buffer->size()));
    object->setArray("kids", WTFMove(kidsArray));

    CString jsonData = object->toJSONString().utf8();
    return SharedBuffer::create(jsonData.data(), jsonData.length());
}

static RefPtr<SharedBuffer> sanitizeCenc(const SharedBuffer& buffer)
{
    // 4. Common SystemID and PSSH Box Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/cenc.html#common-system
    notImplemented();
    return buffer.copy();
}

static Vector<Ref<SharedBuffer>> extractKeyIDsCenc(const SharedBuffer&)
{
    // 4. Common SystemID and PSSH Box Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/cenc.html#common-system
    notImplemented();
    return { };
}

static RefPtr<SharedBuffer> sanitizeWebM(const SharedBuffer& buffer)
{
    // 1. Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/webm.html#format
    notImplemented();
    return buffer.copy();
}

static Vector<Ref<SharedBuffer>> extractKeyIDsWebM(const SharedBuffer&)
{
    // 1. Format
    // https://w3c.github.io/encrypted-media/format-registry/initdata/webm.html#format
    notImplemented();
    return { };
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

Vector<Ref<SharedBuffer>> InitDataRegistry::extractKeyIDs(const AtomicString& initDataType, const SharedBuffer& buffer)
{
    auto iter = m_types.find(initDataType);
    if (iter == m_types.end() || !iter->value.sanitizeInitData)
        return { };
    return iter->value.extractKeyIDs(buffer);
}

void InitDataRegistry::registerInitDataType(const AtomicString& initDataType, InitDataTypeCallbacks&& callbacks)
{
    ASSERT(!m_types.contains(initDataType));
    m_types.set(initDataType, WTFMove(callbacks));
}

}

#endif // ENABLE(ENCRYPTED_MEDIA)
