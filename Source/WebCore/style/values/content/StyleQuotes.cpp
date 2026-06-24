/*
 * Copyright (C) 2011 Nokia Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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
#include "CSSQuotesValue.h"
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

auto ToCSS<Quotes>::operator()(const Quotes& value, const Style::ComputedStyle& style) -> CSS::Quotes
{
    return WTF::switchOn(value,
        [&](CSS::SpecificKeyword auto const& keyword) -> CSS::Quotes {
            return toCSS(keyword, style);
        },
        [&](const Quotes::Data& data) -> CSS::Quotes {
            return CSS::Quotes::Data::map(data, [&](const String& item) {
                return toCSS(item, style);
            });
        }
    );
}

auto ToStyle<CSS::Quotes>::operator()(const CSS::Quotes& value, const BuilderState& state) -> Quotes
{
    return WTF::switchOn(value,
        [&](CSS::SpecificKeyword auto const& keyword) -> Quotes {
            return toStyle(keyword, state);
        },
        [&](const CSS::Quotes::Data& data) -> Quotes {
            if (data.size() % 2 != 0) {
                // FIXME: Update ToStyle to pass BuilderState as non-const.
                const_cast<BuilderState&>(state).setCurrentPropertyInvalidAtComputedValueTime();
                return CSS::Keyword::Auto { };
            }

            return Quotes::Data::map(data, [&](const CSS::String& item) {
                return toStyle(item, state);
            });
        }
    );
}

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

    RefPtr quotesValue = requiredDowncast<CSSQuotesValue>(state, value);
    if (!quotesValue)
        return CSS::Keyword::Auto { };

    return toStyle(quotesValue->quotes(), state);
}

Ref<CSSValue> CSSValueCreation<Quotes>::operator()(CSSValuePool&, const Style::ComputedStyle& style, const Quotes& value)
{
    return CSSQuotesValue::create(toCSS(value, style));
}

} // namespace Style
} // namespace WebCore
