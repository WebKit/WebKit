/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include <WebCore/StyleColor.h>
#include <WebCore/StyleCursor.h>
#include <WebCore/StyleCustomPropertyData.h>
#include <WebCore/StyleDynamicRangeLimit.h>
#include <WebCore/StyleHyphenateCharacter.h>
#include <WebCore/StyleHyphenateLimitEdge.h>
#include <WebCore/StyleHyphenateLimitLines.h>
#include <WebCore/StyleImageOrNone.h>
#include <WebCore/StyleLineBoxContain.h>
#include <WebCore/StyleLineFitEdge.h>
#include <WebCore/StyleListStyleType.h>
#include <WebCore/StyleOrphans.h>
#include <WebCore/StyleQuotes.h>
#include <WebCore/StyleScrollbarColor.h>
#include <WebCore/StyleStrokeMiterlimit.h>
#include <WebCore/StyleStrokeWidth.h>
#include <WebCore/StyleTabSize.h>
#include <WebCore/StyleTextBoxEdge.h>
#include <WebCore/StyleTextEmphasisStyle.h>
#include <WebCore/StyleTextIndent.h>
#include <WebCore/StyleTextShadow.h>
#include <WebCore/StyleTextUnderlineOffset.h>
#include <WebCore/StyleWebKitLineGrid.h>
#include <WebCore/StyleWebKitOverflowScrolling.h>
#include <WebCore/StyleWebKitTextStrokeWidth.h>
#include <WebCore/StyleWebKitTouchCallout.h>
#include <WebCore/StyleWidows.h>
#include <WebCore/TouchAction.h>
#include <wtf/DataRef.h>
#include <wtf/FixedVector.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomString.h>

#if HAVE(CORE_MATERIAL)
#include <WebCore/AppleVisualEffect.h>
#endif

#if ENABLE(TEXT_AUTOSIZING)
#include <WebCore/StyleTextSizeAdjust.h>
#endif

#if ENABLE(DARK_MODE_CSS)
#include <WebCore/StyleColorScheme.h>
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class StyleAppleColorFilterData;
class StyleImage;

// This struct is for rarely used inherited CSS3, CSS2, and WebKit-specific properties.
// By grouping them together, we save space, and only allocate this object when someone
// actually uses one of these properties.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareInheritedData);
class StyleRareInheritedData : public RefCounted<StyleRareInheritedData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleRareInheritedData, StyleRareInheritedData);
public:
    static Ref<StyleRareInheritedData> create() { return adoptRef(*new StyleRareInheritedData); }
    Ref<StyleRareInheritedData> copy() const;
    ~StyleRareInheritedData();

    bool operator==(const StyleRareInheritedData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleRareInheritedData&) const;
#endif

    bool hasColorFilters() const;

    float usedZoom;

    Style::ImageOrNone listStyleImage;

    Style::WebkitTextStrokeWidth textStrokeWidth;
    Style::Color textStrokeColor;
    Style::Color textFillColor;
    Style::Color textEmphasisColor;
    
    Style::Color visitedLinkTextStrokeColor;
    Style::Color visitedLinkTextFillColor;
    Style::Color visitedLinkTextEmphasisColor;

    Style::Color caretColor;
    Style::Color visitedLinkCaretColor;

    Style::AccentColor accentColor;

    Style::ScrollbarColor scrollbarColor;

    Style::DynamicRangeLimit dynamicRangeLimit;

    Style::TextShadows textShadow;

    // The `cursor` property's state is stored broken up into two parts:
    //  - the cursor's `predefined` state is stored in `RenderStyle::InheritedFlags::cursor`.
    //  - the cursor's `images` state is stored here in `StyleRareInheritedData::cursorImages`.
    Style::Cursor::Images cursorImages;

    Style::TextEmphasisStyle textEmphasisStyle;
    Style::TextIndent textIndent;
    Style::TextUnderlineOffset textUnderlineOffset;

    Style::TextBoxEdge textBoxEdge;
    Style::LineFitEdge lineFitEdge;

    Style::StrokeMiterlimit miterLimit;

    DataRef<Style::CustomPropertyData> customProperties;

    Style::Widows widows;
    Style::Orphans orphans;

    PREFERRED_TYPE(TextSecurity) unsigned textSecurity : 2;
    PREFERRED_TYPE(UserModify) unsigned userModify : 2;
    PREFERRED_TYPE(WordBreak) unsigned wordBreak : 3;
    PREFERRED_TYPE(OverflowWrap) unsigned overflowWrap : 2;
    PREFERRED_TYPE(NBSPMode) unsigned nbspMode : 1;
    PREFERRED_TYPE(LineBreak) unsigned lineBreak : 3;
    PREFERRED_TYPE(UserSelect) unsigned userSelect : 2;
    PREFERRED_TYPE(ColorSpace) unsigned colorSpace : 1;
    PREFERRED_TYPE(OptionSet<SpeakAs>) unsigned speakAs : 4 { 0 };
    PREFERRED_TYPE(Hyphens) unsigned hyphens : 2;
    PREFERRED_TYPE(TextCombine) unsigned textCombine : 1;
    PREFERRED_TYPE(TextEmphasisPosition) unsigned textEmphasisPosition : 4;
    PREFERRED_TYPE(TextUnderlinePosition) unsigned textUnderlinePosition : 4;
    PREFERRED_TYPE(OptionSet<Style::LineBoxContain>) unsigned lineBoxContain: 7;
    PREFERRED_TYPE(ImageOrientation) unsigned imageOrientation : 1;
    PREFERRED_TYPE(ImageRendering) unsigned imageRendering : 3;
    PREFERRED_TYPE(LineSnap) unsigned lineSnap : 2;
    PREFERRED_TYPE(LineAlign) unsigned lineAlign : 1;
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    PREFERRED_TYPE(Style::WebkitOverflowScrolling) unsigned webkitOverflowScrolling: 1;
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
    PREFERRED_TYPE(Style::WebkitTouchCallout) unsigned webkitTouchCallout : 1;
#endif
    PREFERRED_TYPE(OptionSet<HangingPunctuation>) unsigned hangingPunctuation : 4;
    PREFERRED_TYPE(PaintOrder) unsigned paintOrder : 3;
    PREFERRED_TYPE(LineCap) unsigned capStyle : 2;
    PREFERRED_TYPE(LineJoin) unsigned joinStyle : 2;
    PREFERRED_TYPE(bool) unsigned hasSetStrokeWidth : 1;
    PREFERRED_TYPE(bool) unsigned hasSetStrokeColor : 1;
    PREFERRED_TYPE(bool) unsigned hasAutoCaretColor : 1;
    PREFERRED_TYPE(bool) unsigned hasVisitedLinkAutoCaretColor : 1;
    PREFERRED_TYPE(bool) unsigned effectiveInert : 1;
    PREFERRED_TYPE(bool) unsigned effectivelyTransparent : 1;
    PREFERRED_TYPE(bool) unsigned isInSubtreeWithBlendMode : 1;
    PREFERRED_TYPE(bool) unsigned isForceHidden : 1;
    PREFERRED_TYPE(ContentVisibility) unsigned usedContentVisibility : 2;
    PREFERRED_TYPE(bool) unsigned autoRevealsWhenFound : 1;
    PREFERRED_TYPE(bool) unsigned insideDefaultButton : 1;
    PREFERRED_TYPE(bool) unsigned insideSubmitButton : 1;
    PREFERRED_TYPE(bool) unsigned enableEvaluationTimeZoom : 1;
#if HAVE(CORE_MATERIAL)
    PREFERRED_TYPE(AppleVisualEffect) unsigned usedAppleVisualEffectForSubtree : 5;
#endif

    OptionSet<TouchAction> usedTouchActions;
    OptionSet<EventListenerRegionType> eventListenerRegionTypes;

    Style::StrokeWidth strokeWidth;
    Style::Color strokeColor;
    Style::Color visitedLinkStrokeColor;

    Style::HyphenateCharacter hyphenateCharacter;
    Style::HyphenateLimitEdge hyphenateLimitBefore;
    Style::HyphenateLimitEdge hyphenateLimitAfter;
    Style::HyphenateLimitLines hyphenateLimitLines;

#if ENABLE(DARK_MODE_CSS)
    Style::ColorScheme colorScheme;
#endif

    Style::Quotes quotes;

    DataRef<StyleAppleColorFilterData> appleColorFilter;

    Style::WebkitLineGrid lineGrid;
    Style::TabSize tabSize;

#if ENABLE(TEXT_AUTOSIZING)
    Style::TextSizeAdjust textSizeAdjust;
#endif

#if ENABLE(TOUCH_EVENTS)
    Style::Color tapHighlightColor;
#endif
    Style::ListStyleType listStyleType;
    Style::BlockEllipsis blockEllipsis;

private:
    StyleRareInheritedData();
    StyleRareInheritedData(const StyleRareInheritedData&);
};

} // namespace WebCore
