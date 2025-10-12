/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "StyleQuotes.h"

#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

const String& Quotes::openQuote(unsigned index) const
{
    return WTF::switchOn(m_value,
        [&](const Data& data) -> const String& {
            auto i = index * 2;

            if (i < data.size())
                return data[i];
            return data[data.size() - 2];
        },
        [&](const auto&) -> const String& {
            return emptyString();
        }
    );
}

const String& Quotes::closeQuote(unsigned index) const
{
    return WTF::switchOn(m_value,
        [&](const Data& data) -> const String& {
            auto i = (index * 2) + 1;

            if (i < data.size())
                return data[i];
            return data[data.size() - 1];
        },
        [&](const auto&) -> const String& {
            return emptyString();
        }
    );
}

// MARK: - Conversion

auto CSSValueConversion<Quotes>::operator()(BuilderState& state, const CSSValue& value) -> Quotes
{
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueAuto:
            return CSS::Keyword::Auto { };
        case CSSValueNone:
            return CSS::Keyword::None { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    auto list = requiredListDowncast<CSSValueList, CSSPrimitiveValue, 2>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    if (list->size() % 2 != 0) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    return Quotes::Data::map(*list, [](const CSSPrimitiveValue& item) {
        return item.stringValue();
    });
}

} // namespace Style
} // namespace WebCore
