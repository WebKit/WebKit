/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include "TimelineRangeValue.h"

#include "CSSNumericFactory.h"
#include "CSSPropertyParserConsumer+Timeline.h"
#include "CSSValuePair.h"
#include "CSSValuePool.h"
#include "Document.h"
#include "Element.h"
#include "NodeDocument.h"
#include "NodeInlines.h"
#include "StyleSingleAnimationRange.h"

namespace WebCore {

RefPtr<CSSValue> convertToCSSValue(TimelineRangeValue&& value, RefPtr<Element> element, Style::SingleAnimationRangeType type)
{
    if (!element)
        return { };

    Ref document = element->document();

    return WTF::switchOn(value,
        [&](String& rangeString) -> RefPtr<CSSValue> {
            return CSSPropertyParserHelpers::parseSingleAnimationRange(rangeString, document->cssParserContext(), type);
        },
        [&](TimelineRangeOffset& rangeOffset) -> RefPtr<CSSValue> {
            if (auto consumedRangeName = CSSPropertyParserHelpers::parseSingleAnimationRange(rangeOffset.rangeName, document->cssParserContext(), type)) {
                if (RefPtr offset = rangeOffset.offset)
                    return CSSValuePair::createNoncoalescing(*consumedRangeName, *offset->toCSSValue());
                return consumedRangeName;
            }
            if (RefPtr offset = rangeOffset.offset)
                return offset->toCSSValue();
            return nullptr;
        },
        [&](Ref<CSSKeywordValue> rangeKeyword) {
            return rangeKeyword->toCSSValue();
        },
        [&](Ref<CSSNumericValue> rangeValue) {
            return rangeValue->toCSSValue();
        }
    );
}

} // namespace WebCore
