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

#pragma once

#include <WebCore/StyleProperties.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ImmutableStyleProperties);
class ImmutableStyleProperties final : public StyleProperties {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ImmutableStyleProperties, ImmutableStyleProperties);
public:
    inline void deref() const;

    WEBCORE_EXPORT ~ImmutableStyleProperties();
    static Ref<ImmutableStyleProperties> create(std::span<const CSSProperty> properties, CSSParserMode);
    static Ref<ImmutableStyleProperties> createDeduplicating(std::span<const CSSProperty> properties, CSSParserMode);

    unsigned propertyCount() const { return m_arraySize; }
    bool isEmpty() const { return !propertyCount(); }
    PropertyReference propertyAt(unsigned index) const;

    Iterator<ImmutableStyleProperties> begin() const LIFETIME_BOUND { return { *this }; }
    static constexpr std::nullptr_t end() { return nullptr; }
    unsigned size() const { return propertyCount(); }

    int findPropertyIndex(CSSPropertyID) const;
    int findCustomPropertyIndex(StringView propertyName) const;

    static constexpr size_t objectSize(unsigned propertyCount);

    static void clearDeduplicationMap();

    void* m_storage;

private:
    std::span<PackedPtr<const CSSValue>> valueSpan() const;
    std::span<const StylePropertyMetadata> metadataSpan() const;
    ImmutableStyleProperties(std::span<const CSSProperty>, CSSParserMode);
};

inline void ImmutableStyleProperties::deref() const
{
    if (derefBase())
        delete this;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
inline std::span<PackedPtr<const CSSValue>> ImmutableStyleProperties::valueSpan() const
{
    return unsafeMakeSpan(std::bit_cast<PackedPtr<const CSSValue>*>(std::bit_cast<const uint8_t*>(metadataSpan().data()) + (m_arraySize * sizeof(StylePropertyMetadata))), propertyCount());
}
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

inline std::span<const StylePropertyMetadata> ImmutableStyleProperties::metadataSpan() const
{
    return unsafeMakeSpan(reinterpret_cast<const StylePropertyMetadata*>(const_cast<const void**>((&(this->m_storage)))), propertyCount());
}

inline ImmutableStyleProperties::PropertyReference ImmutableStyleProperties::propertyAt(unsigned index) const
{
    return PropertyReference(metadataSpan()[index], valueSpan()[index].get());
}

constexpr size_t ImmutableStyleProperties::objectSize(unsigned count)
{
    return sizeof(ImmutableStyleProperties) - sizeof(void*) + sizeof(StylePropertyMetadata) * count + sizeof(PackedPtr<const CSSValue>) * count;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ImmutableStyleProperties)
    static bool isType(const WebCore::StyleProperties& properties) { return !properties.isMutable(); }
SPECIALIZE_TYPE_TRAITS_END()
