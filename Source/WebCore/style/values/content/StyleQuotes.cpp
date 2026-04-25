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

#include "CSSKeywordValue.h"
#include "CSSStringValue.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

const WTF::String& Quotes::openQuote(unsigned index) const
{
    return WTF::switchOn(m_value,
        [&](const Data& data) -> const WTF::String& {
            auto i = index * 2;

            if (i < data.size())
                return data[i].value;
            return data[data.size() - 2].value;
        },
        [&](const auto&) -> const WTF::String& {
            return emptyString();
        }
    );
}

const WTF::String& Quotes::closeQuote(unsigned index) const
{
    return WTF::switchOn(m_value,
        [&](const Data& data) -> const WTF::String& {
            auto i = (index * 2) + 1;

            if (i < data.size())
                return data[i].value;
            return data[data.size() - 1].value;
        },
        [&](const auto&) -> const WTF::String& {
            return emptyString();
        }
    );
}

// MARK: - Conversion

auto CSSValueConversion<Quotes>::operator()(BuilderState& state, const CSSValue& value) -> Quotes
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
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

    auto list = requiredListDowncast<CSSValueList, CSSStringValue, 2>(state, value);
    if (!list)
        return CSS::Keyword::Auto { };

    if (list->size() % 2 != 0) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }

    return Quotes::Data::map(*list, [&](const CSSStringValue& item) {
        return toStyle(item.string(), state);
    });
}

} // namespace Style
} // namespace WebCore
