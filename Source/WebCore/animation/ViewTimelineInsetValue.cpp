/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
#include "ViewTimelineInsetValue.h"

#include "CSSPropertyParserConsumer+Timeline.h"
#include "Document.h"
#include "StylePrimitiveNumericTypes+TypedCSSOMConversions.h"
#include "StyleViewTimelineInsetItem.h"

namespace WebCore {

static std::optional<Style::ViewTimelineInsetItem::Offset> convertToInsetOffset(Ref<CSSNumericValue>&& numericValue)
{
    if (auto offset = Style::toAbsoluteStyleFromCSSNumericValue<Style::ViewTimelineInsetItem::Offset::Numeric>(WTF::move(numericValue)))
        return Style::ViewTimelineInsetItem::Offset { WTF::move(*offset) };
    return std::nullopt;
}

static std::optional<Style::ViewTimelineInsetItem::Offset> convertToInsetOffset(String&& string)
{
    if (string == "auto"_s)
        return Style::ViewTimelineInsetItem::Offset { CSS::Keyword::Auto { } };
    return std::nullopt;
}

static std::optional<Style::ViewTimelineInsetItem::Offset> convertToInsetOffset(Ref<CSSOMKeywordValue>&& keywordValue)
{
    if (keywordValue->value() == "auto"_s)
        return Style::ViewTimelineInsetItem::Offset { CSS::Keyword::Auto { } };
    return std::nullopt;
}

static std::optional<Style::ViewTimelineInsetItem::Offset> convertToInsetOffset(ViewTimelineIndividualInset&& value)
{
    return WTF::switchOn(WTF::move(value),
        [](auto&& value) {
            return convertToInsetOffset(WTF::move(value));
        }
    );
}

std::optional<Style::ViewTimelineInsetItem> validateViewTimelineInset(ViewTimelineInsetValue&& value, const Document& document)
{
    return WTF::switchOn(WTF::move(value),
        [&](String&& rangeString) -> std::optional<Style::ViewTimelineInsetItem> {
            return CSSPropertyParserHelpers::parseAbsoluteSingleViewTimelineInsetItemRaw(rangeString, document.cssParserContext(), document);
        },
        [&](Vector<ViewTimelineIndividualInset>&& sequence) -> std::optional<Style::ViewTimelineInsetItem> {
            auto numberOfInsets = sequence.size();
            if (!numberOfInsets || numberOfInsets > 2)
                return std::nullopt;

            auto start = convertToInsetOffset(WTF::move(sequence[0]));
            if (!start)
                return std::nullopt;

            if (numberOfInsets == 1)
                return Style::ViewTimelineInsetItem { WTF::move(*start) };

            auto end = convertToInsetOffset(WTF::move(sequence[1]));
            if (!end)
                return std::nullopt;

            return Style::ViewTimelineInsetItem { WTF::move(*start), WTF::move(*end) };
        }
    );
}

} // namespace WebCore
