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

#ifndef StyleRareNonInheritedData_h
#define StyleRareNonInheritedData_h

#include "BasicShapes.h"
#include "CSSPropertyNames.h"
#include "ClipPathOperation.h"
#include "CounterDirectives.h"
#include "CursorData.h"
#include "DataRef.h"
#include "FillLayer.h"
#include "LengthPoint.h"
#include "LineClampValue.h"
#include "NinePieceImage.h"
#include "ShapeValue.h"
#include "StyleContentAlignmentData.h"
#include "StyleSelfAlignmentData.h"
#include "WillChangeData.h"
#include <memory>
#include <wtf/PassRefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class AnimationList;
class ShadowData;
class StyleDeprecatedFlexibleBoxData;
class StyleFilterData;
class StyleFlexibleBoxData;
#if ENABLE(CSS_GRID_LAYOUT)
class StyleGridData;
class StyleGridItemData;
#endif
class StyleMarqueeData;
class StyleMultiColData;
class StyleReflection;
class StyleResolver;
class StyleTransformData;
#if ENABLE(CSS_SCROLL_SNAP)
class StyleScrollSnapPoints;
#endif

class ContentData;
struct LengthSize;

#if ENABLE(DASHBOARD_SUPPORT)
struct StyleDashboardRegion;
#endif

// Page size type.
// StyleRareNonInheritedData::m_pageSize is meaningful only when 
// StyleRareNonInheritedData::m_pageSizeType is PAGE_SIZE_RESOLVED.
enum PageSizeType {
    PAGE_SIZE_AUTO, // size: auto
    PAGE_SIZE_AUTO_LANDSCAPE, // size: landscape
    PAGE_SIZE_AUTO_PORTRAIT, // size: portrait
    PAGE_SIZE_RESOLVED // Size is fully resolved.
};

// This struct is for rarely used non-inherited CSS3, CSS2, and WebKit-specific properties.
// By grouping them together, we save space, and only allocate this object when someone
// actually uses one of these properties.
class StyleRareNonInheritedData : public RefCounted<StyleRareNonInheritedData> {
public:
    static Ref<StyleRareNonInheritedData> create() { return adoptRef(*new StyleRareNonInheritedData); }
    Ref<StyleRareNonInheritedData> copy() const;
    ~StyleRareNonInheritedData();
    
    bool operator==(const StyleRareNonInheritedData&) const;
    bool operator!=(const StyleRareNonInheritedData& o) const { return !(*this == o); }

    bool contentDataEquivalent(const StyleRareNonInheritedData&) const;

    bool hasFilters() const;
#if ENABLE(FILTERS_LEVEL_2)
    bool hasBackdropFilters() const;
#endif
    bool hasOpacity() const { return opacity < 1; }

    bool hasAnimationsOrTransitions() const { return m_animations || m_transitions; }

    float opacity;

    float m_aspectRatioDenominator;
    float m_aspectRatioNumerator;

    float m_perspective;
    Length m_perspectiveOriginX;
    Length m_perspectiveOriginY;

    LineClampValue lineClamp; // An Apple extension.
    
    IntSize m_initialLetter;

#if ENABLE(DASHBOARD_SUPPORT)
    Vector<StyleDashboardRegion> m_dashboardRegions;
#endif

    DataRef<StyleDeprecatedFlexibleBoxData> m_deprecatedFlexibleBox; // Flexible box properties
    DataRef<StyleFlexibleBoxData> m_flexibleBox;
    DataRef<StyleMarqueeData> m_marquee; // Marquee properties
    DataRef<StyleMultiColData> m_multiCol; //  CSS3 multicol properties
    DataRef<StyleTransformData> m_transform; // Transform properties (rotate, scale, skew, etc.)
    DataRef<StyleFilterData> m_filter; // Filter operations (url, sepia, blur, etc.)
#if ENABLE(FILTERS_LEVEL_2)
    DataRef<StyleFilterData> m_backdropFilter; // Filter operations (url, sepia, blur, etc.)
#endif

#if ENABLE(CSS_GRID_LAYOUT)
    DataRef<StyleGridData> m_grid;
    DataRef<StyleGridItemData> m_gridItem;
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    DataRef<StyleScrollSnapPoints> m_scrollSnapPoints;
#endif

    std::unique_ptr<ContentData> m_content;
    std::unique_ptr<CounterDirectiveMap> m_counterDirectives;
    String m_altText;

    std::unique_ptr<ShadowData> m_boxShadow; // For box-shadow decorations.

    RefPtr<WillChangeData> m_willChange; // Null indicates 'auto'.
    
    RefPtr<StyleReflection> m_boxReflect;

    std::unique_ptr<AnimationList> m_animations;
    std::unique_ptr<AnimationList> m_transitions;

    FillLayer m_mask;
    NinePieceImage m_maskBoxImage;

    LengthSize m_pageSize;
    LengthPoint m_objectPosition;

#if ENABLE(CSS_SHAPES)
    RefPtr<ShapeValue> m_shapeOutside;
    Length m_shapeMargin;
    float m_shapeImageThreshold;
#endif

    RefPtr<ClipPathOperation> m_clipPath;

    Color m_textDecorationColor;
    Color m_visitedLinkTextDecorationColor;
    Color m_visitedLinkBackgroundColor;
    Color m_visitedLinkOutlineColor;
    Color m_visitedLinkBorderLeftColor;
    Color m_visitedLinkBorderRightColor;
    Color m_visitedLinkBorderTopColor;
    Color m_visitedLinkBorderBottomColor;

    int m_order;

    AtomicString m_flowThread;
    AtomicString m_regionThread;

    StyleContentAlignmentData m_alignContent;
    StyleSelfAlignmentData m_alignItems;
    StyleSelfAlignmentData m_alignSelf;
    StyleContentAlignmentData m_justifyContent;
    StyleSelfAlignmentData m_justifyItems;
    StyleSelfAlignmentData m_justifySelf;

#if ENABLE(TOUCH_EVENTS)
    unsigned m_touchAction : 1; // TouchAction
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    unsigned m_scrollSnapType : 2; // ScrollSnapType
#endif

    unsigned m_regionFragment : 1; // RegionFragment

    unsigned m_pageSizeType : 2; // PageSizeType
    unsigned m_transformStyle3D : 1; // ETransformStyle3D
    unsigned m_backfaceVisibility : 1; // EBackfaceVisibility


    unsigned userDrag : 2; // EUserDrag
    unsigned textOverflow : 1; // Whether or not lines that spill out should be truncated with "..."
    unsigned marginBeforeCollapse : 2; // EMarginCollapse
    unsigned marginAfterCollapse : 2; // EMarginCollapse
    unsigned m_appearance : 6; // EAppearance
    unsigned m_borderFit : 1; // EBorderFit
    unsigned m_textCombine : 1; // CSS3 text-combine properties

    unsigned m_textDecorationStyle : 3; // TextDecorationStyle

    unsigned m_runningAcceleratedAnimation : 1;

    unsigned m_aspectRatioType : 2;

#if ENABLE(CSS_COMPOSITING)
    unsigned m_effectiveBlendMode: 5; // EBlendMode
    unsigned m_isolation : 1; // Isolation
#endif

    unsigned m_objectFit : 3; // ObjectFit
    
    unsigned m_breakBefore : 4; // BreakBetween
    unsigned m_breakAfter : 4;
    unsigned m_breakInside : 3; // BreakInside
    unsigned m_resize : 2; // EResize

private:
    StyleRareNonInheritedData();
    StyleRareNonInheritedData(const StyleRareNonInheritedData&);
};

} // namespace WebCore

#endif // StyleRareNonInheritedData_h
