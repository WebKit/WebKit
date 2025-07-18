/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2024 Apple Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include "CounterDirectives.h"
#include "LengthPoint.h"
#include "LineClampValue.h"
#include "NameScope.h"
#include "NinePieceImage.h"
#include "PositionArea.h"
#include "PositionTryFallback.h"
#include "ScopedName.h"
#include "ScrollTypes.h"
#include "ShapeValue.h"
#include "StyleAnchorName.h"
#include "StyleClip.h"
#include "StyleClipPath.h"
#include "StyleColor.h"
#include "StyleContainIntrinsicSize.h"
#include "StyleContainerName.h"
#include "StyleContentAlignmentData.h"
#include "StyleGapGutter.h"
#include "StyleOffsetAnchor.h"
#include "StyleOffsetDistance.h"
#include "StyleOffsetPath.h"
#include "StyleOffsetPosition.h"
#include "StyleOffsetRotate.h"
#include "StylePerspective.h"
#include "StylePerspectiveOrigin.h"
#include "StylePrimitiveNumericTypes.h"
#include "StyleProgressTimelineAxes.h"
#include "StyleProgressTimelineName.h"
#include "StyleRotate.h"
#include "StyleScale.h"
#include "StyleScrollMargin.h"
#include "StyleScrollPadding.h"
#include "StyleScrollSnapPoints.h"
#include "StyleScrollTimelines.h"
#include "StyleScrollbarGutter.h"
#include "StyleSelfAlignmentData.h"
#include "StyleTextEdge.h"
#include "StyleTranslate.h"
#include "StyleViewTimelineInsets.h"
#include "StyleViewTimelines.h"
#include "StyleViewTransitionClass.h"
#include "TextDecorationThickness.h"
#include "TouchAction.h"
#include "ViewTransitionName.h"
#include <memory>
#include <wtf/DataRef.h>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

using namespace CSS::Literals;

class AnimationList;
class ContentData;
class PathOperation;
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
class WillChangeData;

struct LengthSize;
struct StyleMarqueeData;

namespace Style {
class CustomPropertyData;
}

// Page size type.
// StyleRareNonInheritedData::pageSize is meaningful only when
// StyleRareNonInheritedData::pageSizeType is PAGE_SIZE_RESOLVED.
enum class PageSizeType : uint8_t {
    Auto, // size: auto
    AutoLandscape, // size: landscape
    AutoPortrait, // size: portrait
    Resolved // Size is fully resolved.
};

// This struct is for rarely used non-inherited CSS3, CSS2, and WebKit-specific properties.
// By grouping them together, we save space, and only allocate this object when someone
// actually uses one of these properties.
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleRareNonInheritedData);
class StyleRareNonInheritedData : public RefCounted<StyleRareNonInheritedData> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(StyleRareNonInheritedData, StyleRareNonInheritedData);
public:
    static Ref<StyleRareNonInheritedData> create() { return adoptRef(*new StyleRareNonInheritedData); }
    Ref<StyleRareNonInheritedData> copy() const;
    ~StyleRareNonInheritedData();
    
    bool operator==(const StyleRareNonInheritedData&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const StyleRareNonInheritedData&) const;
#endif

    bool hasBackdropFilters() const;
    bool hasScrollTimelines() const { return !scrollTimelines.isEmpty() || !scrollTimelineNames.isNone(); }
    bool hasViewTimelines() const { return !viewTimelines.isEmpty() || !viewTimelineNames.isNone(); }

    OptionSet<Containment> usedContain() const;

    Style::ContainIntrinsicSize containIntrinsicWidth;
    Style::ContainIntrinsicSize containIntrinsicHeight;

    LineClampValue lineClamp; // An Apple extension.

    float zoom;

    size_t maxLines { 0 };

    OverflowContinue overflowContinue { OverflowContinue::Auto };

    OptionSet<TouchAction> touchActions;
    OptionSet<MarginTrimType> marginTrim;
    OptionSet<Containment> contain;

    IntSize initialLetter;

    DataRef<StyleMarqueeData> marquee; // Marquee properties

    DataRef<StyleFilterData> backdropFilter; // Filter operations (url, sepia, blur, etc.)

    DataRef<StyleGridData> grid;
    DataRef<StyleGridItemData> gridItem;

    Style::Clip clip { CSS::Keyword::Auto { } };

    Style::ScrollMarginBox scrollMargin { 0_css_px };
    Style::ScrollPaddingBox scrollPadding { CSS::Keyword::Auto { } };

    CounterDirectiveMap counterDirectives;

    RefPtr<WillChangeData> willChange; // Null indicates 'auto'.
    
    RefPtr<StyleReflection> boxReflect;

    NinePieceImage maskBorder;

    LengthSize pageSize;

    RefPtr<ShapeValue> shapeOutside;
    Length shapeMargin;
    float shapeImageThreshold;

    Style::Perspective perspective;
    Style::PerspectiveOrigin perspectiveOrigin;

    Style::ClipPath clipPath;

    Style::Color textDecorationColor;

    DataRef<Style::CustomPropertyData> customProperties;
    HashSet<AtomString> customPaintWatchedProperties;

    Style::Rotate rotate;
    Style::Scale scale;
    Style::Translate translate;

    Style::ContainerNames containerNames;

    Style::ViewTransitionClasses viewTransitionClasses;
    Style::ViewTransitionName viewTransitionName;

    Style::GapGutter columnGap;
    Style::GapGutter rowGap;

    Style::OffsetPath offsetPath;
    Style::OffsetDistance offsetDistance;
    Style::OffsetPosition offsetPosition;
    Style::OffsetAnchor offsetAnchor;
    Style::OffsetRotate offsetRotate;

    TextDecorationThickness textDecorationThickness;

    Style::ScrollTimelines scrollTimelines;
    Style::ProgressTimelineAxes scrollTimelineAxes;
    Style::ProgressTimelineNames scrollTimelineNames;

    Style::ViewTimelines viewTimelines;
    Style::ViewTimelineInsets viewTimelineInsets;
    Style::ProgressTimelineAxes viewTimelineAxes;
    Style::ProgressTimelineNames viewTimelineNames;

    NameScope timelineScope;

    Style::ScrollbarGutter scrollbarGutter;

    ScrollSnapType scrollSnapType;
    ScrollSnapAlign scrollSnapAlign;
    ScrollSnapStop scrollSnapStop { ScrollSnapStop::Normal };

    AtomString pseudoElementNameArgument;

    Style::AnchorNames anchorNames;
    NameScope anchorScope;
    std::optional<Style::ScopedName> positionAnchor;
    std::optional<PositionArea> positionArea;
    FixedVector<Style::PositionTryFallback> positionTryFallbacks;

    std::optional<Length> blockStepSize;
    PREFERRED_TYPE(BlockStepAlign) unsigned blockStepAlign : 2;
    PREFERRED_TYPE(BlockStepInsert) unsigned blockStepInsert : 2;
    PREFERRED_TYPE(BlockStepRound) unsigned blockStepRound : 2;

    PREFERRED_TYPE(OverscrollBehavior) unsigned overscrollBehaviorX : 2;
    PREFERRED_TYPE(OverscrollBehavior) unsigned overscrollBehaviorY : 2;

    PREFERRED_TYPE(PageSizeType) unsigned pageSizeType : 2;
    PREFERRED_TYPE(TransformStyle3D) unsigned transformStyle3D : 2;
    PREFERRED_TYPE(bool) unsigned transformStyleForcedToFlat : 1; // The used value for transform-style is forced to flat by a grouping property.
    PREFERRED_TYPE(BackfaceVisibility) unsigned backfaceVisibility : 1;

    PREFERRED_TYPE(ScrollBehavior) unsigned useSmoothScrolling : 1;
    PREFERRED_TYPE(TextDecorationStyle) unsigned textDecorationStyle : 3;
    PREFERRED_TYPE(TextGroupAlign) unsigned textGroupAlign : 3;
    PREFERRED_TYPE(ContentVisibility) unsigned contentVisibility : 2;
    PREFERRED_TYPE(BlendMode) unsigned effectiveBlendMode: 5;
    PREFERRED_TYPE(Isolation) unsigned isolation : 1;
    PREFERRED_TYPE(InputSecurity) unsigned inputSecurity : 1;
#if ENABLE(APPLE_PAY)
    PREFERRED_TYPE(ApplePayButtonStyle) unsigned applePayButtonStyle : 2;
    PREFERRED_TYPE(ApplePayButtonType) unsigned applePayButtonType : 4;
#endif
    PREFERRED_TYPE(BreakBetween) unsigned breakBefore : 4;
    PREFERRED_TYPE(BreakBetween) unsigned breakAfter : 4;
    PREFERRED_TYPE(BreakInside) unsigned breakInside : 3;
    PREFERRED_TYPE(ContainerType) unsigned containerType : 2;
    PREFERRED_TYPE(TextBoxTrim) unsigned textBoxTrim : 2;
    PREFERRED_TYPE(OverflowAnchor) unsigned overflowAnchor : 1;
    PREFERRED_TYPE(Style::PositionTryOrder) unsigned positionTryOrder : 3;
    PREFERRED_TYPE(OptionSet<PositionVisibility>) unsigned positionVisibility : 3;
    PREFERRED_TYPE(FieldSizing) unsigned fieldSizing : 1;
    PREFERRED_TYPE(bool) unsigned nativeAppearanceDisabled : 1;
#if HAVE(CORE_MATERIAL)
    PREFERRED_TYPE(AppleVisualEffect) unsigned appleVisualEffect : 5;
#endif
    PREFERRED_TYPE(ScrollbarWidth) unsigned scrollbarWidth : 2;
    PREFERRED_TYPE(bool) unsigned usesAnchorFunctions : 1;
    PREFERRED_TYPE(OptionSet<BoxAxisFlag>) unsigned anchorFunctionScrollCompensatedAxes : 2;
    PREFERRED_TYPE(bool) unsigned usesTreeCountingFunctions : 1;
    PREFERRED_TYPE(bool) unsigned isPopoverInvoker : 1;

private:
    StyleRareNonInheritedData();
    StyleRareNonInheritedData(const StyleRareNonInheritedData&);
};

} // namespace WebCore
