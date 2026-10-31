/*
 * Copyright (C) 2021-2026 Apple Inc. All rights reserved.
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

#include "CSSProperty.h"

namespace WebCore {
namespace Style {

PropertyAllowlist propertyAllowlistForPseudoElement(PseudoElementType type)
{
    switch (type) {
    case PseudoElementType::GrammarError:
    case PseudoElementType::Highlight:
    case PseudoElementType::Selection:
    case PseudoElementType::SpellingError:
    case PseudoElementType::TargetText:
        return PropertyAllowlist::Highlight;
    case PseudoElementType::Marker:
        return PropertyAllowlist::Marker;
    case PseudoElementType::FirstLetter:
        return PropertyAllowlist::FirstLetter;
    case PseudoElementType::FirstLine:
        return PropertyAllowlist::FirstLine;
    default:
        return PropertyAllowlist::None;
    }
}

// https://drafts.csswg.org/css-pseudo-4/#highlight-styling
bool isValidHighlightStyleProperty(CSSPropertyID id)
{
    // Custom properties are not part of the applicable property list but are allowed, since they
    // can be substituted into the properties that are. https://drafts.csswg.org/css-pseudo-4/#highlight-cascade
    if (id == CSSPropertyID::Custom)
        return true;
    return CSSProperty::appliesToHighlightPseudoElements(id);
}

// https://drafts.csswg.org/css-lists-3/#marker-properties
bool isValidMarkerStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyID::Color:
    case CSSPropertyID::Content:
    case CSSPropertyID::Custom:
    case CSSPropertyID::Cursor:
    case CSSPropertyID::Direction:
    case CSSPropertyID::Font:
    case CSSPropertyID::FontFamily:
    case CSSPropertyID::FontFeatureSettings:
    case CSSPropertyID::FontKerning:
    case CSSPropertyID::FontSize:
    case CSSPropertyID::FontSizeAdjust:
    case CSSPropertyID::FontWidth:
    case CSSPropertyID::FontStyle:
    case CSSPropertyID::FontSynthesis:
    case CSSPropertyID::FontSynthesisWeight:
    case CSSPropertyID::FontSynthesisStyle:
    case CSSPropertyID::FontSynthesisSmallCaps:
    case CSSPropertyID::FontVariantAlternates:
    case CSSPropertyID::FontVariantCaps:
    case CSSPropertyID::FontVariantEastAsian:
    case CSSPropertyID::FontVariantLigatures:
    case CSSPropertyID::FontVariantNumeric:
    case CSSPropertyID::FontVariantPosition:
    case CSSPropertyID::FontWeight:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyID::FontOpticalSizing:
    case CSSPropertyID::FontVariationSettings:
#endif
    case CSSPropertyID::Hyphens:
    case CSSPropertyID::LetterSpacing:
    case CSSPropertyID::LineBreak:
    case CSSPropertyID::LineHeight:
    case CSSPropertyID::ListStyle:
    case CSSPropertyID::OverflowWrap:
    case CSSPropertyID::Quotes:
    case CSSPropertyID::TabSize:
    case CSSPropertyID::TextCombineUpright:
    case CSSPropertyID::TextDecorationSkipInk:
    case CSSPropertyID::TextEmphasis:
    case CSSPropertyID::TextEmphasisColor:
    case CSSPropertyID::TextEmphasisPosition:
    case CSSPropertyID::TextEmphasisStyle:
    case CSSPropertyID::TextIndent:
    case CSSPropertyID::TextOrientation:
    case CSSPropertyID::TextShadow:
    case CSSPropertyID::TextTransform:
    case CSSPropertyID::TextWrapMode:
    case CSSPropertyID::TextWrapStyle:
    case CSSPropertyID::UnicodeBidi:
    case CSSPropertyID::WebkitTextFillColor:
    case CSSPropertyID::WebkitTextOrientation:
    case CSSPropertyID::WordBreak:
    case CSSPropertyID::WordSpacing:
    case CSSPropertyID::WhiteSpace:
    case CSSPropertyID::WhiteSpaceCollapse:
    case CSSPropertyID::AnimationDuration:
    case CSSPropertyID::AnimationTimingFunction:
    case CSSPropertyID::AnimationDelay:
    case CSSPropertyID::AnimationIterationCount:
    case CSSPropertyID::AnimationDirection:
    case CSSPropertyID::AnimationFillMode:
    case CSSPropertyID::AnimationPlayState:
    case CSSPropertyID::AnimationComposition:
    case CSSPropertyID::AnimationName:
    case CSSPropertyID::AnimationRangeEnd:
    case CSSPropertyID::AnimationRangeStart:
    case CSSPropertyID::AnimationTimeline:
    case CSSPropertyID::TransitionBehavior:
    case CSSPropertyID::TransitionDuration:
    case CSSPropertyID::TransitionTimingFunction:
    case CSSPropertyID::TransitionDelay:
    case CSSPropertyID::TransitionProperty:
        return true;
    default:
        break;
    }
    return false;
}

// https://drafts.csswg.org/css-pseudo/#first-letter-styling
bool isValidFirstLetterStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyID::BackgroundAttachment:
    case CSSPropertyID::BackgroundBlendMode:
    case CSSPropertyID::BackgroundClip:
    case CSSPropertyID::BackgroundColor:
    case CSSPropertyID::BackgroundImage:
    case CSSPropertyID::BackgroundOrigin:
    case CSSPropertyID::BackgroundPositionX:
    case CSSPropertyID::BackgroundPositionY:
    case CSSPropertyID::BackgroundRepeat:
    case CSSPropertyID::BackgroundSize:
    case CSSPropertyID::BorderBottomColor:
    case CSSPropertyID::BorderBottomStyle:
    case CSSPropertyID::BorderBottomWidth:
    case CSSPropertyID::BorderLeftColor:
    case CSSPropertyID::BorderLeftStyle:
    case CSSPropertyID::BorderLeftWidth:
    case CSSPropertyID::BorderRightColor:
    case CSSPropertyID::BorderRightStyle:
    case CSSPropertyID::BorderRightWidth:
    case CSSPropertyID::BorderTopColor:
    case CSSPropertyID::BorderTopStyle:
    case CSSPropertyID::BorderTopWidth:
    case CSSPropertyID::BorderBottomLeftRadius:
    case CSSPropertyID::BorderBottomRightRadius:
    case CSSPropertyID::BorderTopLeftRadius:
    case CSSPropertyID::BorderTopRightRadius:
    case CSSPropertyID::BorderBlockStyle:
    case CSSPropertyID::BorderImageSource:
    case CSSPropertyID::BorderImageWidth:
    case CSSPropertyID::BorderImageOutset:
    case CSSPropertyID::BorderImageRepeat:
    case CSSPropertyID::BorderImageSlice:
    case CSSPropertyID::BorderInlineStartColor:
    case CSSPropertyID::BorderInlineStartStyle:
    case CSSPropertyID::BorderInlineStartWidth:
    case CSSPropertyID::BorderInlineEndColor:
    case CSSPropertyID::BorderInlineEndStyle:
    case CSSPropertyID::BorderInlineEndWidth:
    case CSSPropertyID::BorderBlockStartColor:
    case CSSPropertyID::BorderBlockStartStyle:
    case CSSPropertyID::BorderBlockStartWidth:
    case CSSPropertyID::BorderBlockEndColor:
    case CSSPropertyID::BorderBlockEndStyle:
    case CSSPropertyID::BorderBlockEndWidth:
    case CSSPropertyID::BorderStartStartRadius:
    case CSSPropertyID::BorderStartEndRadius:
    case CSSPropertyID::BorderEndStartRadius:
    case CSSPropertyID::BorderEndEndRadius:
    case CSSPropertyID::WebkitBorderImage:
    case CSSPropertyID::BoxShadow:
    case CSSPropertyID::CornerStartStartShape:
    case CSSPropertyID::CornerStartEndShape:
    case CSSPropertyID::CornerEndStartShape:
    case CSSPropertyID::CornerEndEndShape:
    case CSSPropertyID::CornerBottomLeftShape:
    case CSSPropertyID::CornerBottomRightShape:
    case CSSPropertyID::CornerTopLeftShape:
    case CSSPropertyID::CornerTopRightShape:
    case CSSPropertyID::Color:
    case CSSPropertyID::Custom:
    case CSSPropertyID::Float:
    case CSSPropertyID::FontFamily:
    case CSSPropertyID::FontFeatureSettings:
    case CSSPropertyID::FontKerning:
    case CSSPropertyID::FontSize:
    case CSSPropertyID::FontSizeAdjust:
    case CSSPropertyID::FontPalette:
    case CSSPropertyID::FontStyle:
    case CSSPropertyID::FontSynthesisWeight:
    case CSSPropertyID::FontSynthesisStyle:
    case CSSPropertyID::FontSynthesisSmallCaps:
    case CSSPropertyID::FontVariantAlternates:
    case CSSPropertyID::FontVariantCaps:
    case CSSPropertyID::FontVariantEastAsian:
    case CSSPropertyID::FontVariantLigatures:
    case CSSPropertyID::FontVariantNumeric:
    case CSSPropertyID::FontVariantEmoji:
    case CSSPropertyID::FontVariantPosition:
    case CSSPropertyID::FontWeight:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyID::FontOpticalSizing:
    case CSSPropertyID::FontVariationSettings:
#endif
    case CSSPropertyID::LetterSpacing:
    case CSSPropertyID::LineHeight:
    case CSSPropertyID::MarginBottom:
    case CSSPropertyID::MarginLeft:
    case CSSPropertyID::MarginRight:
    case CSSPropertyID::MarginTop:
    case CSSPropertyID::MarginBlockEnd:
    case CSSPropertyID::MarginBlockStart:
    case CSSPropertyID::MarginInlineEnd:
    case CSSPropertyID::MarginInlineStart:
    case CSSPropertyID::Opacity:
    case CSSPropertyID::PaddingTop:
    case CSSPropertyID::PaddingRight:
    case CSSPropertyID::PaddingBottom:
    case CSSPropertyID::PaddingLeft:
    case CSSPropertyID::TextDecorationStyle:
    case CSSPropertyID::TextDecorationColor:
    case CSSPropertyID::TextDecorationLine:
    case CSSPropertyID::TextDecorationSkipInk:
    case CSSPropertyID::TextDecorationThickness:
    case CSSPropertyID::TextJustify:
    case CSSPropertyID::TextShadow:
    case CSSPropertyID::TextTransform:
    case CSSPropertyID::TextUnderlinePosition:
    case CSSPropertyID::TextUnderlineOffset:
    case CSSPropertyID::VerticalAlign:
    case CSSPropertyID::Visibility:
    case CSSPropertyID::WebkitBorderHorizontalSpacing:
    case CSSPropertyID::WebkitBorderVerticalSpacing:
    case CSSPropertyID::WebkitFontSmoothing:
    case CSSPropertyID::WebkitInitialLetter:
    case CSSPropertyID::WebkitLineBoxContain:
    case CSSPropertyID::WordSpacing:
    case CSSPropertyID::AnimationComposition:
    case CSSPropertyID::AnimationDelay:
    case CSSPropertyID::AnimationDirection:
    case CSSPropertyID::AnimationDuration:
    case CSSPropertyID::AnimationFillMode:
    case CSSPropertyID::AnimationIterationCount:
    case CSSPropertyID::AnimationName:
    case CSSPropertyID::AnimationPlayState:
    case CSSPropertyID::AnimationRangeEnd:
    case CSSPropertyID::AnimationRangeStart:
    case CSSPropertyID::AnimationTimeline:
    case CSSPropertyID::AnimationTimingFunction:
        return true;
    default:
        break;
    }
    return false;
}

// https://drafts.csswg.org/css-pseudo/#first-line-styling
bool isValidFirstLineStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyID::BackgroundAttachment:
    case CSSPropertyID::BackgroundBlendMode:
    case CSSPropertyID::BackgroundClip:
    case CSSPropertyID::BackgroundColor:
    case CSSPropertyID::BackgroundImage:
    case CSSPropertyID::BackgroundOrigin:
    case CSSPropertyID::BackgroundPosition:
    case CSSPropertyID::BackgroundPositionX:
    case CSSPropertyID::BackgroundPositionY:
    case CSSPropertyID::BackgroundRepeat:
    case CSSPropertyID::BackgroundSize:
    case CSSPropertyID::BoxShadow:
    case CSSPropertyID::Color:
    case CSSPropertyID::Custom:
    case CSSPropertyID::FontFamily:
    case CSSPropertyID::FontFeatureSettings:
    case CSSPropertyID::FontKerning:
    case CSSPropertyID::FontSize:
    case CSSPropertyID::FontSizeAdjust:
    case CSSPropertyID::FontPalette:
    case CSSPropertyID::FontStyle:
    case CSSPropertyID::FontSynthesis:
    case CSSPropertyID::FontSynthesisWeight:
    case CSSPropertyID::FontSynthesisStyle:
    case CSSPropertyID::FontSynthesisSmallCaps:
    case CSSPropertyID::FontVariantAlternates:
    case CSSPropertyID::FontVariantCaps:
    case CSSPropertyID::FontVariantEastAsian:
    case CSSPropertyID::FontVariantLigatures:
    case CSSPropertyID::FontVariantNumeric:
    case CSSPropertyID::FontVariantEmoji:
    case CSSPropertyID::FontVariantPosition:
    case CSSPropertyID::FontWeight:
#if ENABLE(VARIATION_FONTS)
    case CSSPropertyID::FontOpticalSizing:
    case CSSPropertyID::FontVariationSettings:
#endif
    case CSSPropertyID::LetterSpacing:
    case CSSPropertyID::LineHeight:
    case CSSPropertyID::Opacity:
    case CSSPropertyID::RubyPosition:
    case CSSPropertyID::TextDecorationStyle:
    case CSSPropertyID::TextDecorationColor:
    case CSSPropertyID::TextDecorationLine:
    case CSSPropertyID::TextDecorationSkipInk:
    case CSSPropertyID::TextDecorationThickness:
    case CSSPropertyID::TextJustify:
    case CSSPropertyID::TextShadow:
    case CSSPropertyID::TextTransform:
    case CSSPropertyID::TextUnderlinePosition:
    case CSSPropertyID::TextUnderlineOffset:
    case CSSPropertyID::VerticalAlign:
    case CSSPropertyID::Visibility:
    case CSSPropertyID::WebkitFontSmoothing:
    case CSSPropertyID::WordSpacing:
    case CSSPropertyID::AnimationComposition:
    case CSSPropertyID::AnimationDelay:
    case CSSPropertyID::AnimationDirection:
    case CSSPropertyID::AnimationDuration:
    case CSSPropertyID::AnimationFillMode:
    case CSSPropertyID::AnimationIterationCount:
    case CSSPropertyID::AnimationName:
    case CSSPropertyID::AnimationPlayState:
    case CSSPropertyID::AnimationRangeEnd:
    case CSSPropertyID::AnimationRangeStart:
    case CSSPropertyID::AnimationTimeline:
    case CSSPropertyID::AnimationTimingFunction:
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
    case CSSPropertyID::Color:
    case CSSPropertyID::Custom:
    case CSSPropertyID::Font:
    case CSSPropertyID::FontFamily:
    case CSSPropertyID::FontSize:
    case CSSPropertyID::FontStyle:
    case CSSPropertyID::FontVariantCaps:
    case CSSPropertyID::FontWeight:
    case CSSPropertyID::LineHeight:
    case CSSPropertyID::Opacity:
    case CSSPropertyID::Outline:
    case CSSPropertyID::OutlineColor:
    case CSSPropertyID::OutlineOffset:
    case CSSPropertyID::OutlineStyle:
    case CSSPropertyID::OutlineWidth:
    case CSSPropertyID::Visibility:
    case CSSPropertyID::WhiteSpace:
    case CSSPropertyID::WhiteSpaceCollapse:
    case CSSPropertyID::TextCombineUpright:
    case CSSPropertyID::TextDecorationColor:
    case CSSPropertyID::TextDecorationInset:
    case CSSPropertyID::TextDecorationLine:
    case CSSPropertyID::TextDecorationStyle:
    case CSSPropertyID::TextDecorationThickness:
    case CSSPropertyID::TextShadow:
    case CSSPropertyID::TextWrapMode:
    case CSSPropertyID::TextWrapStyle:
    case CSSPropertyID::BorderStyle:
    case CSSPropertyID::PaintOrder:
    case CSSPropertyID::StrokeLinejoin:
    case CSSPropertyID::StrokeLinecap:
    case CSSPropertyID::StrokeColor:
    case CSSPropertyID::StrokeWidth:
        return true;
    default:
        break;
    }
    return false;
}
#endif

#if ENABLE(VIDEO)
bool isValidCueSelectorStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyID::Background:
    case CSSPropertyID::BackgroundAttachment:
    case CSSPropertyID::BackgroundClip:
    case CSSPropertyID::BackgroundColor:
    case CSSPropertyID::BackgroundImage:
    case CSSPropertyID::BackgroundOrigin:
    case CSSPropertyID::BackgroundPosition:
    case CSSPropertyID::BackgroundPositionX:
    case CSSPropertyID::BackgroundPositionY:
    case CSSPropertyID::BackgroundRepeat:
    case CSSPropertyID::BackgroundSize:
    case CSSPropertyID::Color:
    case CSSPropertyID::Custom:
    case CSSPropertyID::Font:
    case CSSPropertyID::FontFamily:
    case CSSPropertyID::FontSize:
    case CSSPropertyID::FontStyle:
    case CSSPropertyID::FontVariantCaps:
    case CSSPropertyID::FontWeight:
    case CSSPropertyID::LineHeight:
    case CSSPropertyID::Opacity:
    case CSSPropertyID::Outline:
    case CSSPropertyID::OutlineColor:
    case CSSPropertyID::OutlineOffset:
    case CSSPropertyID::OutlineStyle:
    case CSSPropertyID::OutlineWidth:
    case CSSPropertyID::Visibility:
    case CSSPropertyID::WhiteSpace:
    case CSSPropertyID::WhiteSpaceCollapse:
    case CSSPropertyID::TextCombineUpright:
    case CSSPropertyID::TextDecorationColor:
    case CSSPropertyID::TextDecorationInset:
    case CSSPropertyID::TextDecorationLine:
    case CSSPropertyID::TextDecorationStyle:
    case CSSPropertyID::TextDecorationThickness:
    case CSSPropertyID::TextShadow:
    case CSSPropertyID::TextWrapMode:
    case CSSPropertyID::TextWrapStyle:
    case CSSPropertyID::BorderStyle:
    case CSSPropertyID::PaintOrder:
    case CSSPropertyID::StrokeLinejoin:
    case CSSPropertyID::StrokeLinecap:
    case CSSPropertyID::StrokeColor:
    case CSSPropertyID::StrokeWidth:
        return true;
    default:
        break;
    }
    return false;
}

bool isValidCueBackgroundStyleProperty(CSSPropertyID id)
{
    switch (id) {
    case CSSPropertyID::Background:
    case CSSPropertyID::BackgroundAttachment:
    case CSSPropertyID::BackgroundClip:
    case CSSPropertyID::BackgroundColor:
    case CSSPropertyID::BackgroundImage:
    case CSSPropertyID::BackgroundOrigin:
    case CSSPropertyID::BackgroundPosition:
    case CSSPropertyID::BackgroundPositionX:
    case CSSPropertyID::BackgroundPositionY:
    case CSSPropertyID::BackgroundRepeat:
    case CSSPropertyID::BackgroundSize:
        return true;
    default:
        break;
    }
    return false;
}
#endif

}
}
