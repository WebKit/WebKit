/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#include "PasteboardCustomData.h"

#include "SharedBuffer.h"
#include <wtf/URLParser.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

static Variant<RefPtr<StringImpl>, Ref<SharedBuffer>> copyPlatformData(const Variant<RefPtr<StringImpl>, Ref<SharedBuffer>>& other)
{
    if (std::holds_alternative<RefPtr<StringImpl>>(other))
        return { std::get<RefPtr<StringImpl>>(other) };

    if (std::holds_alternative<Ref<SharedBuffer>>(other))
        return { std::get<Ref<SharedBuffer>>(other).copyRef() };

    return { };
}

PasteboardCustomData::Entry::Entry(const Entry& entry)
    : type(entry.type)
    , customData(entry.customData)
    , platformData(copyPlatformData(entry.platformData))
{
}

PasteboardCustomData::Entry::Entry(StringImpl* dataType)
    : type(dataType)
{
}

PasteboardCustomData::Entry::Entry() = default;
PasteboardCustomData::Entry::Entry(Entry&&) = default;

PasteboardCustomData::Entry::Entry(StringImpl* type, StringImpl* customData, const Variant<RefPtr<StringImpl>, Ref<WebCore::SharedBuffer>>& platformData)
    : type(type)
    , customData(customData)
    , platformData(platformData)
{
}

PasteboardCustomData::Entry& PasteboardCustomData::Entry::operator=(const Entry& entry)
{
    type = entry.type;
    customData = entry.customData;
    platformData = copyPlatformData(entry.platformData);
    return *this;
}

PasteboardCustomData::Entry& PasteboardCustomData::Entry::operator=(Entry&&) = default;

PasteboardCustomData::PasteboardCustomData() = default;
PasteboardCustomData::PasteboardCustomData(const PasteboardCustomData&) = default;
PasteboardCustomData::PasteboardCustomData(PasteboardCustomData&&) = default;
PasteboardCustomData::~PasteboardCustomData() = default;

PasteboardCustomData::PasteboardCustomData(RefPtr<StringImpl>&& origin, Vector<Entry>&& data)
    : m_origin(WTF::move(origin))
    , m_data(WTF::move(data))
{
}

Ref<SharedBuffer> PasteboardCustomData::createSharedBuffer() const
{
    constexpr unsigned currentCustomDataSerializationVersion = 1;

    WTF::Persistence::Encoder encoder;
    encoder << currentCustomDataSerializationVersion;
    encoder << m_origin;
    encoder << sameOriginCustomStringData();
    encoder << orderedTypes();
    return SharedBuffer::create(encoder.span());
}

PasteboardCustomData PasteboardCustomData::fromPersistenceDecoder(WTF::Persistence::Decoder&& decoder)
{
    constexpr unsigned maxSupportedDataSerializationVersionNumber = 1;

    PasteboardCustomData result;
    std::optional<unsigned> version;
    decoder >> version;
    if (!version || *version > maxSupportedDataSerializationVersionNumber)
        return { };

    std::optional<RefPtr<StringImpl>> origin;
    decoder >> origin;
    if (!origin)
        return { };
    result.m_origin = WTF::move(*origin);

    std::optional<HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>>> sameOriginCustomStringData;
    decoder >> sameOriginCustomStringData;
    if (!sameOriginCustomStringData)
        return { };

    std::optional<Vector<RefPtr<StringImpl>>> orderedTypes;
    decoder >> orderedTypes;
    if (!orderedTypes)
        return { };

    for (auto& type : *orderedTypes)
        result.writeStringInCustomData(type, sameOriginCustomStringData->get(type));

    return result;
}

PasteboardCustomData PasteboardCustomData::fromSharedBuffer(const SharedBuffer& buffer)
{
    return fromPersistenceDecoder(buffer.decoder());
}

void PasteboardCustomData::writeString(StringImpl* type, StringImpl* value)
{
    addOrMoveEntryToEnd(type).platformData = { value };
}

void PasteboardCustomData::writeData(StringImpl* type, Ref<SharedBuffer>&& data)
{
    addOrMoveEntryToEnd(type).platformData = { WTF::move(data) };
}

void PasteboardCustomData::writeStringInCustomData(StringImpl* type, StringImpl* value)
{
    addOrMoveEntryToEnd(type).customData = value;
}

PasteboardCustomData::Entry& PasteboardCustomData::addOrMoveEntryToEnd(StringImpl* type)
{
    auto index = m_data.findIf([&] (auto& entry) {
        return equal(entry.type, type);
    });
    auto entry = index == notFound ? Entry(type) : m_data[index];
    if (index != notFound)
        m_data.removeAt(index);
    m_data.append(WTF::move(entry));
    return m_data.last();
}

void PasteboardCustomData::clear()
{
    m_data.clear();
}

void PasteboardCustomData::clear(StringImpl* type)
{
    m_data.removeFirstMatching([&] (auto& entry) {
        return equal(entry.type, type);
    });
}

PasteboardCustomData& PasteboardCustomData::operator=(const PasteboardCustomData&) = default;

Vector<RefPtr<StringImpl>> PasteboardCustomData::orderedTypes() const
{
    return m_data.map([&] (auto& entry) {
        return entry.type;
    });
}

bool PasteboardCustomData::hasData() const
{
    return !m_data.isEmpty();
}

bool PasteboardCustomData::hasSameOriginCustomData() const
{
    return notFound != m_data.findIf([&] (auto& entry) {
        return !!entry.customData;
    });
}

HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> PasteboardCustomData::sameOriginCustomStringData() const
{
    HashMap<RefPtr<StringImpl>, RefPtr<StringImpl>> customData;
    for (auto& entry : m_data)
        customData.set(entry.type, entry.customData);
    return customData;
}

RefPtr<SharedBuffer> PasteboardCustomData::readBuffer(StringImpl* type) const
{
    for (auto& entry : m_data) {
        if (!equal(entry.type, type))
            continue;

        if (std::holds_alternative<Ref<SharedBuffer>>(entry.platformData))
            return std::get<Ref<SharedBuffer>>(entry.platformData).copyRef();

        return nullptr;
    }
    return nullptr;
}

RefPtr<StringImpl> PasteboardCustomData::readString(StringImpl* type) const
{
    for (auto& entry : m_data) {
        if (!equal(entry.type, type))
            continue;

        if (std::holds_alternative<RefPtr<StringImpl>>(entry.platformData))
            return std::get<RefPtr<StringImpl>>(entry.platformData);

        return { };
    }
    return { };
}

RefPtr<StringImpl> PasteboardCustomData::readStringInCustomData(StringImpl* type) const
{
    for (auto& entry : m_data) {
        if (equal(entry.type, type))
            return entry.customData;
    }
    return { };
}

void PasteboardCustomData::forEachType(NOESCAPE const Function<void(StringImpl*)>& function) const
{
    for (auto& entry : m_data)
        function(entry.type);
}

void PasteboardCustomData::forEachPlatformString(NOESCAPE const Function<void(StringImpl* type, StringImpl* data)>& function) const
{
    for (auto& entry : m_data) {
        if (!std::holds_alternative<RefPtr<StringImpl>>(entry.platformData))
            continue;

        auto string = std::get<RefPtr<StringImpl>>(entry.platformData);
        if (string)
            function(entry.type, string);
    }
}

void PasteboardCustomData::forEachCustomString(NOESCAPE const Function<void(StringImpl* type, StringImpl* data)>& function) const
{
    for (auto& entry : m_data) {
        if (entry.customData)
            function(entry.type, entry.customData);
    }
}

void PasteboardCustomData::forEachPlatformStringOrBuffer(NOESCAPE const Function<void(StringImpl* type, const Variant<RefPtr<StringImpl>, Ref<SharedBuffer>>& data)>& function) const
{
    for (auto& entry : m_data) {
        auto& data = entry.platformData;
        if ((std::holds_alternative<RefPtr<StringImpl>>(data) && !!std::get<RefPtr<StringImpl>>(data)) || std::holds_alternative<Ref<SharedBuffer>>(data))
            function(entry.type, data);
    }
}

} // namespace WebCore
