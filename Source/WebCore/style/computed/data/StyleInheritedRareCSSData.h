/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#pragma once

#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleAccentColor.h>
#include <WebCore/StyleBlockEllipsis.h>
#include <WebCore/StyleCaretColor.h>
#include <WebCore/StyleColor.h>
#include <WebCore/StyleCursor.h>
#include <WebCore/StyleDynamicRangeLimit.h>
#include <WebCore/StyleHangingPunctuation.h>
#include <WebCore/StyleHyphenateCharacter.h>
#include <WebCore/StyleHyphenateLimitEdge.h>
#include <WebCore/StyleHyphenateLimitLines.h>
#include <WebCore/StyleImageOrNone.h>
#include <WebCore/StyleImageOrientation.h>
#include <WebCore/StyleLineFitEdge.h>
#include <WebCore/StyleListStyleType.h>
#include <WebCore/StyleMathDepth.h>
#include <WebCore/StyleOrphans.h>
#include <WebCore/StyleQuotes.h>
#include <WebCore/StyleSVGPaintOrder.h>
#include <WebCore/StyleScrollbarColor.h>
#include <WebCore/StyleSpeakAs.h>
#include <WebCore/StyleStrokeMiterlimit.h>
#include <WebCore/StyleStrokeWidth.h>
#include <WebCore/StyleTabSize.h>
#include <WebCore/StyleTextAlignLast.h>
#include <WebCore/StyleTextBoxEdge.h>
#include <WebCore/StyleTextEmphasisPosition.h>
#include <WebCore/StyleTextEmphasisStyle.h>
#include <WebCore/StyleTextIndent.h>
#include <WebCore/StyleTextShadow.h>
#include <WebCore/StyleTextUnderlineOffset.h>
#include <WebCore/StyleTextUnderlinePosition.h>
#include <WebCore/StyleWebKitLineBoxContain.h>
#include <WebCore/StyleWebKitLineGrid.h>
#include <WebCore/StyleWebKitOverflowScrolling.h>
#include <WebCore/StyleWebKitTextStrokeWidth.h>
#include <WebCore/StyleWebKitTouchCallout.h>
#include <WebCore/StyleWidows.h>
#include <wtf/DataRef.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

#if ENABLE(TEXT_AUTOSIZING)
#include <WebCore/StyleTextSizeAdjust.h>
#endif

#if ENABLE(DARK_MODE_CSS)
#include <WebCore/StyleColorScheme.h>
#endif

namespace WebCore {
namespace Style {

class AppleColorFilterData;
class CustomPropertyData;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InheritedRareCSSData);
class InheritedRareCSSData : public RefCounted<InheritedRareCSSData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(InheritedRareCSSData, InheritedRareCSSData);
public:
    static Ref<InheritedRareCSSData> create() { return adoptRef(*new InheritedRareCSSData); }
    Ref<InheritedRareCSSData> copy() const;
    ~InheritedRareCSSData();

    bool operator==(const InheritedRareCSSData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const InheritedRareCSSData&) const;
#endif

    WebkitTextStrokeWidth textStrokeWidth;

    Color textStrokeColor;
    Color textFillColor;
    Color textEmphasisColor;
    Color visitedLinkTextStrokeColor;
    Color visitedLinkTextFillColor;
    Color visitedLinkTextEmphasisColor;
    CaretColor caretColor;
    CaretColor visitedLinkCaretColor;

    AccentColor accentColor;

    ScrollbarColor scrollbarColor;

    TextEmphasisStyle textEmphasisStyle;

    Quotes quotes;

    Color strokeColor;
    Color visitedLinkStrokeColor;

#if ENABLE(DARK_MODE_CSS)
    ColorScheme colorScheme;
#endif

    Cursor::Images cursorImages;

#if ENABLE(CSS_TAP_HIGHLIGHT_COLOR)
    Color tapHighlightColor;
#endif

    ListStyleType listStyleType;
    BlockEllipsis blockEllipsis;

    TextIndent textIndent;

    ImageOrNone listStyleImage;
    DynamicRangeLimit dynamicRangeLimit;
    TextShadows textShadow;
    HyphenateCharacter hyphenateCharacter;
    DataRef<CustomPropertyData> customProperties;
    StrokeWidth strokeWidth;
    TextUnderlineOffset textUnderlineOffset;
    DataRef<AppleColorFilterData> appleColorFilter;
    WebkitLineGrid lineGrid;
    TabSize tabSize;

    StrokeMiterlimit strokeMiterLimit;

#if ENABLE(TEXT_AUTOSIZING)
    TextSizeAdjust textSizeAdjust;
#endif

    MathDepth mathDepth;

    TextBoxEdge textBoxEdge;
    LineFitEdge lineFitEdge;

    Widows widows;
    Orphans orphans;
    HyphenateLimitEdge hyphenateLimitBefore;
    HyphenateLimitEdge hyphenateLimitAfter;
    HyphenateLimitLines hyphenateLimitLines;

    PREFERRED_TYPE(TextSecurity) unsigned textSecurity : 2;
    PREFERRED_TYPE(UserModify) unsigned userModify : 2;
    PREFERRED_TYPE(WordBreak) unsigned wordBreak : 3;
    PREFERRED_TYPE(OverflowWrap) unsigned overflowWrap : 2;
    PREFERRED_TYPE(NBSPMode) unsigned nbspMode : 1;
    PREFERRED_TYPE(LineBreak) unsigned lineBreak : 3;
    PREFERRED_TYPE(UserSelect) unsigned userSelect : 2;
    PREFERRED_TYPE(SpeakAs) unsigned speakAs : 4;
    PREFERRED_TYPE(Hyphens) unsigned hyphens : 2;
    PREFERRED_TYPE(TextCombine) unsigned textCombine : 1;
    PREFERRED_TYPE(TextEmphasisPosition) unsigned textEmphasisPosition : 4;
    PREFERRED_TYPE(TextUnderlinePosition) unsigned textUnderlinePosition : 4;
    PREFERRED_TYPE(WebkitLineBoxContain) unsigned lineBoxContain: 7;
    PREFERRED_TYPE(ImageOrientation) unsigned imageOrientation : 1;
    PREFERRED_TYPE(ImageRendering) unsigned imageRendering : 3;
    PREFERRED_TYPE(LineSnap) unsigned lineSnap : 2;
    PREFERRED_TYPE(LineAlign) unsigned lineAlign : 1;
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    PREFERRED_TYPE(WebkitOverflowScrolling) unsigned overflowScrolling: 1;
#endif
    PREFERRED_TYPE(TextAlignLast) unsigned textAlignLast : 3;
    PREFERRED_TYPE(TextJustify) unsigned textJustify : 2;
    PREFERRED_TYPE(TextDecorationSkipInk) unsigned textDecorationSkipInk : 2;
    PREFERRED_TYPE(MathShift) unsigned mathShift : 1;
    PREFERRED_TYPE(MathStyle) unsigned mathStyle : 1;
    PREFERRED_TYPE(RubyPosition) unsigned rubyPosition : 2;
    PREFERRED_TYPE(RubyAlign) unsigned rubyAlign : 2;
    PREFERRED_TYPE(RubyOverhang) unsigned rubyOverhang : 1;
    PREFERRED_TYPE(TextZoom) unsigned textZoom: 1;
#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    PREFERRED_TYPE(WebkitTouchCallout) unsigned touchCallout : 1;
#endif
    PREFERRED_TYPE(HangingPunctuation) unsigned hangingPunctuation : 4;
    PREFERRED_TYPE(SVGPaintOrder::Type) unsigned paintOrder : 3;
    PREFERRED_TYPE(LineCap) unsigned capStyle : 2;
    PREFERRED_TYPE(LineJoin) unsigned joinStyle : 2;
    PREFERRED_TYPE(bool) unsigned hasExplicitlySetStrokeWidth : 1;
    PREFERRED_TYPE(bool) unsigned hasExplicitlySetStrokeColor : 1;

private:
    InheritedRareCSSData();
    InheritedRareCSSData(const InheritedRareCSSData&);
};

} // namespace Style
} // namespace WebCore
