/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "StyleContent.h"

#include "CSSAttrValue.h"
#include "CSSCounterValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "RenderStyleInlines.h"
#include "RenderStyleSetters.h"
#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<Content>::operator()(BuilderState& state, const CSSValue& value) -> Content
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value)) {
        switch (primitiveValue->valueID()) {
        case CSSValueNormal:
            return CSS::Keyword::Normal { };
        case CSSValueNone:
            return CSS::Keyword::None { };
        default:
            break;
        }

        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Normal { };
    }

    RefPtr contentListAltTextPair = dynamicDowncast<CSSValuePair>(value);
    RefPtr contentList = requiredDowncast<CSSValueList>(state, contentListAltTextPair ? contentListAltTextPair->first() : value);
    if (!contentList)
        return CSS::Keyword::Normal { };

    // FIXME: Replace with support for CSS Values 5 attr() substitution function.
    auto processAttrContent = [&](const CSSAttrValue& value) -> AtomString {
        if (state.style().pseudoElementType() == PseudoId::None)
            state.style().setHasAttrContent();
        else
            const_cast<RenderStyle&>(state.parentStyle()).setHasAttrContent();

        QualifiedName attr(nullAtom(), value.attributeName().impl(), nullAtom());
        RefPtr element = state.element();
        const AtomString& attributeValue = element ? element->getAttribute(attr) : nullAtom();

        // Register the fact that the attribute value affects the style.
        state.registerContentAttribute(attr.localName());

        if (attributeValue.isNull()) {
            RefPtr fallback = dynamicDowncast<CSSPrimitiveValue>(value.fallback());
            return fallback && fallback->isString() ? fallback->stringValue().impl() : emptyAtom();
        }
        return attributeValue.impl();
    };

    auto computeContentList = [&] -> Content::List {
        return Content::List::map(*contentList, [&](const CSSValue& item) -> Content::ListItem {
            if (item.isImage()) {
                if (RefPtr image = state.createStyleImage(item))
                    return Content::Image { ImageWrapper { image.releaseNonNull() } };

                state.setCurrentPropertyInvalidAtComputedValueTime();
                return Content::Text { emptyString() };
            }

            if (RefPtr primitive = dynamicDowncast<CSSPrimitiveValue>(item)) {
                switch (primitive->valueID()) {
                case CSSValueOpenQuote:
                    return Content::Quote { QuoteType::OpenQuote };
                case CSSValueCloseQuote:
                    return Content::Quote { QuoteType::CloseQuote };
                case CSSValueNoOpenQuote:
                    return Content::Quote { QuoteType::NoOpenQuote };
                case CSSValueNoCloseQuote:
                    return Content::Quote { QuoteType::NoCloseQuote };
                default:
                    break;
                }
                if (primitive->isString())
                    return Content::Text { primitive->stringValue() };
                if (RefPtr attr = primitive->cssAttrValue())
                    return Content::Text { processAttrContent(*attr) };

                state.setCurrentPropertyInvalidAtComputedValueTime();
                return Content::Text { emptyString() };
            }

            if (RefPtr counter = dynamicDowncast<CSSCounterValue>(item))
                return Content::Counter { counter->identifier(), counter->separator(), toStyleFromCSSValue<CounterStyle>(state, counter->counterStyle()) };

            state.setCurrentPropertyInvalidAtComputedValueTime();
            return Content::Text { emptyString() };
        });
    };

    auto computeAltText = [&] -> std::optional<String> {
        if (!contentListAltTextPair)
            return { };

        auto altTextList = requiredListDowncast<CSSValueList, CSSPrimitiveValue>(state, contentListAltTextPair->second());
        if (!altTextList)
            return { };

        StringBuilder altTextBuilder;
        for (Ref item : *altTextList) {
            if (item->isString())
                altTextBuilder.append(item->stringValue());
            else if (RefPtr attr = item->cssAttrValue())
                altTextBuilder.append(processAttrContent(*attr));
        }
        return altTextBuilder.toString();
    };

    return Content::Data { computeContentList(), computeAltText() };
}

Ref<CSSValue> CSSValueCreation<Content::Counter>::operator()(CSSValuePool& pool, const RenderStyle& style, const Content::Counter& value)
{
    return CSSCounterValue::create(value.identifier, value.separator, createCSSValue(pool, style, value.style));
}

} // namespace Style
} // namespace WebCore
