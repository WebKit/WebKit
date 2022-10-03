/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "Length.h"
#include "RenderStyleConstants.h"
#include "StyleColor.h"
#include "StyleCustomPropertyData.h"
#include "TabSize.h"
#include "TextUnderlineOffset.h"
#include "TouchAction.h"
#include <wtf/DataRef.h>
#include <wtf/OptionSet.h>
#include <wtf/RefCounted.h>
#include <wtf/text/AtomString.h>

#if ENABLE(TEXT_AUTOSIZING)
#include "TextSizeAdjustment.h"
#endif

#if ENABLE(DARK_MODE_CSS)
#include "StyleColorScheme.h"
#endif

namespace WebCore {

class CursorList;
class QuotesData;
class ShadowData;
class StyleFilterData;
class StyleImage;

// This struct is for rarely used inherited CSS3, CSS2, and WebKit-specific properties.
// By grouping them together, we save space, and only allocate this object when someone
// actually uses one of these properties.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareInheritedData);
class StyleRareInheritedData : public RefCounted<StyleRareInheritedData> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleRareInheritedData);
public:
    static Ref<StyleRareInheritedData> create() { return adoptRef(*new StyleRareInheritedData); }
    Ref<StyleRareInheritedData> copy() const;
    ~StyleRareInheritedData();

    bool operator==(const StyleRareInheritedData& o) const;
    bool operator!=(const StyleRareInheritedData& o) const
    {
        return !(*this == o);
    }

    bool hasColorFilters() const;

    float textStrokeWidth;

    RefPtr<StyleImage> listStyleImage;
    AtomString listStyleStringValue;

    StyleColor textStrokeColor;
    StyleColor textFillColor;
    StyleColor textEmphasisColor;
    
    StyleColor visitedLinkTextStrokeColor;
    StyleColor visitedLinkTextFillColor;
    StyleColor visitedLinkTextEmphasisColor;

    StyleColor caretColor;
    StyleColor visitedLinkCaretColor;

    StyleColor accentColor;

    std::unique_ptr<ShadowData> textShadow;
    
    RefPtr<CursorList> cursorData;
    Length indent;
    float effectiveZoom;

    TextUnderlineOffset textUnderlineOffset;
    
    Length wordSpacing;
    float miterLimit;

    DataRef<StyleCustomPropertyData> customProperties;

    // Paged media properties.
    unsigned short widows;
    unsigned short orphans;
    unsigned hasAutoWidows : 1;
    unsigned hasAutoOrphans : 1;
    
    unsigned textSecurity : 2; // TextSecurity
    unsigned userModify : 2; // UserModify (editing)
    unsigned wordBreak : 2; // WordBreak
    unsigned overflowWrap : 2; // OverflowWrap
    unsigned nbspMode : 1; // NBSPMode
    unsigned lineBreak : 3; // LineBreak
    unsigned userSelect : 2; // UserSelect
    unsigned colorSpace : 1; // ColorSpace
    unsigned speakAs : 4; // ESpeakAs
    unsigned hyphens : 2; // Hyphens
    unsigned textCombine : 1; // text-combine-upright
    unsigned textEmphasisFill : 1; // TextEmphasisFill
    unsigned textEmphasisMark : 3; // TextEmphasisMark
    unsigned textEmphasisPosition : 4; // TextEmphasisPosition
    unsigned textOrientation : 2; // TextOrientation
    unsigned textIndentLine : 1; // TextIndentLine
    unsigned textIndentType : 1; // TextIndentType
    unsigned lineBoxContain: 7; // OptionSet<LineBoxContain>
    // CSS Image Values Level 3
    unsigned imageOrientation : 1; // ImageOrientation
    unsigned imageRendering : 3; // ImageRendering
    unsigned lineSnap : 2; // LineSnap
    unsigned lineAlign : 1; // LineAlign
#if ENABLE(OVERFLOW_SCROLLING_TOUCH)
    unsigned useTouchOverflowScrolling: 1;
#endif
#if ENABLE(CSS_IMAGE_RESOLUTION)
    unsigned imageResolutionSource : 1; // ImageResolutionSource
    unsigned imageResolutionSnap : 1; // ImageResolutionSnap
#endif
    unsigned textAlignLast : 3; // TextAlignLast
    unsigned textJustify : 2; // TextJustify
    unsigned textDecorationSkipInk : 2; // TextDecorationSkipInk
    unsigned textUnderlinePosition : 2; // TextUnderlinePosition
    unsigned rubyPosition : 2; // RubyPosition
    unsigned textZoom: 1; // TextZoom

#if PLATFORM(IOS_FAMILY)
    unsigned touchCalloutEnabled : 1;
#endif

    unsigned hangingPunctuation : 4;

    unsigned paintOrder : 3; // PaintOrder
    unsigned capStyle : 2; // LineCap
    unsigned joinStyle : 2; // LineJoin
    unsigned hasSetStrokeWidth : 1;
    unsigned hasSetStrokeColor : 1;

    unsigned mathStyle : 1;

    unsigned hasAutoCaretColor : 1;
    unsigned hasVisitedLinkAutoCaretColor : 1;

    unsigned hasAutoAccentColor : 1;

    unsigned effectiveInert : 1;

    unsigned isInSubtreeWithBlendMode : 1;

    OptionSet<TouchAction> effectiveTouchActions;
    OptionSet<EventListenerRegionType> eventListenerRegionTypes;

    Length strokeWidth;
    StyleColor strokeColor;
    StyleColor visitedLinkStrokeColor;

    AtomString hyphenationString;
    short hyphenationLimitBefore { -1 };
    short hyphenationLimitAfter { -1 };
    short hyphenationLimitLines { -1 };

#if ENABLE(DARK_MODE_CSS)
    StyleColorScheme colorScheme;
#endif

    AtomString textEmphasisCustomMark;
    RefPtr<QuotesData> quotes;
    DataRef<StyleFilterData> appleColorFilter;

    AtomString lineGrid;
    TabSize tabSize;

#if ENABLE(TEXT_AUTOSIZING)
    TextSizeAdjustment textSizeAdjust;
#endif

#if ENABLE(CSS_IMAGE_RESOLUTION)
    float imageResolution;
#endif

#if ENABLE(TOUCH_EVENTS)
    StyleColor tapHighlightColor;
#endif

private:
    StyleRareInheritedData();
    StyleRareInheritedData(const StyleRareInheritedData&);
};

} // namespace WebCore
