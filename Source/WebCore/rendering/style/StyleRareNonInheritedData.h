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

#include <WebCore/CSSPropertyNames.h>
#include <WebCore/CounterDirectives.h>
#include <WebCore/ScopedName.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/StyleAnchorName.h>
#include <WebCore/StyleBlockStepSize.h>
#include <WebCore/StyleClip.h>
#include <WebCore/StyleClipPath.h>
#include <WebCore/StyleColor.h>
#include <WebCore/StyleContain.h>
#include <WebCore/StyleContainIntrinsicSize.h>
#include <WebCore/StyleContainerName.h>
#include <WebCore/StyleGapGutter.h>
#include <WebCore/StyleItemTolerance.h>
#include <WebCore/StyleMarginTrim.h>
#include <WebCore/StyleMaskBorder.h>
#include <WebCore/StyleMaximumLines.h>
#include <WebCore/StyleNameScope.h>
#include <WebCore/StyleOffsetAnchor.h>
#include <WebCore/StyleOffsetDistance.h>
#include <WebCore/StyleOffsetPath.h>
#include <WebCore/StyleOffsetPosition.h>
#include <WebCore/StyleOffsetRotate.h>
#include <WebCore/StylePageSize.h>
#include <WebCore/StylePerspective.h>
#include <WebCore/StylePerspectiveOrigin.h>
#include <WebCore/StylePositionAnchor.h>
#include <WebCore/StylePositionArea.h>
#include <WebCore/StylePositionTryFallbacks.h>
#include <WebCore/StylePositionVisibility.h>
#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleProgressTimelineAxes.h>
#include <WebCore/StyleProgressTimelineName.h>
#include <WebCore/StyleRotate.h>
#include <WebCore/StyleScale.h>
#include <WebCore/StyleScrollBehavior.h>
#include <WebCore/StyleScrollMargin.h>
#include <WebCore/StyleScrollPadding.h>
#include <WebCore/StyleScrollSnapAlign.h>
#include <WebCore/StyleScrollSnapType.h>
#include <WebCore/StyleScrollTimelines.h>
#include <WebCore/StyleScrollbarGutter.h>
#include <WebCore/StyleScrollbarWidth.h>
#include <WebCore/StyleShapeImageThreshold.h>
#include <WebCore/StyleShapeMargin.h>
#include <WebCore/StyleShapeOutside.h>
#include <WebCore/StyleTextDecorationThickness.h>
#include <WebCore/StyleTouchAction.h>
#include <WebCore/StyleTranslate.h>
#include <WebCore/StyleViewTimelineInsets.h>
#include <WebCore/StyleViewTimelines.h>
#include <WebCore/StyleViewTransitionClass.h>
#include <WebCore/StyleViewTransitionName.h>
#include <WebCore/StyleWebKitBoxReflect.h>
#include <WebCore/StyleWebKitInitialLetter.h>
#include <WebCore/StyleWebKitLineClamp.h>
#include <WebCore/StyleWillChange.h>
#include <memory>
#include <wtf/DataRef.h>
#include <wtf/Markable.h>
#include <wtf/OptionSet.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

using namespace CSS::Literals;

class PathOperation;
class StyleBackdropFilterData;
class StyleCustomPropertyData;
class StyleDeprecatedFlexibleBoxData;
class StyleFlexibleBoxData;
class StyleGridData;
class StyleGridItemData;
class StyleMultiColData;
class StyleResolver;
class StyleTransformData;

struct StyleMarqueeData;

namespace Style {
class CustomPropertyData;
}

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

    Style::Contain usedContain() const;

    Style::ContainIntrinsicSize containIntrinsicWidth;
    Style::ContainIntrinsicSize containIntrinsicHeight;

    Style::WebkitLineClamp lineClamp;

    float zoom;

    Style::MaximumLines maxLines;

    OverflowContinue overflowContinue { OverflowContinue::Auto };

    Style::TouchAction touchAction;

    Style::WebkitInitialLetter initialLetter;

    DataRef<StyleMarqueeData> marquee; // Marquee properties

    DataRef<StyleBackdropFilterData> backdropFilter; // Filter operations (url, sepia, blur, etc.)

    DataRef<StyleGridData> grid;
    DataRef<StyleGridItemData> gridItem;

    Style::Clip clip { CSS::Keyword::Auto { } };

    Style::ScrollMarginBox scrollMargin { 0_css_px };
    Style::ScrollPaddingBox scrollPadding { CSS::Keyword::Auto { } };

    CounterDirectiveMap counterDirectives;

    Style::WillChange willChange;

    Style::WebkitBoxReflect boxReflect;

    Style::MaskBorder maskBorder;

    Style::PageSize pageSize;

    Style::ShapeOutside shapeOutside;
    Style::ShapeMargin shapeMargin;
    Style::ShapeImageThreshold shapeImageThreshold;

    Style::Perspective perspective;
    Style::PerspectiveOrigin perspectiveOrigin;

    Style::ClipPath clipPath;

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

    Style::ItemTolerance itemTolerance;

    Style::OffsetPath offsetPath;
    Style::OffsetDistance offsetDistance;
    Style::OffsetPosition offsetPosition;
    Style::OffsetAnchor offsetAnchor;
    Style::OffsetRotate offsetRotate;

    Style::Color textDecorationColor;
    Style::TextDecorationThickness textDecorationThickness;

    Style::ScrollTimelines scrollTimelines;
    Style::ProgressTimelineAxes scrollTimelineAxes;
    Style::ProgressTimelineNames scrollTimelineNames;

    Style::ViewTimelines viewTimelines;
    Style::ViewTimelineInsets viewTimelineInsets;
    Style::ProgressTimelineAxes viewTimelineAxes;
    Style::ProgressTimelineNames viewTimelineNames;

    Style::NameScope timelineScope;

    Style::ScrollbarGutter scrollbarGutter;

    Style::ScrollSnapType scrollSnapType;
    Style::ScrollSnapAlign scrollSnapAlign;
    ScrollSnapStop scrollSnapStop;

    AtomString pseudoElementNameArgument;

    Style::AnchorNames anchorNames;
    Style::NameScope anchorScope;
    Style::PositionAnchor positionAnchor;
    Style::PositionArea positionArea;
    Style::PositionTryFallbacks positionTryFallbacks;
    std::optional<size_t> usedPositionOptionIndex;

    Style::BlockStepSize blockStepSize;
    PREFERRED_TYPE(BlockStepAlign) unsigned blockStepAlign : 2;
    PREFERRED_TYPE(BlockStepInsert) unsigned blockStepInsert : 2;
    PREFERRED_TYPE(BlockStepRound) unsigned blockStepRound : 2;

    PREFERRED_TYPE(OverscrollBehavior) unsigned overscrollBehaviorX : 2;
    PREFERRED_TYPE(OverscrollBehavior) unsigned overscrollBehaviorY : 2;

    PREFERRED_TYPE(TransformStyle3D) unsigned transformStyle3D : 2;
    PREFERRED_TYPE(bool) unsigned transformStyleForcedToFlat : 1; // The used value for transform-style is forced to flat by a grouping property.
    PREFERRED_TYPE(BackfaceVisibility) unsigned backfaceVisibility : 1;

    PREFERRED_TYPE(Style::ScrollBehavior) unsigned scrollBehavior : 1;
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
    PREFERRED_TYPE(Style::PositionVisibility) unsigned positionVisibility : 3;
    PREFERRED_TYPE(FieldSizing) unsigned fieldSizing : 1;
    PREFERRED_TYPE(bool) unsigned nativeAppearanceDisabled : 1;
#if HAVE(CORE_MATERIAL)
    PREFERRED_TYPE(AppleVisualEffect) unsigned appleVisualEffect : 5;
#endif
    PREFERRED_TYPE(Style::ScrollbarWidth) unsigned scrollbarWidth : 2;
    PREFERRED_TYPE(bool) unsigned usesAnchorFunctions : 1;
    PREFERRED_TYPE(EnumSet<BoxAxis>) unsigned anchorFunctionScrollCompensatedAxes : 2;
    PREFERRED_TYPE(bool) unsigned usesTreeCountingFunctions : 1;
    PREFERRED_TYPE(bool) unsigned isPopoverInvoker : 1;
    PREFERRED_TYPE(bool) unsigned useSVGZoomRulesForLength : 1;
    PREFERRED_TYPE(Style::MarginTrim) unsigned marginTrim : 4;
    PREFERRED_TYPE(Style::Contain) unsigned contain : 5;

private:
    StyleRareNonInheritedData();
    StyleRareNonInheritedData(const StyleRareNonInheritedData&);
};

} // namespace WebCore
