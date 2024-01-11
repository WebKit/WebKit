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
#include "CounterDirectives.h"
#include "FillLayer.h"
#include "GapLength.h"
#include "LengthPoint.h"
#include "LineClampValue.h"
#include "NinePieceImage.h"
#include "OffsetRotation.h"
#include "PathOperation.h"
#include "RotateTransformOperation.h"
#include "ScaleTransformOperation.h"
#include "ScopedName.h"
#include "ScrollAxis.h"
#include "ScrollTimeline.h"
#include "ScrollTypes.h"
#include "ScrollbarGutter.h"
#include "ShapeValue.h"
#include "StyleColor.h"
#include "StyleContentAlignmentData.h"
#include "StyleScrollSnapPoints.h"
#include "StyleSelfAlignmentData.h"
#include "TextDecorationThickness.h"
#include "TouchAction.h"
#include "TranslateTransformOperation.h"
#include "ViewTimeline.h"
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
class StyleMultiColData;
class StyleReflection;
class StyleResolver;
class StyleTransformData;

struct LengthSize;
struct StyleMarqueeData;

// Page size type.
// StyleRareNonInheritedData::pageSize is meaningful only when
// StyleRareNonInheritedData::pageSizeType is PAGE_SIZE_RESOLVED.
enum PageSizeType : uint8_t {
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

    LengthPoint perspectiveOrigin() const { return { perspectiveOriginX, perspectiveOriginY }; }

    bool hasBackdropFilters() const;

    OptionSet<Containment> effectiveContainment() const;

    std::optional<Length> containIntrinsicWidth;
    std::optional<Length> containIntrinsicHeight;

    Length perspectiveOriginX;
    Length perspectiveOriginY;

    LineClampValue lineClamp; // An Apple extension.
    
    IntSize initialLetter;

    DataRef<StyleMarqueeData> marquee; // Marquee properties

    DataRef<StyleFilterData> backdropFilter; // Filter operations (url, sepia, blur, etc.)

    DataRef<StyleGridData> grid;
    DataRef<StyleGridItemData> gridItem;

    LengthBox clip;
    LengthBox scrollMargin { 0, 0, 0, 0 };
    LengthBox scrollPadding { Length(LengthType::Auto), Length(LengthType::Auto), Length(LengthType::Auto), Length(LengthType::Auto) };

    CounterDirectiveMap counterDirectives;

    RefPtr<WillChangeData> willChange; // Null indicates 'auto'.
    
    RefPtr<StyleReflection> boxReflect;

    NinePieceImage maskBorder;

    LengthSize pageSize;

    RefPtr<ShapeValue> shapeOutside;
    Length shapeMargin;
    float shapeImageThreshold;

    float perspective;

    RefPtr<PathOperation> clipPath;

    StyleColor textDecorationColor;

    DataRef<StyleCustomPropertyData> customProperties;
    HashSet<AtomString> customPaintWatchedProperties;

    RefPtr<RotateTransformOperation> rotate;
    RefPtr<ScaleTransformOperation> scale;
    RefPtr<TranslateTransformOperation> translate;
    RefPtr<PathOperation> offsetPath;

    Vector<Style::ScopedName> containerNames;
    std::optional<Style::ScopedName> viewTransitionName;

    GapLength columnGap;
    GapLength rowGap;

    Length offsetDistance;
    LengthPoint offsetPosition;
    LengthPoint offsetAnchor;
    OffsetRotation offsetRotate;

    TextDecorationThickness textDecorationThickness;

    OptionSet<TouchAction> touchActions;
    OptionSet<MarginTrimType> marginTrim;
    OptionSet<Containment> contain;

    ScrollSnapType scrollSnapType;
    ScrollSnapAlign scrollSnapAlign;
    ScrollSnapStop scrollSnapStop { ScrollSnapStop::Normal };

    Vector<Ref<ScrollTimeline>> scrollTimelines;
    Vector<ScrollAxis> scrollTimelineAxes;
    Vector<AtomString> scrollTimelineNames;

    Vector<Ref<ViewTimeline>> viewTimelines;
    Vector<ScrollAxis> viewTimelineAxes;
    Vector<ViewTimelineInsets> viewTimelineInsets;
    Vector<AtomString> viewTimelineNames;

    ScrollbarGutter scrollbarGutter;
    ScrollbarWidth scrollbarWidth { ScrollbarWidth::Auto };

    float zoom;

    std::optional<Length> blockStepSize;
    unsigned blockStepInsert : 1; // BlockStepInsert

    unsigned overscrollBehaviorX : 2; // OverscrollBehavior
    unsigned overscrollBehaviorY : 2; // OverscrollBehavior

    unsigned pageSizeType : 2; // PageSizeType
    unsigned transformStyle3D : 2; // TransformStyle3D
    unsigned transformStyleForcedToFlat : 1; // The used value for transform-style is forced to flat by a grouping property.
    unsigned backfaceVisibility : 1; // BackfaceVisibility

    unsigned useSmoothScrolling : 1; // ScrollBehavior

    unsigned textDecorationStyle : 3; // TextDecorationStyle

    unsigned textGroupAlign : 3; // TextGroupAlign

    unsigned contentVisibility : 2; // ContentVisibility

    unsigned effectiveBlendMode: 5; // EBlendMode
    unsigned isolation : 1; // Isolation

#if ENABLE(APPLE_PAY)
    unsigned applePayButtonStyle : 2;
    unsigned applePayButtonType : 4;
#endif

    unsigned breakBefore : 4; // BreakBetween
    unsigned breakAfter : 4;
    unsigned breakInside : 3; // BreakInside

    unsigned inputSecurity : 1; // InputSecurity

    unsigned containIntrinsicWidthType : 2; // ContainIntrinsicSizeType
    unsigned containIntrinsicHeightType : 2; // ContainIntrinsicSizeType

    unsigned containerType : 2; // ContainerType

    unsigned textBoxTrim : 2; // TextBoxTrim

    unsigned overflowAnchor : 1; // Scroll Anchoring- OverflowAnchor

    bool hasClip : 1;

private:
    StyleRareNonInheritedData();
    StyleRareNonInheritedData(const StyleRareNonInheritedData&);
};

} // namespace WebCore
