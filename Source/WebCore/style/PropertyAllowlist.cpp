/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "PropertyAllowlist.h"

namespace WebCore {
namespace Style {

PropertyAllowlist propertyAllowlistForPseudoId(PseudoId pseudoId)
{
    if (pseudoId == PseudoId::Marker)
        return PropertyAllowlist::Marker;
    return PropertyAllowlist::None;
}

// https://drafts.csswg.org/css-lists-3/#marker-properties (Editor's Draft, 14 July 2021)
// FIXME: this is outdated, see https://bugs.webkit.org/show_bug.cgi?id=218791.
bool isValidMarkerStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyColor:
    case CSSPropertyContent:
    case CSSPropertyCustom:
    case CSSPropertyDirection:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontFeatureSettings:
    case CSSPropertyFontKerning:
    case CSSPropertyFontSize:
    case CSSPropertyFontSizeAdjust:
    case CSSPropertyFontStretch:
    case CSSPropertyFontStyle:
    case CSSPropertyFontSynthesis:
    case CSSPropertyFontSynthesisWeight:
    case CSSPropertyFontSynthesisStyle:
    case CSSPropertyFontSynthesisSmallCaps:
    case CSSPropertyFontVariantAlternates:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantEastAsian:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontVariantPosition:
    case CSSPropertyFontWeight:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyFontOpticalSizing:
    case CSSPropertyFontVariationSettings:
#endif
    case CSSPropertyHyphens:
    case CSSPropertyLetterSpacing:
    case CSSPropertyLineBreak:
    case CSSPropertyLineHeight:
    case CSSPropertyListStyle:
    case CSSPropertyOverflowWrap:
    case CSSPropertyTabSize:
    case CSSPropertyTextCombineUpright:
    case CSSPropertyTextDecorationSkipInk:
    case CSSPropertyTextEmphasis:
    case CSSPropertyTextEmphasisColor:
    case CSSPropertyTextEmphasisPosition:
    case CSSPropertyTextEmphasisStyle:
    case CSSPropertyTextShadow:
    case CSSPropertyTextTransform:
    case CSSPropertyTextWrapMode:
    case CSSPropertyTextWrapStyle:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyWordBreak:
    case CSSPropertyWordSpacing:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWhiteSpaceCollapse:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationTimingFunction:
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationComposition:
    case CSSPropertyAnimationName:
    case CSSPropertyTransitionBehavior:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionProperty:
        return true;
    default:
        break;
    }
    return false;
}

#if ENABLE(VIDEO)
bool isValidCueStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyBackground:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundSize:
    case CSSPropertyColor:
    case CSSPropertyCustom:
    case CSSPropertyFont:
    case CSSPropertyFontFamily:
    case CSSPropertyFontSize:
    case CSSPropertyFontStyle:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontWeight:
    case CSSPropertyLineHeight:
    case CSSPropertyOpacity:
    case CSSPropertyOutline:
    case CSSPropertyOutlineColor:
    case CSSPropertyOutlineOffset:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOutlineWidth:
    case CSSPropertyVisibility:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWhiteSpaceCollapse:
    case CSSPropertyTextDecorationLine:
    case CSSPropertyTextShadow:
    case CSSPropertyTextWrapMode:
    case CSSPropertyTextWrapStyle:
    case CSSPropertyBorderStyle:
    case CSSPropertyPaintOrder:
    case CSSPropertyStrokeLinejoin:
    case CSSPropertyStrokeLinecap:
    case CSSPropertyStrokeColor:
    case CSSPropertyStrokeWidth:
        return true;
    default:
        break;
    }
    return false;
}
#endif

}
}
