/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include "ClipPathOperation.h"
#include "CounterDirectives.h"
#include "FillLayer.h"
#include "GapLength.h"
#include "LengthPoint.h"
#include "LineClampValue.h"
#include "NinePieceImage.h"
#include "ShapeValue.h"
#include "StyleContentAlignmentData.h"
#include "StyleSelfAlignmentData.h"
#include "TouchAction.h"
#include "WillChangeData.h"
#include <memory>
#include <wtf/DataRef.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

class AnimationList;
class ContentData;
class ShadowData;
class StyleCustomPropertyData;
class StyleDeprecatedFlexibleBoxData;
class StyleFilterData;
class StyleFlexibleBoxData;
class StyleGridData;
class StyleGridItemData;
class StyleMarqueeData;
class StyleMultiColData;
class StyleReflection;
class StyleResolver;
class StyleScrollSnapArea;
class StyleScrollSnapPort;
class StyleTransformData;

struct LengthSize;

// Page size type.
// StyleRareNonInheritedData::pageSize is meaningful only when
// StyleRareNonInheritedData::pageSizeType is PAGE_SIZE_RESOLVED.
enum PageSizeType {
    PAGE_SIZE_AUTO, // size: auto
    PAGE_SIZE_AUTO_LANDSCAPE, // size: landscape
    PAGE_SIZE_AUTO_PORTRAIT, // size: portrait
    PAGE_SIZE_RESOLVED // Size is fully resolved.
};

// This struct is for rarely used non-inherited CSS3, CSS2, and WebKit-specific properties.
// By grouping them together, we save space, and only allocate this object when someone
// actually uses one of these properties.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareNonInheritedData);
class StyleRareNonInheritedData : public RefCounted<StyleRareNonInheritedData> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleRareNonInheritedData);
public:
    static Ref<StyleRareNonInheritedData> create() { return adoptRef(*new StyleRareNonInheritedData); }
    Ref<StyleRareNonInheritedData> copy() const;
    ~StyleRareNonInheritedData();
    
    bool operator==(const StyleRareNonInheritedData&) const;
    bool operator!=(const StyleRareNonInheritedData& other) const { return !(*this == other); }

    LengthPoint perspectiveOrigin() const { return { perspectiveOriginX, perspectiveOriginY }; }

    bool contentDataEquivalent(const StyleRareNonInheritedData&) const;

    bool hasFilters() const;

#if ENABLE(FILTERS_LEVEL_2)
    bool hasBackdropFilters() const;
#endif

    bool hasOpacity() const { return opacity < 1; }

    float opacity;

    float aspectRatioDenominator;
    float aspectRatioNumerator;

    float perspective;
    Length perspectiveOriginX;
    Length perspectiveOriginY;

    LineClampValue lineClamp; // An Apple extension.
    
    IntSize initialLetter;

    DataRef<StyleDeprecatedFlexibleBoxData> deprecatedFlexibleBox; // Flexible box properties
    DataRef<StyleFlexibleBoxData> flexibleBox;
    DataRef<StyleMarqueeData> marquee; // Marquee properties
    DataRef<StyleMultiColData> multiCol; //  CSS3 multicol properties
    DataRef<StyleTransformData> transform; // Transform properties (rotate, scale, skew, etc.)
    DataRef<StyleFilterData> filter; // Filter operations (url, sepia, blur, etc.)

#if ENABLE(FILTERS_LEVEL_2)
    DataRef<StyleFilterData> backdropFilter; // Filter operations (url, sepia, blur, etc.)
#endif

    DataRef<StyleGridData> grid;
    DataRef<StyleGridItemData> gridItem;

#if ENABLE(CSS_SCROLL_SNAP)
    DataRef<StyleScrollSnapPort> scrollSnapPort;
    DataRef<StyleScrollSnapArea> scrollSnapArea;
#endif

    std::unique_ptr<ContentData> content;
    std::unique_ptr<CounterDirectiveMap> counterDirectives;
    String altText;

    std::unique_ptr<ShadowData> boxShadow; // For box-shadow decorations.

    RefPtr<WillChangeData> willChange; // Null indicates 'auto'.
    
    RefPtr<StyleReflection> boxReflect;

    RefPtr<AnimationList> animations;
    RefPtr<AnimationList> transitions;

    DataRef<FillLayer> mask;
    NinePieceImage maskBoxImage;

    LengthSize pageSize;
    LengthPoint objectPosition;

    RefPtr<ShapeValue> shapeOutside;
    Length shapeMargin;
    float shapeImageThreshold;

    int order;

    RefPtr<ClipPathOperation> clipPath;

    Color textDecorationColor;
    Color visitedLinkTextDecorationColor;
    Color visitedLinkBackgroundColor;
    Color visitedLinkOutlineColor;
    Color visitedLinkBorderLeftColor;
    Color visitedLinkBorderRightColor;
    Color visitedLinkBorderTopColor;
    Color visitedLinkBorderBottomColor;

    StyleContentAlignmentData alignContent;
    StyleSelfAlignmentData alignItems;
    StyleSelfAlignmentData alignSelf;
    StyleContentAlignmentData justifyContent;
    StyleSelfAlignmentData justifyItems;
    StyleSelfAlignmentData justifySelf;

    DataRef<StyleCustomPropertyData> customProperties;
    std::unique_ptr<HashSet<String>> customPaintWatchedProperties;

    OptionSet<TouchAction> touchActions;

    unsigned pageSizeType : 2; // PageSizeType
    unsigned transformStyle3D : 1; // TransformStyle3D
    unsigned backfaceVisibility : 1; // BackfaceVisibility

    unsigned userDrag : 2; // UserDrag
    unsigned textOverflow : 1; // Whether or not lines that spill out should be truncated with "..."
    unsigned useSmoothScrolling : 1; // ScrollBehavior
    unsigned marginBeforeCollapse : 2; // MarginCollapse
    unsigned marginAfterCollapse : 2; // MarginCollapse
    unsigned appearance : 6; // EAppearance
    unsigned borderFit : 1; // BorderFit
    unsigned textCombine : 1; // CSS3 text-combine properties

    unsigned textDecorationStyle : 3; // TextDecorationStyle

    unsigned aspectRatioType : 2;

#if ENABLE(CSS_COMPOSITING)
    unsigned effectiveBlendMode: 5; // EBlendMode
    unsigned isolation : 1; // Isolation
#endif

#if ENABLE(APPLE_PAY)
    unsigned applePayButtonStyle : 2;
    unsigned applePayButtonType : 4;
#endif

    unsigned objectFit : 3; // ObjectFit
    
    unsigned breakBefore : 4; // BreakBetween
    unsigned breakAfter : 4;
    unsigned breakInside : 3; // BreakInside
    unsigned resize : 2; // Resize

    unsigned hasAttrContent : 1;

    unsigned isNotFinal : 1;

    GapLength columnGap;
    GapLength rowGap;

private:
    StyleRareNonInheritedData();
    StyleRareNonInheritedData(const StyleRareNonInheritedData&);
};

} // namespace WebCore
