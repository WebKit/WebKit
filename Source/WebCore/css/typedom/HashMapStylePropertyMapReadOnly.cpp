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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HashMapStylePropertyMapReadOnly.h"

#include "CSSPropertyParser.h"

namespace WebCore {

Ref<HashMapStylePropertyMapReadOnly> HashMapStylePropertyMapReadOnly::create(HashMap<AtomString, RefPtr<CSSValue>>&& map)
{
    return adoptRef(*new HashMapStylePropertyMapReadOnly(WTFMove(map)));
}

HashMapStylePropertyMapReadOnly::HashMapStylePropertyMapReadOnly(HashMap<AtomString, RefPtr<CSSValue>>&& map)
    : m_map(WTFMove(map))
{
}

HashMapStylePropertyMapReadOnly::~HashMapStylePropertyMapReadOnly() = default;

RefPtr<CSSValue> HashMapStylePropertyMapReadOnly::propertyValue(CSSPropertyID propertyID) const
{
    return m_map.get(nameString(propertyID));
}

String HashMapStylePropertyMapReadOnly::shorthandPropertySerialization(CSSPropertyID) const
{
    // FIXME: Not supported.
    return { };
}

RefPtr<CSSValue> HashMapStylePropertyMapReadOnly::customPropertyValue(const AtomString& property) const
{
    return m_map.get(property);
}

unsigned HashMapStylePropertyMapReadOnly::size() const
{
    return m_map.size();
}

auto HashMapStylePropertyMapReadOnly::entries(ScriptExecutionContext* context) const -> Vector<StylePropertyMapEntry>
{
    auto* document = context ? documentFromContext(*context) : nullptr;
    if (!document)
        return { };

    Vector<StylePropertyMapEntry> result;
    result.reserveInitialCapacity(m_map.size());
    for (auto& [propertyName, cssValue] : m_map)
        result.uncheckedAppend(makeKeyValuePair(propertyName,  Vector<RefPtr<CSSStyleValue>> { reifyValue(cssValue.get(), cssPropertyID(propertyName), *document) }));
    return result;
}

} // namespace WebCore
