/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyParserConsumer+PointerEvents.h"

#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"

namespace WebCore {
namespace CSSPropertyParserHelpers {

RefPtr<CSSValue> consumeTouchAction(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <'touch-action'> = auto | none | [ [ pan-x | pan-left | pan-right ] || [ pan-y | pan-up | pan-down ] ] | manipulation
    // https://w3c.github.io/pointerevents/#the-touch-action-css-property

    if (auto ident = consumeIdent<CSSValueNone, CSSValueAuto, CSSValueManipulation>(range))
        return ident;

    bool hasPanX = false;
    bool hasPanY = false;
    bool hasPinchZoom = false;
    while (true) {
        auto ident = consumeIdentRaw<CSSValuePanX, CSSValuePanY, CSSValuePinchZoom>(range);
        if (!ident)
            break;
        switch (*ident) {
        case CSSValuePanX:
            if (hasPanX)
                return nullptr;
            hasPanX = true;
            break;
        case CSSValuePanY:
            if (hasPanY)
                return nullptr;
            hasPanY = true;
            break;
        case CSSValuePinchZoom:
            if (hasPinchZoom)
                return nullptr;
            hasPinchZoom = true;
            break;
        default:
            return nullptr;
        }
    }

    if (!hasPanX && !hasPanY && !hasPinchZoom)
        return nullptr;

    CSSValueListBuilder builder;
    if (hasPanX)
        builder.append(CSSPrimitiveValue::create(CSSValuePanX));
    if (hasPanY)
        builder.append(CSSPrimitiveValue::create(CSSValuePanY));
    if (hasPinchZoom)
        builder.append(CSSPrimitiveValue::create(CSSValuePinchZoom));

    return CSSValueList::createSpaceSeparated(WTFMove(builder));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
