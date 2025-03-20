/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "CSSPropertyParserConsumer+AppleVisualEffect.h"

#if HAVE(CORE_MATERIAL)

#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSValueKeywords.h"

namespace WebCore::CSSPropertyParserHelpers {

static bool isKeywordValidForAppleVisualEffect(CSSValueID keyword)
{
    switch (keyword) {
    case CSSValueID::CSSValueNone:
    case CSSValueID::CSSValueAppleSystemBlurMaterial:
    case CSSValueID::CSSValueAppleSystemBlurMaterialChrome:
    case CSSValueID::CSSValueAppleSystemBlurMaterialThick:
    case CSSValueID::CSSValueAppleSystemBlurMaterialThin:
    case CSSValueID::CSSValueAppleSystemBlurMaterialUltraThin:
#if HAVE(MATERIAL_HOSTING)
    case CSSValueID::CSSValueAppleSystemHostedBlurMaterial:
    case CSSValueID::CSSValueAppleSystemHostedThinBlurMaterial:
    case CSSValueID::CSSValueAppleSystemHostedMediaControlsMaterial:
#endif
    case CSSValueID::CSSValueAppleSystemVibrancyFill:
    case CSSValueID::CSSValueAppleSystemVibrancyLabel:
    case CSSValueID::CSSValueAppleSystemVibrancyQuaternaryLabel:
    case CSSValueID::CSSValueAppleSystemVibrancySecondaryFill:
    case CSSValueID::CSSValueAppleSystemVibrancySecondaryLabel:
    case CSSValueID::CSSValueAppleSystemVibrancySeparator:
    case CSSValueID::CSSValueAppleSystemVibrancyTertiaryFill:
    case CSSValueID::CSSValueAppleSystemVibrancyTertiaryLabel:
        return true;
    default:
        return false;
    }
}

RefPtr<CSSValue> consumeAppleVisualEffect(CSSParserTokenRange& range, const CSSParserContext&)
{
    return consumeIdent(range, isKeywordValidForAppleVisualEffect);
}

} // namespace WebCore::CSSPropertyParserHelpers

#endif // HAVE(CORE_MATERIAL)
