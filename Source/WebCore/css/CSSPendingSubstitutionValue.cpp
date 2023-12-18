/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSPendingSubstitutionValue.h"

#include "CSSProperty.h"
#include "CSSPropertyParser.h"

namespace WebCore {

RefPtr<CSSValue> CSSPendingSubstitutionValue::resolveValue(Style::BuilderState& builderState, CSSPropertyID propertyID) const
{
    auto cacheValue = [&](auto data) {
        ParsedPropertyVector parsedProperties;
        if (!CSSPropertyParser::parseValue(m_shorthandPropertyId, false, data->tokens(), data->context(), parsedProperties, StyleRuleType::Style)) {
            m_cachedPropertyValues = { };
            return;
        }

        m_cachedPropertyValues = parsedProperties;
    };

    if (!m_shorthandValue->resolveAndCacheValue(builderState, cacheValue))
        return nullptr;

    for (auto& property : m_cachedPropertyValues) {
        if (property.id() == propertyID)
            return property.value();
    }

    return nullptr;
}

}
