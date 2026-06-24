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

#include "CSSContentValue.h"
#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleInvalidImage.h"
#include "StyleValueTypes+CSSValueConversion.h"

namespace WebCore {
namespace Style {

WTF::String Content::altText() const
{
    if (auto* contentData = tryData())
        return contentData->alt.value_or(String { nullString() }).value;
    return { };
}

// MARK: - Conversion

template<> struct ToCSS<Content::Data> { auto operator()(const Content::Data&, const Style::ComputedStyle&) -> CSS::Content::Data; };
template<> struct ToStyle<CSS::Content::Data> { auto operator()(const CSS::Content::Data&, const BuilderState&) -> Content::Data; };

auto ToCSS<Content::Data>::operator()(const Content::Data& value, const Style::ComputedStyle& style) -> CSS::Content::Data
{
    auto computeVisibleContentList = [&] -> CSS::Content::VisibleContentList {
        return CSS::Content::VisibleContentList::map(value.visible, [&](const auto& item) -> CSS::Content::VisibleContentListItem {
            return WTF::switchOn(item,
                [&](const Content::Text& text) -> CSS::Content::VisibleContentListItem {
                    return CSS::Content::Text { toCSS(text.text, style) };
                },
                [&](const Content::Image& image) -> CSS::Content::VisibleContentListItem {
                    return CSS::Content::Image { toCSS(image.image, style) };
                },
                [&](const Content::Counter& counter) -> CSS::Content::VisibleContentListItem {
                    if (counter.separator.value.isEmpty()) {
                        return CSS::Content::CounterFunction {
                            .parameters = {
                                toCSS(counter.identifier, style),
                                toCSS(counter.style, style),
                            }
                        };
                    } else {
                        return CSS::Content::CountersFunction {
                            .parameters = {
                                toCSS(counter.identifier, style),
                                toCSS(counter.separator, style),
                                toCSS(counter.style, style),
                            }
                        };
                    }
                },
                [&](const Content::Quote& quote) -> CSS::Content::VisibleContentListItem {
                    switch (quote.quote) {
                    case QuoteType::OpenQuote:
                        return CSS::Content::Quote { CSS::Keyword::OpenQuote { } };
                    case QuoteType::CloseQuote:
                        return CSS::Content::Quote { CSS::Keyword::CloseQuote { } };
                    case QuoteType::NoOpenQuote:
                        return CSS::Content::Quote { CSS::Keyword::NoOpenQuote { } };
                    case QuoteType::NoCloseQuote:
                        return CSS::Content::Quote { CSS::Keyword::NoCloseQuote { } };
                    }
                    RELEASE_ASSERT_NOT_REACHED();
                }
            );
        });
    };

    auto computeAltContentList = [&] -> std::optional<CSS::Content::AltContentList> {
        if (!value.alt)
            return { };

        return CSS::Content::AltContentList {
            CSS::Content::Text { toCSS(*value.alt, style) }
        };
    };

    return {
        .visible = computeVisibleContentList(),
        .alt = computeAltContentList(),
    };
}

auto ToStyle<CSS::Content::Data>::operator()(const CSS::Content::Data& value, const BuilderState& state) -> Content::Data
{
    auto processAttrContent = [&](const CSS::Content::LegacyAttrFunction& value) -> String {
        if (!state.style().pseudoElementType())
            const_cast<BuilderState&>(state).style().setHasAttrContent();
        else
            const_cast<ComputedStyle&>(state.parentStyle()).setHasAttrContent();

        auto attrName = toStyle(value->name, state);
        QualifiedName attr(nullAtom(), attrName.value.impl(), nullAtom());
        RefPtr element = state.element();
        const AtomString& attributeValue = element ? element->getAttribute(attr) : nullAtom();

        // Register the fact that the attribute value affects the style.
        const_cast<BuilderState&>(state).registerSubstitutionAttribute(attr.localName());

        if (attributeValue.isNull()) {
            if (auto fallback = value->fallback)
                return toStyle(*fallback, state);
            return String { emptyString() };
        }
        return String { attributeValue.string() };
    };

    auto computeVisibleContentList = [&] -> Content::VisibleContentList {
        return Content::VisibleContentList::map(value.visible, [&](const auto& item) -> Content::VisibleContentListItem {
            return WTF::switchOn(item,
                [&](const CSS::Content::Text& text) -> Content::VisibleContentListItem {
                    return Content::Text { toStyle(text.text, state) };
                },
                [&](const CSS::Content::LegacyAttrFunction& attr) -> Content::VisibleContentListItem {
                    return Content::Text { processAttrContent(attr) };
                },
                [&](const CSS::Content::Image& image) -> Content::VisibleContentListItem {
                    return Content::Image { toStyle(image.image, state) };
                },
                [&](const CSS::Content::CounterFunction& counterFunction) -> Content::VisibleContentListItem {
                    return Content::Counter {
                        toStyle(counterFunction->identifier, state),
                        String { nullString() },
                        toStyle(counterFunction->style, state),
                    };
                },
                [&](const CSS::Content::CountersFunction& countersFunction) -> Content::VisibleContentListItem {
                    return Content::Counter {
                        toStyle(countersFunction->identifier, state),
                        toStyle(countersFunction->separator, state),
                        toStyle(countersFunction->style, state),
                    };
                },
                [&](const CSS::Content::Quote& quote) -> Content::VisibleContentListItem {
                    return WTF::switchOn(quote,
                        [](CSS::Keyword::OpenQuote) -> Content::Quote { return { QuoteType::OpenQuote }; },
                        [](CSS::Keyword::CloseQuote) -> Content::Quote { return { QuoteType::CloseQuote }; },
                        [](CSS::Keyword::NoOpenQuote) -> Content::Quote { return { QuoteType::NoOpenQuote }; },
                        [](CSS::Keyword::NoCloseQuote) -> Content::Quote { return { QuoteType::NoCloseQuote }; }
                    );
                }
            );
        });
    };

    auto computeAltText = [&] -> std::optional<String> {
        if (!value.alt)
            return { };

        StringBuilder altTextBuilder;
        for (auto& item : *value.alt) {
            WTF::switchOn(item,
                [&](const CSS::Content::Text& text) {
                    altTextBuilder.append(toStyle(text.text, state).value);
                },
                [&](const CSS::Content::LegacyAttrFunction& attr) {
                    altTextBuilder.append(processAttrContent(attr).value);
                }
            );
        }
        return String { altTextBuilder.toString() };
    };

    return {
        .visible = computeVisibleContentList(),
        .alt = computeAltText(),
    };
}

auto ToCSS<Content>::operator()(const Content& value, const Style::ComputedStyle& style) -> CSS::Content
{
    return WTF::switchOn(value, [&](const auto& alternative) -> CSS::Content { return toCSS(alternative, style); });
}

auto ToStyle<CSS::Content>::operator()(const CSS::Content& value, const BuilderState& state) -> Content
{
    return WTF::switchOn(value, [&](const auto& alternative) -> Content { return toStyle(alternative, state); });
}

auto CSSValueConversion<Content>::operator()(BuilderState& state, const CSSValue& value) -> Content
{
    if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
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

    RefPtr contentValue = requiredDowncast<CSSContentValue>(state, value);
    if (!contentValue)
        return CSS::Keyword::Normal { };

    return toStyle(contentValue->content(), state);
}

Ref<CSSValue> CSSValueCreation<Content>::operator()(CSSValuePool&, const Style::ComputedStyle& style, const Content& value)
{
    return CSSContentValue::create(toCSS(value, style));
}

} // namespace Style
} // namespace WebCore
