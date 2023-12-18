/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImmutableStyleProperties.h"

#include "CSSCustomPropertyValue.h"
#include "StylePropertiesInlines.h"
#include <wtf/HashMap.h>
#include <wtf/Hasher.h>

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ImmutableStyleProperties);

ImmutableStyleProperties::ImmutableStyleProperties(const CSSProperty* properties, unsigned length, CSSParserMode mode)
    : StyleProperties(mode, length)
{
    auto* metadataArray = const_cast<StylePropertyMetadata*>(this->metadataArray());
    auto* valueArray = bitwise_cast<PackedPtr<CSSValue>*>(this->valueArray());
    for (unsigned i = 0; i < length; ++i) {
        metadataArray[i] = properties[i].metadata();
        auto* value = properties[i].value();
        valueArray[i] = value;
        value->ref();
    }
}

ImmutableStyleProperties::~ImmutableStyleProperties()
{
    auto* valueArray = bitwise_cast<PackedPtr<CSSValue>*>(this->valueArray());
    for (unsigned i = 0; i < m_arraySize; ++i)
        valueArray[i]->deref();
}

Ref<ImmutableStyleProperties> ImmutableStyleProperties::create(const CSSProperty* properties, unsigned count, CSSParserMode mode)
{
    void* slot = ImmutableStylePropertiesMalloc::malloc(objectSize(count));
    return adoptRef(*new (NotNull, slot) ImmutableStyleProperties(properties, count, mode));
}

static auto& deduplicationMap()
{
    static NeverDestroyed<HashMap<unsigned, Ref<ImmutableStyleProperties>, AlreadyHashed>> map;
    return map.get();
}

Ref<ImmutableStyleProperties> ImmutableStyleProperties::createDeduplicating(const CSSProperty* properties, unsigned count, CSSParserMode mode)
{
    static constexpr auto maximumDeduplicationMapSize = 1024u;
    if (deduplicationMap().size() >= maximumDeduplicationMapSize)
        deduplicationMap().remove(deduplicationMap().random());

    auto computeHash = [&] {
        Hasher hasher;
        add(hasher, mode);
        for (auto* property = properties; property < properties + count; ++property) {
            if (!property->value()->addHash(hasher))
                return 0u;
            add(hasher, property->id(), property->isImportant());
        }
        return hasher.hash();
    };

    auto hash = computeHash();
    if (!hash)
        return create(properties, count, mode);

    auto result = deduplicationMap().ensure(hash, [&] {
        return create(properties, count, mode);
    });

    auto isEqual = [&](auto& existingValue) {
        if (existingValue.propertyCount() != count)
            return false;
        if (existingValue.cssParserMode() != mode)
            return false;
        for (unsigned i = 0; i < count; ++i) {
            if (existingValue.propertyAt(i).toCSSProperty() != *(properties + i))
                return false;
        }
        return true;
    };

    if (!result.isNewEntry && !isEqual(result.iterator->value.get()))
        return create(properties, count, mode);

    return result.iterator->value;
}

void ImmutableStyleProperties::clearDeduplicationMap()
{
    deduplicationMap().clear();
}

int ImmutableStyleProperties::findPropertyIndex(CSSPropertyID propertyID) const
{
    // Convert here propertyID into an uint16_t to compare it with the metadata's m_propertyID to avoid
    // the compiler converting it to an int multiple times in the loop.
    uint16_t id = enumToUnderlyingType(propertyID);
    for (int n = m_arraySize - 1 ; n >= 0; --n) {
        if (metadataArray()[n].m_propertyID == id)
            return n;
    }
    return -1;
}

int ImmutableStyleProperties::findCustomPropertyIndex(StringView propertyName) const
{
    for (int n = m_arraySize - 1 ; n >= 0; --n) {
        if (metadataArray()[n].m_propertyID == CSSPropertyCustom) {
            // We found a custom property. See if the name matches.
            auto* value = valueArray()[n].get();
            if (!value)
                continue;
            if (downcast<CSSCustomPropertyValue>(*value).name() == propertyName)
                return n;
        }
    }
    return -1;
}

}
