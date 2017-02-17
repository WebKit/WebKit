/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "StyleRareInheritedData.h"

#include "CursorList.h"
#include "DataRef.h"
#include "QuotesData.h"
#include "RenderStyle.h"
#include "RenderStyleConstants.h"
#include "ShadowData.h"
#include "StyleCustomPropertyData.h"
#include "StyleImage.h"
#include <wtf/PointerComparison.h>

namespace WebCore {

struct GreaterThanOrSameSizeAsStyleRareInheritedData : public RefCounted<GreaterThanOrSameSizeAsStyleRareInheritedData> {
    void* styleImage;
    Color firstColor;
    float firstFloat;
    Color colors[5];
    void* ownPtrs[1];
    AtomicString atomicStrings[5];
    void* refPtrs[2];
    Length lengths[2];
    float secondFloat;
    unsigned bitfields[4];
    short pagedMediaShorts[2];
    unsigned unsigneds[1];
    short hyphenationShorts[3];

#if PLATFORM(IOS)
    Color compositionColor;
#endif
#if ENABLE(TEXT_AUTOSIZING)
    TextSizeAdjustment textSizeAdjust;
#endif

#if ENABLE(CSS_IMAGE_RESOLUTION)
    float imageResolutionFloats;
#endif

#if ENABLE(TOUCH_EVENTS)
    Color tapHighlightColor;
#endif

    void* customPropertyDataRefs[1];
};

COMPILE_ASSERT(sizeof(StyleRareInheritedData) <= sizeof(GreaterThanOrSameSizeAsStyleRareInheritedData), StyleRareInheritedData_should_bit_pack);

StyleRareInheritedData::StyleRareInheritedData()
    : listStyleImage(RenderStyle::initialListStyleImage())
    , textStrokeWidth(RenderStyle::initialTextStrokeWidth())
    , indent(RenderStyle::initialTextIndent())
    , effectiveZoom(RenderStyle::initialZoom())
    , customProperties(StyleCustomPropertyData::create())
    , widows(RenderStyle::initialWidows())
    , orphans(RenderStyle::initialOrphans())
    , hasAutoWidows(true)
    , hasAutoOrphans(true)
    , textSecurity(RenderStyle::initialTextSecurity())
    , userModify(READ_ONLY)
    , wordBreak(RenderStyle::initialWordBreak())
    , overflowWrap(RenderStyle::initialOverflowWrap())
    , nbspMode(NBNORMAL)
    , lineBreak(LineBreakAuto)
    , userSelect(RenderStyle::initialUserSelect())
    , speak(SpeakNormal)
    , hyphens(HyphensManual)
    , textEmphasisFill(TextEmphasisFillFilled)
    , textEmphasisMark(TextEmphasisMarkNone)
    , textEmphasisPosition(TextEmphasisPositionOver | TextEmphasisPositionRight)
    , textOrientation(static_cast<unsigned>(TextOrientation::Mixed))
#if ENABLE(CSS3_TEXT)
    , textIndentLine(RenderStyle::initialTextIndentLine())
    , textIndentType(RenderStyle::initialTextIndentType())
#endif
    , lineBoxContain(RenderStyle::initialLineBoxContain())
#if ENABLE(CSS_IMAGE_ORIENTATION)
    , imageOrientation(RenderStyle::initialImageOrientation())
#endif
    , imageRendering(RenderStyle::initialImageRendering())
    , lineSnap(RenderStyle::initialLineSnap())
    , lineAlign(RenderStyle::initialLineAlign())
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    , useTouchOverflowScrolling(RenderStyle::initialUseTouchOverflowScrolling())
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , imageResolutionSource(RenderStyle::initialImageResolutionSource())
    , imageResolutionSnap(RenderStyle::initialImageResolutionSnap())
#endif
#if ENABLE(CSS3_TEXT)
    , textAlignLast(RenderStyle::initialTextAlignLast())
    , textJustify(RenderStyle::initialTextJustify())
#endif
    , textDecorationSkip(RenderStyle::initialTextDecorationSkip())
    , textUnderlinePosition(RenderStyle::initialTextUnderlinePosition())
    , rubyPosition(RenderStyle::initialRubyPosition())
    , textZoom(RenderStyle::initialTextZoom())
#if PLATFORM(IOS)
    , touchCalloutEnabled(RenderStyle::initialTouchCalloutEnabled())
#endif
#if ENABLE(CSS_TRAILING_WORD)
    , trailingWord(static_cast<unsigned>(RenderStyle::initialTrailingWord()))
#endif
    , hangingPunctuation(RenderStyle::initialHangingPunctuation())
    , paintOrder(RenderStyle::initialPaintOrder())
    , capStyle(RenderStyle::initialCapStyle())
    , joinStyle(RenderStyle::initialJoinStyle())
    , strokeWidth(RenderStyle::initialOneLength())
    , hyphenationLimitBefore(-1)
    , hyphenationLimitAfter(-1)
    , hyphenationLimitLines(-1)
    , lineGrid(RenderStyle::initialLineGrid())
    , tabSize(RenderStyle::initialTabSize())
#if ENABLE(TEXT_AUTOSIZING)
    , textSizeAdjust(RenderStyle::initialTextSizeAdjust())
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , imageResolution(RenderStyle::initialImageResolution())
#endif
#if ENABLE(TOUCH_EVENTS)
    , tapHighlightColor(RenderStyle::initialTapHighlightColor())
#endif
{
}

inline StyleRareInheritedData::StyleRareInheritedData(const StyleRareInheritedData& o)
    : RefCounted<StyleRareInheritedData>()
    , listStyleImage(o.listStyleImage)
    , textStrokeColor(o.textStrokeColor)
    , textStrokeWidth(o.textStrokeWidth)
    , textFillColor(o.textFillColor)
    , textEmphasisColor(o.textEmphasisColor)
    , visitedLinkTextStrokeColor(o.visitedLinkTextStrokeColor)
    , visitedLinkTextFillColor(o.visitedLinkTextFillColor)
    , visitedLinkTextEmphasisColor(o.visitedLinkTextEmphasisColor)
    , textShadow(o.textShadow ? std::make_unique<ShadowData>(*o.textShadow) : nullptr)
    , cursorData(o.cursorData)
    , indent(o.indent)
    , effectiveZoom(o.effectiveZoom)
    , customProperties(o.customProperties)
    , widows(o.widows)
    , orphans(o.orphans)
    , hasAutoWidows(o.hasAutoWidows)
    , hasAutoOrphans(o.hasAutoOrphans)
    , textSecurity(o.textSecurity)
    , userModify(o.userModify)
    , wordBreak(o.wordBreak)
    , overflowWrap(o.overflowWrap)
    , nbspMode(o.nbspMode)
    , lineBreak(o.lineBreak)
    , userSelect(o.userSelect)
    , speak(o.speak)
    , hyphens(o.hyphens)
    , textEmphasisFill(o.textEmphasisFill)
    , textEmphasisMark(o.textEmphasisMark)
    , textEmphasisPosition(o.textEmphasisPosition)
    , textOrientation(o.textOrientation)
#if ENABLE(CSS3_TEXT)
    , textIndentLine(o.textIndentLine)
    , textIndentType(o.textIndentType)
#endif
    , lineBoxContain(o.lineBoxContain)
#if ENABLE(CSS_IMAGE_ORIENTATION)
    , imageOrientation(o.imageOrientation)
#endif
    , imageRendering(o.imageRendering)
    , lineSnap(o.lineSnap)
    , lineAlign(o.lineAlign)
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    , useTouchOverflowScrolling(o.useTouchOverflowScrolling)
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , imageResolutionSource(o.imageResolutionSource)
    , imageResolutionSnap(o.imageResolutionSnap)
#endif
#if ENABLE(CSS3_TEXT)
    , textAlignLast(o.textAlignLast)
    , textJustify(o.textJustify)
#endif
    , textDecorationSkip(o.textDecorationSkip)
    , textUnderlinePosition(o.textUnderlinePosition)
    , rubyPosition(o.rubyPosition)
    , textZoom(o.textZoom)
#if PLATFORM(IOS)
    , touchCalloutEnabled(o.touchCalloutEnabled)
#endif
#if ENABLE(CSS_TRAILING_WORD)
    , trailingWord(o.trailingWord)
#endif
    , hangingPunctuation(o.hangingPunctuation)
    , paintOrder(o.paintOrder)
    , capStyle(o.capStyle)
    , joinStyle(o.joinStyle)
    , strokeWidth(o.strokeWidth)
    , hyphenationString(o.hyphenationString)
    , hyphenationLimitBefore(o.hyphenationLimitBefore)
    , hyphenationLimitAfter(o.hyphenationLimitAfter)
    , hyphenationLimitLines(o.hyphenationLimitLines)
    , textEmphasisCustomMark(o.textEmphasisCustomMark)
    , lineGrid(o.lineGrid)
    , tabSize(o.tabSize)
#if ENABLE(TEXT_AUTOSIZING)
    , textSizeAdjust(o.textSizeAdjust)
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , imageResolution(o.imageResolution)
#endif
#if ENABLE(TOUCH_EVENTS)
    , tapHighlightColor(o.tapHighlightColor)
#endif
{
}

Ref<StyleRareInheritedData> StyleRareInheritedData::copy() const
{
    return adoptRef(*new StyleRareInheritedData(*this));
}

StyleRareInheritedData::~StyleRareInheritedData()
{
}

bool StyleRareInheritedData::operator==(const StyleRareInheritedData& o) const
{
    return textStrokeColor == o.textStrokeColor
        && textStrokeWidth == o.textStrokeWidth
        && textFillColor == o.textFillColor
        && textEmphasisColor == o.textEmphasisColor
        && visitedLinkTextStrokeColor == o.visitedLinkTextStrokeColor
        && visitedLinkTextFillColor == o.visitedLinkTextFillColor
        && visitedLinkTextEmphasisColor == o.visitedLinkTextEmphasisColor
#if ENABLE(TOUCH_EVENTS)
        && tapHighlightColor == o.tapHighlightColor
#endif
        && arePointingToEqualData(textShadow, o.textShadow)
        && arePointingToEqualData(cursorData, o.cursorData)
        && indent == o.indent
        && effectiveZoom == o.effectiveZoom
        && widows == o.widows
        && orphans == o.orphans
        && hasAutoWidows == o.hasAutoWidows
        && hasAutoOrphans == o.hasAutoOrphans
        && textSecurity == o.textSecurity
        && userModify == o.userModify
        && wordBreak == o.wordBreak
        && overflowWrap == o.overflowWrap
        && nbspMode == o.nbspMode
        && lineBreak == o.lineBreak
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
        && useTouchOverflowScrolling == o.useTouchOverflowScrolling
#endif
#if ENABLE(TEXT_AUTOSIZING)
        && textSizeAdjust == o.textSizeAdjust
#endif
        && userSelect == o.userSelect
        && speak == o.speak
        && hyphens == o.hyphens
        && hyphenationLimitBefore == o.hyphenationLimitBefore
        && hyphenationLimitAfter == o.hyphenationLimitAfter
        && hyphenationLimitLines == o.hyphenationLimitLines
        && textEmphasisFill == o.textEmphasisFill
        && textEmphasisMark == o.textEmphasisMark
        && textEmphasisPosition == o.textEmphasisPosition
        && textOrientation == o.textOrientation
#if ENABLE(CSS3_TEXT)
        && textIndentLine == o.textIndentLine
        && textIndentType == o.textIndentType
#endif
        && lineBoxContain == o.lineBoxContain
#if PLATFORM(IOS)
        && touchCalloutEnabled == o.touchCalloutEnabled
#endif
        && hyphenationString == o.hyphenationString
        && textEmphasisCustomMark == o.textEmphasisCustomMark
        && arePointingToEqualData(quotes, o.quotes)
        && tabSize == o.tabSize
        && lineGrid == o.lineGrid
#if ENABLE(CSS_IMAGE_ORIENTATION)
        && imageOrientation == o.imageOrientation
#endif
        && imageRendering == o.imageRendering
#if ENABLE(CSS_IMAGE_RESOLUTION)
        && imageResolutionSource == o.imageResolutionSource
        && imageResolutionSnap == o.imageResolutionSnap
        && imageResolution == o.imageResolution
#endif
#if ENABLE(CSS3_TEXT)
        && textAlignLast == o.textAlignLast
        && textJustify == o.textJustify
#endif // CSS3_TEXT
        && textDecorationSkip == o.textDecorationSkip
        && textUnderlinePosition == o.textUnderlinePosition
        && rubyPosition == o.rubyPosition
        && textZoom == o.textZoom
        && lineSnap == o.lineSnap
        && lineAlign == o.lineAlign
#if ENABLE(CSS_TRAILING_WORD)
        && trailingWord == o.trailingWord
#endif
        && hangingPunctuation == o.hangingPunctuation
        && paintOrder == o.paintOrder
        && capStyle == o.capStyle
        && joinStyle == o.joinStyle
        && strokeWidth == o.strokeWidth
        && customProperties == o.customProperties
        && arePointingToEqualData(listStyleImage, o.listStyleImage);
}

} // namespace WebCore
