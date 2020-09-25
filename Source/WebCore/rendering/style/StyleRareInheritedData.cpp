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
#include "QuotesData.h"
#include "RenderStyle.h"
#include "RenderStyleConstants.h"
#include "ShadowData.h"
#include "StyleColorScheme.h"
#include "StyleCustomPropertyData.h"
#include "StyleFilterData.h"
#include "StyleImage.h"
#include <wtf/DataRef.h>
#include <wtf/PointerComparison.h>

namespace WebCore {

struct GreaterThanOrSameSizeAsStyleRareInheritedData : public RefCounted<GreaterThanOrSameSizeAsStyleRareInheritedData> {
    void* styleImage;
    Color firstColor;
    float firstFloat;
    Color colors[9];
    void* ownPtrs[1];
    AtomString atomStrings[5];
    void* refPtrs[3];
    Length lengths[2];
    float secondFloat;
    TextUnderlineOffset offset;
    TextDecorationThickness thickness;
    void* customPropertyDataRefs[1];
    unsigned bitfields[6];
    short pagedMediaShorts[2];
    TabSize tabSize;
    short hyphenationShorts[3];

#if ENABLE(TEXT_AUTOSIZING)
    TextSizeAdjustment textSizeAdjust;
#endif

#if ENABLE(CSS_IMAGE_RESOLUTION)
    float imageResolutionFloats;
#endif

#if ENABLE(TOUCH_EVENTS)
    Color tapHighlightColor;
#endif

#if ENABLE(DARK_MODE_CSS)
    StyleColorScheme colorScheme;
#endif
};

COMPILE_ASSERT(sizeof(StyleRareInheritedData) <= sizeof(GreaterThanOrSameSizeAsStyleRareInheritedData), StyleRareInheritedData_should_bit_pack);

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareInheritedData);

StyleRareInheritedData::StyleRareInheritedData()
    : listStyleImage(RenderStyle::initialListStyleImage())
    , textStrokeWidth(RenderStyle::initialTextStrokeWidth())
    , indent(RenderStyle::initialTextIndent())
    , effectiveZoom(RenderStyle::initialZoom())
    , textUnderlineOffset(RenderStyle::initialTextUnderlineOffset())
    , textDecorationThickness(RenderStyle::initialTextDecorationThickness())
    , customProperties(StyleCustomPropertyData::create())
    , widows(RenderStyle::initialWidows())
    , orphans(RenderStyle::initialOrphans())
    , hasAutoWidows(true)
    , hasAutoOrphans(true)
    , textSecurity(static_cast<unsigned>(RenderStyle::initialTextSecurity()))
    , userModify(static_cast<unsigned>(UserModify::ReadOnly))
    , wordBreak(static_cast<unsigned>(RenderStyle::initialWordBreak()))
    , overflowWrap(static_cast<unsigned>(RenderStyle::initialOverflowWrap()))
    , nbspMode(static_cast<unsigned>(NBSPMode::Normal))
    , lineBreak(static_cast<unsigned>(LineBreak::Auto))
    , userSelect(static_cast<unsigned>(RenderStyle::initialUserSelect()))
    , speakAs(RenderStyle::initialSpeakAs().toRaw())
    , hyphens(static_cast<unsigned>(Hyphens::Manual))
    , textEmphasisFill(static_cast<unsigned>(TextEmphasisFill::Filled))
    , textEmphasisMark(static_cast<unsigned>(TextEmphasisMark::None))
    , textEmphasisPosition(static_cast<unsigned>(RenderStyle::initialTextEmphasisPosition().toRaw()))
    , textOrientation(static_cast<unsigned>(TextOrientation::Mixed))
#if ENABLE(CSS3_TEXT)
    , textIndentLine(RenderStyle::initialTextIndentLine())
    , textIndentType(RenderStyle::initialTextIndentType())
#endif
    , lineBoxContain(static_cast<unsigned>(RenderStyle::initialLineBoxContain().toRaw()))
    , imageOrientation(RenderStyle::initialImageOrientation())
    , imageRendering(static_cast<unsigned>(RenderStyle::initialImageRendering()))
    , lineSnap(static_cast<unsigned>(RenderStyle::initialLineSnap()))
    , lineAlign(static_cast<unsigned>(RenderStyle::initialLineAlign()))
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
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
    , textDecorationSkip(RenderStyle::initialTextDecorationSkip().toRaw())
    , textUnderlinePosition(static_cast<unsigned>(RenderStyle::initialTextUnderlinePosition()))
    , rubyPosition(static_cast<unsigned>(RenderStyle::initialRubyPosition()))
    , textZoom(static_cast<unsigned>(RenderStyle::initialTextZoom()))
#if PLATFORM(IOS_FAMILY)
    , touchCalloutEnabled(RenderStyle::initialTouchCalloutEnabled())
#endif
    , hangingPunctuation(RenderStyle::initialHangingPunctuation().toRaw())
    , paintOrder(static_cast<unsigned>(RenderStyle::initialPaintOrder()))
    , capStyle(RenderStyle::initialCapStyle())
    , joinStyle(RenderStyle::initialJoinStyle())
    , hasSetStrokeWidth(false)
    , hasSetStrokeColor(false)
    , mathStyle(static_cast<unsigned>(RenderStyle::initialMathStyle()))
    , effectiveTouchActions(RenderStyle::initialTouchActions())
    , strokeWidth(RenderStyle::initialStrokeWidth())
    , strokeColor(RenderStyle::initialStrokeColor())
    , miterLimit(RenderStyle::initialStrokeMiterLimit())
    , hyphenationLimitBefore(-1)
    , hyphenationLimitAfter(-1)
    , hyphenationLimitLines(-1)
    , appleColorFilter(StyleFilterData::create())
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
#if ENABLE(DARK_MODE_CSS)
    , colorScheme(RenderStyle::initialColorScheme())
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
    , caretColor(o.caretColor)
    , visitedLinkCaretColor(o.visitedLinkCaretColor)
    , textShadow(o.textShadow ? makeUnique<ShadowData>(*o.textShadow) : nullptr)
    , cursorData(o.cursorData)
    , indent(o.indent)
    , effectiveZoom(o.effectiveZoom)
    , textUnderlineOffset(o.textUnderlineOffset)
    , textDecorationThickness(o.textDecorationThickness)
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
    , speakAs(o.speakAs)
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
    , imageOrientation(o.imageOrientation)
    , imageRendering(o.imageRendering)
    , lineSnap(o.lineSnap)
    , lineAlign(o.lineAlign)
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
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
#if PLATFORM(IOS_FAMILY)
    , touchCalloutEnabled(o.touchCalloutEnabled)
#endif
    , hangingPunctuation(o.hangingPunctuation)
    , paintOrder(o.paintOrder)
    , capStyle(o.capStyle)
    , joinStyle(o.joinStyle)
    , hasSetStrokeWidth(o.hasSetStrokeWidth)
    , hasSetStrokeColor(o.hasSetStrokeColor)
    , mathStyle(o.mathStyle)
    , effectiveTouchActions(o.effectiveTouchActions)
    , eventListenerRegionTypes(o.eventListenerRegionTypes)
    , strokeWidth(o.strokeWidth)
    , strokeColor(o.strokeColor)
    , visitedLinkStrokeColor(o.visitedLinkStrokeColor)
    , miterLimit(o.miterLimit)
    , hyphenationString(o.hyphenationString)
    , hyphenationLimitBefore(o.hyphenationLimitBefore)
    , hyphenationLimitAfter(o.hyphenationLimitAfter)
    , hyphenationLimitLines(o.hyphenationLimitLines)
    , textEmphasisCustomMark(o.textEmphasisCustomMark)
    , appleColorFilter(o.appleColorFilter)
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
#if ENABLE(DARK_MODE_CSS)
    , colorScheme(o.colorScheme)
#endif
{
}

Ref<StyleRareInheritedData> StyleRareInheritedData::copy() const
{
    return adoptRef(*new StyleRareInheritedData(*this));
}

StyleRareInheritedData::~StyleRareInheritedData() = default;

bool StyleRareInheritedData::operator==(const StyleRareInheritedData& o) const
{
    return textStrokeColor == o.textStrokeColor
        && textStrokeWidth == o.textStrokeWidth
        && textFillColor == o.textFillColor
        && textEmphasisColor == o.textEmphasisColor
        && visitedLinkTextStrokeColor == o.visitedLinkTextStrokeColor
        && visitedLinkTextFillColor == o.visitedLinkTextFillColor
        && visitedLinkTextEmphasisColor == o.visitedLinkTextEmphasisColor
        && caretColor == o.caretColor
        && visitedLinkCaretColor == o.visitedLinkCaretColor
#if ENABLE(TOUCH_EVENTS)
        && tapHighlightColor == o.tapHighlightColor
#endif
        && arePointingToEqualData(textShadow, o.textShadow)
        && arePointingToEqualData(cursorData, o.cursorData)
        && indent == o.indent
        && effectiveZoom == o.effectiveZoom
        && textUnderlineOffset == o.textUnderlineOffset
        && textDecorationThickness == o.textDecorationThickness
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
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
        && useTouchOverflowScrolling == o.useTouchOverflowScrolling
#endif
#if ENABLE(TEXT_AUTOSIZING)
        && textSizeAdjust == o.textSizeAdjust
#endif
#if ENABLE(DARK_MODE_CSS)
        && colorScheme == o.colorScheme
#endif
        && userSelect == o.userSelect
        && speakAs == o.speakAs
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
#if PLATFORM(IOS_FAMILY)
        && touchCalloutEnabled == o.touchCalloutEnabled
#endif
        && hyphenationString == o.hyphenationString
        && textEmphasisCustomMark == o.textEmphasisCustomMark
        && arePointingToEqualData(quotes, o.quotes)
        && appleColorFilter == o.appleColorFilter
        && tabSize == o.tabSize
        && lineGrid == o.lineGrid
        && imageOrientation == o.imageOrientation
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
        && hangingPunctuation == o.hangingPunctuation
        && paintOrder == o.paintOrder
        && capStyle == o.capStyle
        && joinStyle == o.joinStyle
        && hasSetStrokeWidth == o.hasSetStrokeWidth
        && hasSetStrokeColor == o.hasSetStrokeColor
        && mathStyle == o.mathStyle
        && effectiveTouchActions == o.effectiveTouchActions
        && eventListenerRegionTypes == o.eventListenerRegionTypes
        && strokeWidth == o.strokeWidth
        && strokeColor == o.strokeColor
        && visitedLinkStrokeColor == o.visitedLinkStrokeColor
        && miterLimit == o.miterLimit
        && customProperties == o.customProperties
        && arePointingToEqualData(listStyleImage, o.listStyleImage);
}

bool StyleRareInheritedData::hasColorFilters() const
{
    return !appleColorFilter->operations.isEmpty();
}

} // namespace WebCore
