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
    unsigned m_bitfields[4];
    short pagedMediaShorts[2];
    unsigned unsigneds[1];
    short hyphenationShorts[3];

#if PLATFORM(IOS)
    Color compositionColor;
#endif
#if ENABLE(IOS_TEXT_AUTOSIZING)
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
    , m_effectiveZoom(RenderStyle::initialZoom())
    , m_customProperties(StyleCustomPropertyData::create())
    , widows(RenderStyle::initialWidows())
    , orphans(RenderStyle::initialOrphans())
    , m_hasAutoWidows(true)
    , m_hasAutoOrphans(true)
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
    , m_textOrientation(static_cast<unsigned>(TextOrientation::Mixed))
#if ENABLE(CSS3_TEXT)
    , m_textIndentLine(RenderStyle::initialTextIndentLine())
    , m_textIndentType(RenderStyle::initialTextIndentType())
#endif
    , m_lineBoxContain(RenderStyle::initialLineBoxContain())
#if ENABLE(CSS_IMAGE_ORIENTATION)
    , m_imageOrientation(RenderStyle::initialImageOrientation())
#endif
    , m_imageRendering(RenderStyle::initialImageRendering())
    , m_lineSnap(RenderStyle::initialLineSnap())
    , m_lineAlign(RenderStyle::initialLineAlign())
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    , useTouchOverflowScrolling(RenderStyle::initialUseTouchOverflowScrolling())
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , m_imageResolutionSource(RenderStyle::initialImageResolutionSource())
    , m_imageResolutionSnap(RenderStyle::initialImageResolutionSnap())
#endif
#if ENABLE(CSS3_TEXT)
    , m_textAlignLast(RenderStyle::initialTextAlignLast())
    , m_textJustify(RenderStyle::initialTextJustify())
#endif // CSS3_TEXT
    , m_textDecorationSkip(RenderStyle::initialTextDecorationSkip())
    , m_textUnderlinePosition(RenderStyle::initialTextUnderlinePosition())
    , m_rubyPosition(RenderStyle::initialRubyPosition())
    , m_textZoom(RenderStyle::initialTextZoom())
#if PLATFORM(IOS)
    , touchCalloutEnabled(RenderStyle::initialTouchCalloutEnabled())
#endif
#if ENABLE(CSS_TRAILING_WORD)
    , trailingWord(static_cast<unsigned>(RenderStyle::initialTrailingWord()))
#endif
    , m_hangingPunctuation(RenderStyle::initialHangingPunctuation())
    , hyphenationLimitBefore(-1)
    , hyphenationLimitAfter(-1)
    , hyphenationLimitLines(-1)
    , m_lineGrid(RenderStyle::initialLineGrid())
    , m_tabSize(RenderStyle::initialTabSize())
#if ENABLE(IOS_TEXT_AUTOSIZING)
    , textSizeAdjust(RenderStyle::initialTextSizeAdjust())
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , m_imageResolution(RenderStyle::initialImageResolution())
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
    , m_effectiveZoom(o.m_effectiveZoom)
    , m_customProperties(o.m_customProperties)
    , widows(o.widows)
    , orphans(o.orphans)
    , m_hasAutoWidows(o.m_hasAutoWidows)
    , m_hasAutoOrphans(o.m_hasAutoOrphans)
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
    , m_textOrientation(o.m_textOrientation)
#if ENABLE(CSS3_TEXT)
    , m_textIndentLine(o.m_textIndentLine)
    , m_textIndentType(o.m_textIndentType)
#endif
    , m_lineBoxContain(o.m_lineBoxContain)
#if ENABLE(CSS_IMAGE_ORIENTATION)
    , m_imageOrientation(o.m_imageOrientation)
#endif
    , m_imageRendering(o.m_imageRendering)
    , m_lineSnap(o.m_lineSnap)
    , m_lineAlign(o.m_lineAlign)
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
    , useTouchOverflowScrolling(o.useTouchOverflowScrolling)
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , m_imageResolutionSource(o.m_imageResolutionSource)
    , m_imageResolutionSnap(o.m_imageResolutionSnap)
#endif
#if ENABLE(CSS3_TEXT)
    , m_textAlignLast(o.m_textAlignLast)
    , m_textJustify(o.m_textJustify)
#endif // CSS3_TEXT
    , m_textDecorationSkip(o.m_textDecorationSkip)
    , m_textUnderlinePosition(o.m_textUnderlinePosition)
    , m_rubyPosition(o.m_rubyPosition)
    , m_textZoom(o.m_textZoom)
#if PLATFORM(IOS)
    , touchCalloutEnabled(o.touchCalloutEnabled)
#endif
#if ENABLE(CSS_TRAILING_WORD)
    , trailingWord(o.trailingWord)
#endif
    , m_hangingPunctuation(o.m_hangingPunctuation)
    , hyphenationString(o.hyphenationString)
    , hyphenationLimitBefore(o.hyphenationLimitBefore)
    , hyphenationLimitAfter(o.hyphenationLimitAfter)
    , hyphenationLimitLines(o.hyphenationLimitLines)
    , textEmphasisCustomMark(o.textEmphasisCustomMark)
    , m_lineGrid(o.m_lineGrid)
    , m_tabSize(o.m_tabSize)
#if ENABLE(IOS_TEXT_AUTOSIZING)
    , textSizeAdjust(o.textSizeAdjust)
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    , m_imageResolution(o.m_imageResolution)
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
        && m_effectiveZoom == o.m_effectiveZoom
        && widows == o.widows
        && orphans == o.orphans
        && m_hasAutoWidows == o.m_hasAutoWidows
        && m_hasAutoOrphans == o.m_hasAutoOrphans
        && textSecurity == o.textSecurity
        && userModify == o.userModify
        && wordBreak == o.wordBreak
        && overflowWrap == o.overflowWrap
        && nbspMode == o.nbspMode
        && lineBreak == o.lineBreak
#if ENABLE(ACCELERATED_OVERFLOW_SCROLLING)
        && useTouchOverflowScrolling == o.useTouchOverflowScrolling
#endif
#if ENABLE(IOS_TEXT_AUTOSIZING)
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
        && m_textOrientation == o.m_textOrientation
#if ENABLE(CSS3_TEXT)
        && m_textIndentLine == o.m_textIndentLine
        && m_textIndentType == o.m_textIndentType
#endif
        && m_lineBoxContain == o.m_lineBoxContain
#if PLATFORM(IOS)
        && touchCalloutEnabled == o.touchCalloutEnabled
#endif
        && hyphenationString == o.hyphenationString
        && textEmphasisCustomMark == o.textEmphasisCustomMark
        && arePointingToEqualData(quotes, o.quotes)
        && m_tabSize == o.m_tabSize
        && m_lineGrid == o.m_lineGrid
#if ENABLE(CSS_IMAGE_ORIENTATION)
        && m_imageOrientation == o.m_imageOrientation
#endif
        && m_imageRendering == o.m_imageRendering
#if ENABLE(CSS_IMAGE_RESOLUTION)
        && m_imageResolutionSource == o.m_imageResolutionSource
        && m_imageResolutionSnap == o.m_imageResolutionSnap
        && m_imageResolution == o.m_imageResolution
#endif
#if ENABLE(CSS3_TEXT)
        && m_textAlignLast == o.m_textAlignLast
        && m_textJustify == o.m_textJustify
#endif // CSS3_TEXT
        && m_textDecorationSkip == o.m_textDecorationSkip
        && m_textUnderlinePosition == o.m_textUnderlinePosition
        && m_rubyPosition == o.m_rubyPosition
        && m_textZoom == o.m_textZoom
        && m_lineSnap == o.m_lineSnap
        && m_lineAlign == o.m_lineAlign
#if ENABLE(CSS_TRAILING_WORD)
        && trailingWord == o.trailingWord
#endif
        && m_hangingPunctuation == o.m_hangingPunctuation
        && m_customProperties == o.m_customProperties
        && arePointingToEqualData(listStyleImage, o.listStyleImage);
}

} // namespace WebCore
