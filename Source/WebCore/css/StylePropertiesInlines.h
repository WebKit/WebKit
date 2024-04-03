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

#include "CSSPropertyParser.h"
#include "ImmutableStyleProperties.h"
#include "MutableStyleProperties.h"

namespace WebCore {

inline StyleProperties::StyleProperties(CSSParserMode mode)
    : m_cssParserMode(mode)
    , m_isMutable(true)
{
}

inline StyleProperties::StyleProperties(CSSParserMode mode, unsigned immutableArraySize)
    : m_cssParserMode(mode)
    , m_isMutable(false)
    , m_arraySize(immutableArraySize)
{
}

inline StyleProperties::PropertyReference StyleProperties::propertyAt(unsigned index) const
{
    if (m_isMutable)
        return uncheckedDowncast<MutableStyleProperties>(*this).propertyAt(index);
    return uncheckedDowncast<ImmutableStyleProperties>(*this).propertyAt(index);
}

inline unsigned StyleProperties::propertyCount() const
{
    if (m_isMutable)
        return uncheckedDowncast<MutableStyleProperties>(*this).propertyCount();
    return uncheckedDowncast<ImmutableStyleProperties>(*this).propertyCount();
}

inline void StyleProperties::deref() const
{
    if (!derefBase())
        return;

    if (auto* mutableProperties = dynamicDowncast<MutableStyleProperties>(*this))
        delete mutableProperties;
    else if (auto* immutableProperties = dynamicDowncast<ImmutableStyleProperties>(*this))
        delete immutableProperties;
    else
        RELEASE_ASSERT_NOT_REACHED();
}

inline int StyleProperties::findPropertyIndex(CSSPropertyID propertyID) const
{
    if (m_isMutable)
        return uncheckedDowncast<MutableStyleProperties>(*this).findPropertyIndex(propertyID);
    return uncheckedDowncast<ImmutableStyleProperties>(*this).findPropertyIndex(propertyID);
}

inline int StyleProperties::findCustomPropertyIndex(StringView propertyName) const
{
    if (m_isMutable)
        return uncheckedDowncast<MutableStyleProperties>(*this).findCustomPropertyIndex(propertyName);
    return uncheckedDowncast<ImmutableStyleProperties>(*this).findCustomPropertyIndex(propertyName);
}

inline bool StyleProperties::isEmpty() const
{
    return !propertyCount();
}

inline unsigned StyleProperties::size() const
{
    return propertyCount();
}

inline String serializeLonghandValue(CSSPropertyID property, const CSSValue* value)
{
    return value ? serializeLonghandValue(property, *value) : String();
}

inline CSSValueID longhandValueID(CSSPropertyID property, const CSSValue& value)
{
    return value.isImplicitInitialValue() ? initialValueIDForLonghand(property) : valueID(value);
}

inline std::optional<CSSValueID> longhandValueID(CSSPropertyID property, const CSSValue* value)
{
    if (!value)
        return std::nullopt;
    return longhandValueID(property, *value);
}

}
