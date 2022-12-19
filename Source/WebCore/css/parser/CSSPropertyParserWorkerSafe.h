/*
 * Copyright (C) 2021 Metrological Group B.V.
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSParserContext.h"
#include "CSSPropertyParserHelpers.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSPrimitiveValue;
class CSSValue;
class CSSValueList;
class CSSValuePool;
class Color;
class ScriptExecutionContext;

class CSSPropertyParserWorkerSafe {
public:
    static Color parseColor(const String&);
    static std::optional<CSSPropertyParserHelpers::FontRaw> parseFont(const String&, CSSParserMode = HTMLStandardMode);

    static RefPtr<CSSValueList> parseFontFaceSrc(const String&, const CSSParserContext&);
    static RefPtr<CSSValue> parseFontFaceStyle(const String&, ScriptExecutionContext&);
    static RefPtr<CSSValue> parseFontFaceWeight(const String&, ScriptExecutionContext&);
    static RefPtr<CSSValue> parseFontFaceStretch(const String&, ScriptExecutionContext&);
    static RefPtr<CSSValueList> parseFontFaceUnicodeRange(const String&, ScriptExecutionContext&);
    static RefPtr<CSSValue> parseFontFaceFeatureSettings(const String&, ScriptExecutionContext&);
    static RefPtr<CSSPrimitiveValue> parseFontFaceDisplay(const String&, ScriptExecutionContext&);
};

namespace CSSPropertyParserHelpersWorkerSafe {

RefPtr<CSSValueList> consumeFontFaceSrc(CSSParserTokenRange&, const CSSParserContext&);
RefPtr<CSSValue> consumeFontStyle(CSSParserTokenRange&, CSSParserMode, CSSValuePool&);
RefPtr<CSSPrimitiveValue> consumeFontStretchKeywordValue(CSSParserTokenRange&, CSSValuePool&);
RefPtr<CSSPrimitiveValue> consumeFontStretch(CSSParserTokenRange&, CSSValuePool&);
RefPtr<CSSValueList> consumeFontFaceUnicodeRange(CSSParserTokenRange&);
RefPtr<CSSValue> consumeFontFeatureSettings(CSSParserTokenRange&, CSSValuePool&);
RefPtr<CSSPrimitiveValue> consumeFontFaceFontDisplay(CSSParserTokenRange&, CSSValuePool&);

#if ENABLE(VARIATION_FONTS)
RefPtr<CSSValue> consumeFontStyleRange(CSSParserTokenRange&, CSSParserMode, CSSValuePool&);
RefPtr<CSSValue> consumeFontWeightAbsoluteRange(CSSParserTokenRange&, CSSValuePool&);
RefPtr<CSSValue> consumeFontStretchRange(CSSParserTokenRange&, CSSValuePool&);
#endif

#if !ENABLE(VARIATION_FONTS)
RefPtr<CSSPrimitiveValue> consumeFontWeightAbsolute(CSSParserTokenRange&, CSSValuePool&);
#endif

} // namespace CSSPropertyParserHelpersWorkerSafe

} // namespace WebCore
