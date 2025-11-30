/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
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

#include <WebCore/BoxExtents.h>
#include <WebCore/PseudoElementIdentifier.h>
#include <WebCore/StyleGridAutoFlow.h>
#include <WebCore/StylePrimitiveNumeric+Forward.h>
#include <WebCore/WritingMode.h>
#include <unicode/utypes.h>
#include <wtf/CheckedRef.h>
#include <wtf/DataRef.h>
#include <wtf/FixedVector.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class AutosizeStatus;
class BorderData;
class BorderValue;
class Color;
class Element;
class FloatPoint;
class FloatSize;
class FloatPoint3D;
class FloatRect;
class FontCascade;
class FontCascadeDescription;
class FontMetrics;
class FontSelectionValue;
class HitTestRequest;
class IntSize;
class LayoutRect;
class LayoutSize;
class LayoutUnit;
class OutlineValue;
class RenderElement;
class RenderStyle;
class SVGRenderStyle;
class ScrollTimeline;
class StyleInheritedData;
class StyleNonInheritedData;
class StyleRareInheritedData;
class TransformationMatrix;
class ViewTimeline;

enum CSSPropertyID : uint16_t;

enum class AlignmentBaseline : uint8_t;
enum class ApplePayButtonStyle : uint8_t;
enum class ApplePayButtonType : uint8_t;
enum class AppleVisualEffect : uint8_t;
enum class BackfaceVisibility : uint8_t;
enum class BlendMode : uint8_t;
enum class FlowDirection : uint8_t;
enum class BlockStepAlign : uint8_t;
enum class BlockStepInsert : uint8_t;
enum class BlockStepRound : uint8_t;
enum class BorderCollapse : bool;
enum class BorderStyle : uint8_t;
enum class BoxAlignment : uint8_t;
enum class BoxDecorationBreak : bool;
enum class BoxDirection : bool;
enum class BoxLines : bool;
enum class BoxOrient : bool;
enum class BoxPack : uint8_t;
enum class BoxSizing : bool;
enum class BreakBetween : uint8_t;
enum class BreakInside : uint8_t;
enum class BufferedRendering : uint8_t;
enum class CaptionSide : uint8_t;
enum class Clear : uint8_t;
enum class ColumnAxis : uint8_t;
enum class ColumnFill : bool;
enum class ColumnProgression : bool;
enum class ColumnSpan : bool;
enum class CompositeOperator : uint8_t;
enum class ContainerType : uint8_t;
enum class ContentDistribution : uint8_t;
enum class ContentPosition : uint8_t;
enum class ContentVisibility : uint8_t;
enum class CursorType : uint8_t;
enum class CursorVisibility : bool;
enum class DisplayType : uint8_t;
enum class DominantBaseline : uint8_t;
enum class EmptyCell : bool;
enum class EventListenerRegionType : uint64_t;
enum class FieldSizing : bool;
enum class FillAttachment : uint8_t;
enum class FillBox : uint8_t;
enum class FillSizeType : uint8_t;
enum class FlexDirection : uint8_t;
enum class FlexWrap : uint8_t;
enum class Float : uint8_t;
enum class FontOpticalSizing : bool;
enum class FontOrientation : bool;
enum class FontSmoothingMode : uint8_t;
enum class FontSynthesisLonghandValue : bool;
enum class FontVariantCaps : uint8_t;
enum class FontVariantEmoji : uint8_t;
enum class FontVariantPosition : uint8_t;
enum class Hyphens : uint8_t;
enum class ImageRendering : uint8_t;
enum class InputSecurity : bool;
enum class InsideLink : uint8_t;
enum class Isolation : bool;
enum class ItemPosition : uint8_t;
enum class Kerning : uint8_t;
enum class LineAlign : bool;
enum class LineBreak : uint8_t;
enum class LineCap : uint8_t;
enum class LineJoin : uint8_t;
enum class LineSnap : uint8_t;
enum class ListStylePosition : bool;
enum class MarqueeBehavior : uint8_t;
enum class MarqueeDirection : uint8_t;
enum class MaskType : uint8_t;
enum class MathShift : bool;
enum class MathStyle : bool;
enum class NBSPMode : bool;
enum class NinePieceImageRule : uint8_t;
enum class NonCJKGlyphOrientation : bool;
enum class ObjectFit : uint8_t;
enum class Order : bool;
enum class OutlineStyle : uint8_t;
enum class Overflow : uint8_t;
enum class OverflowAnchor : bool;
enum class OverflowContinue : bool;
enum class OverflowWrap : uint8_t;
enum class OverscrollBehavior : uint8_t;
enum class PaginationMode : uint8_t;
enum class PaintBehavior : uint32_t;
enum class PointerEvents : uint8_t;
enum class PositionType : uint8_t;
enum class PrintColorAdjust : bool;
enum class PseudoId : uint8_t;
enum class RubyPosition : uint8_t;
enum class RubyAlign : uint8_t;
enum class RubyOverhang : bool;
enum class ScrollAxis : uint8_t;
enum class ScrollSnapStop : bool;
enum class StyleAppearance : uint8_t;
enum class StyleColorOptions : uint8_t;
enum class StyleDifference : uint8_t;
enum class StyleDifferenceContextSensitiveProperty : uint8_t;
enum class TableLayoutType : bool;
enum class TextBoxTrim : uint8_t;
enum class TextCombine : bool;
enum class TextDecorationSkipInk : uint8_t;
enum class TextDecorationStyle : uint8_t;
enum class TextGroupAlign : uint8_t;
enum class TextJustify : uint8_t;
enum class TextOverflow : bool;
enum class TextRenderingMode : uint8_t;
enum class TextSecurity : uint8_t;
enum class TextTransform : uint8_t;
enum class TextWrapMode : bool;
enum class TextWrapStyle : uint8_t;
enum class TextZoom : bool;
enum class TransformBox : uint8_t;
enum class TransformStyle3D : uint8_t;
enum class UnicodeBidi : uint8_t;
enum class UsedClear : uint8_t;
enum class UsedFloat : uint8_t;
enum class UserDrag : uint8_t;
enum class UserModify : uint8_t;
enum class UserSelect : uint8_t;
enum class VectorEffect : uint8_t;
enum class Visibility : uint8_t;
enum class WhiteSpace : uint8_t;
enum class WhiteSpaceCollapse : uint8_t;
enum class WindRule : bool;
enum class WordBreak : uint8_t;

struct CSSPropertiesBitSet;
struct CounterDirectiveMap;
struct GridTrackList;
struct TransformOperationData;

template<typename> class RectEdges;
template<typename> class RectCorners;
template<typename> struct MinimallySerializingSpaceSeparatedRectEdges;
template<typename> struct MinimallySerializingSpaceSeparatedSize;

using IntOutsets = RectEdges<int>;

namespace Style {
class CustomProperty;
class CustomPropertyData;
class CustomPropertyRegistry;

struct AccentColor;
struct AlignContent;
struct AlignItems;
struct AlignSelf;
struct Animation;
struct AnchorNames;
struct AppleColorFilter;
struct AspectRatio;
struct BackgroundLayer;
struct BackgroundSize;
struct BlockEllipsis;
struct BlockStepSize;
struct BorderImage;
struct BorderImageOutset;
struct BorderImageRepeat;
struct BorderImageSlice;
struct BorderImageSource;
struct BorderImageWidth;
struct BorderRadius;
struct BoxShadow;
struct Clip;
struct ClipPath;
struct Color;
struct ColorScheme;
struct ColumnCount;
struct ColumnWidth;
struct Contain;
struct ContainIntrinsicSize;
struct ContainerNames;
struct Content;
struct CornerShapeValue;
struct Cursor;
struct DynamicRangeLimit;
struct Filter;
struct FlexBasis;
struct FontFamilies;
struct FontFamiliesView;
struct FontFeatureSettings;
struct FontPalette;
struct FontSizeAdjust;
struct FontStyle;
struct FontVariantAlternates;
struct FontVariantEastAsian;
struct FontVariantLigatures;
struct FontVariantNumeric;
struct FontVariationSettings;
struct FontWeight;
struct FontWidth;
struct GapGutter;
struct GridPosition;
struct GridTemplateAreas;
struct GridTemplateList;
struct GridTrackSizes;
struct HangingPunctuation;
struct HyphenateCharacter;
struct HyphenateLimitEdge;
struct HyphenateLimitLines;
struct ImageOrNone;
struct InsetEdge;
struct ItemTolerance;
struct JustifyContent;
struct JustifyItems;
struct JustifySelf;
struct LetterSpacing;
struct LineHeight;
struct LineWidth;
struct LineFitEdge;
struct ListStyleType;
struct MarginEdge;
struct MarginTrim;
struct MaskBorder;
struct MaskBorderOutset;
struct MaskBorderRepeat;
struct MaskBorderSlice;
struct MaskBorderSource;
struct MaskBorderWidth;
struct MaskLayer;
struct MathDepth;
struct MaximumLines;
struct MaximumSize;
struct MinimumSize;
struct NameScope;
struct OffsetAnchor;
struct OffsetDistance;
struct OffsetPath;
struct OffsetPosition;
struct OffsetRotate;
struct Opacity;
struct Orphans;
struct PaddingEdge;
struct PageSize;
struct Perspective;
struct Position;
struct PositionAnchor;
struct PositionArea;
struct PositionVisibility;
struct PositionX;
struct PositionY;
struct PositionTryFallbacks;
struct PreferredSize;
struct ProgressTimelineAxes;
struct ProgressTimelineNames;
struct Quotes;
struct RepeatStyle;
struct Rotate;
struct SVGBaselineShift;
struct SVGCenterCoordinateComponent;
struct SVGCoordinateComponent;
struct SVGMarkerResource;
struct SVGPaint;
struct SVGPaintOrder;
struct SVGPathData;
struct SVGRadius;
struct SVGRadiusComponent;
struct SVGStrokeDasharray;
struct SVGStrokeDashoffset;
struct Scale;
struct ScopedName;
struct ScrollMarginEdge;
struct ScrollPaddingEdge;
struct ScrollSnapAlign;
struct ScrollSnapType;
struct ScrollTimelines;
struct ScrollbarColor;
struct ScrollbarGutter;
struct ShapeMargin;
struct ShapeOutside;
struct SpeakAs;
struct StrokeMiterlimit;
struct StrokeWidth;
struct TabSize;
struct TextAutospace;
struct TextBoxEdge;
struct TextDecorationLine;
struct TextDecorationThickness;
struct TextEmphasisPosition;
struct TextEmphasisStyle;
struct TextIndent;
struct TextShadow;
struct TextSizeAdjust;
struct TextSpacingTrim;
struct TextTransform;
struct TextUnderlineOffset;
struct TextUnderlinePosition;
struct TouchAction;
struct Transform;
struct TransformOrigin;
struct Transition;
struct Translate;
struct VerticalAlign;
struct ViewTimelineInsets;
struct ViewTimelines;
struct ViewTransitionClasses;
struct ViewTransitionName;
struct WebkitBoxReflect;
struct WebkitInitialLetter;
struct WebkitLineBoxContain;
struct WebkitLineClamp;
struct WebkitLineGrid;
struct WebkitLocale;
struct WebkitMarqueeIncrement;
struct WebkitMarqueeRepetition;
struct WebkitMarqueeSpeed;
struct WebkitTextStrokeWidth;
struct Widows;
struct WillChange;
struct WordSpacing;
struct ZIndex;
struct ZoomFactor;

enum class Change : uint8_t;
enum class GridTrackSizingDirection : bool;
enum class ImageOrientation : bool;
enum class PositionTryOrder : uint8_t;
enum class Resize : uint8_t;
enum class SVGGlyphOrientationHorizontal : uint8_t;
enum class SVGGlyphOrientationVertical : uint8_t;
enum class ScrollBehavior : bool;
enum class ScrollbarWidth : uint8_t;
enum class TextAlignLast : uint8_t;
enum class TextAlign : uint8_t;
enum class WebkitOverflowScrolling : bool;
enum class WebkitTouchCallout : bool;

template<typename> struct CoordinatedValueList;
template<typename> struct Shadows;

using Animations = CoordinatedValueList<Animation>;
using BackgroundLayers = CoordinatedValueList<BackgroundLayer>;
using BorderRadiusValue = MinimallySerializingSpaceSeparatedSize<LengthPercentage<CSS::Nonnegative>>;
using BoxShadows = Shadows<BoxShadow>;
using FlexGrow = Number<CSS::Nonnegative, float>;
using FlexShrink = Number<CSS::Nonnegative, float>;
using InsetBox = MinimallySerializingSpaceSeparatedRectEdges<InsetEdge>;
using LineWidthBox = MinimallySerializingSpaceSeparatedRectEdges<LineWidth>;
using MarginBox = MinimallySerializingSpaceSeparatedRectEdges<MarginEdge>;
using MaskLayers = CoordinatedValueList<MaskLayer>;
using ObjectPosition = Position;
using Order = Integer<>;
using PaddingBox = MinimallySerializingSpaceSeparatedRectEdges<PaddingEdge>;
using PerspectiveOrigin = Position;
using PerspectiveOriginX = PositionX;
using PerspectiveOriginY = PositionY;
using ScrollMarginBox = MinimallySerializingSpaceSeparatedRectEdges<ScrollMarginEdge>;
using ScrollPaddingBox = MinimallySerializingSpaceSeparatedRectEdges<ScrollPaddingEdge>;
using ShapeImageThreshold = Number<CSS::ClosedUnitRangeClampBoth, float>;
using TextShadows = Shadows<TextShadow>;
using TransformOriginX = PositionX;
using TransformOriginXY = Position;
using TransformOriginY = PositionY;
using TransformOriginZ = Length<>;
using Transitions = CoordinatedValueList<Transition>;
using WebkitBorderSpacing = Length<CSS::NonnegativeUnzoomed>;
using WebkitBoxFlex = Number<CSS::All, float>;
using WebkitBoxFlexGroup = Integer<CSS::Nonnegative>;
using WebkitBoxOrdinalGroup = Integer<CSS::Positive>;
}

constexpr auto PublicPseudoIDBits = 17;
constexpr auto TextDecorationLineBits = 5;
constexpr auto TextTransformBits = 6;
constexpr auto PseudoElementTypeBits = 5;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PseudoStyleCache);
struct PseudoStyleCache {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(PseudoStyleCache, PseudoStyleCache);
    HashMap<Style::PseudoElementIdentifier, std::unique_ptr<RenderStyle>> styles;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyleBase);
class RenderStyleBase : public CanMakeCheckedPtr<RenderStyleBase, WTF::DefaultedOperatorEqual::No, WTF::CheckedPtrDeleteCheckException::Yes> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(RenderStyleBase, RenderStyleBase);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderStyleBase);
public:
    enum CloneTag { Clone };
    enum CreateDefaultStyleTag { CreateDefaultStyle };

    WEBCORE_EXPORT ~RenderStyleBase();

    // MARK: - Zoom

    inline bool evaluationTimeZoomEnabled() const;
    void setEvaluationTimeZoomEnabled(bool);

    inline float deviceScaleFactor() const;
    void setDeviceScaleFactor(float);

    inline bool useSVGZoomRulesForLength() const;
    void setUseSVGZoomRulesForLength(bool);

    inline float usedZoom() const;
    inline bool setUsedZoom(float);
    inline Style::ZoomFactor usedZoomForLength() const;

    // MARK: - Fonts

    inline const FontCascade& fontCascade() const;
    CheckedRef<const FontCascade> checkedFontCascade() const;
    WEBCORE_EXPORT FontCascade& mutableFontCascadeWithoutUpdate();
    void setFontCascade(FontCascade&&);

    WEBCORE_EXPORT const FontCascadeDescription& fontDescription() const;
    WEBCORE_EXPORT FontCascadeDescription& mutableFontDescriptionWithoutUpdate();
    WEBCORE_EXPORT void setFontDescription(FontCascadeDescription&&);
    bool setFontDescriptionWithoutUpdate(FontCascadeDescription&&);

    WEBCORE_EXPORT const FontMetrics& metricsOfPrimaryFont() const;
    std::pair<FontOrientation, NonCJKGlyphOrientation> fontAndGlyphOrientation();
    float computedFontSize() const;

    const Style::LineHeight& specifiedLineHeight() const;
#if ENABLE(TEXT_AUTOSIZING)
    void setSpecifiedLineHeight(Style::LineHeight&&);
#endif

    constexpr WritingMode writingMode() const { return m_inheritedFlags.writingMode; } // FIXME: Rename to something that doesn't conflict with a property name.
    CursorType cursorType() const { return static_cast<CursorType>(m_inheritedFlags.cursorType); }

    // MARK: Aggregates

    inline Style::Animations& ensureAnimations();
    inline Style::BackgroundLayers& ensureBackgroundLayers();
    inline Style::MaskLayers& ensureMaskLayers();
    inline Style::Transitions& ensureTransitions();

    inline const BorderData& border() const;
    inline const BorderValue& borderBottom() const;
    inline const BorderValue& borderLeft() const;
    inline const BorderValue& borderRight() const;
    inline const BorderValue& borderTop() const;
    inline const BorderValue& columnRule() const;
    inline const OutlineValue& outline() const;
    inline const Style::Animations& animations() const;
    inline const Style::BackgroundLayers& backgroundLayers() const;
    inline const Style::BorderImage& borderImage() const;
    inline const Style::BorderRadius& borderRadii() const;
    inline const Style::InsetBox& insetBox() const;
    inline const Style::MarginBox& marginBox() const;
    inline const Style::MaskBorder& maskBorder() const;
    inline const Style::MaskLayers& maskLayers() const;
    inline const Style::PaddingBox& paddingBox() const;
    inline const Style::PerspectiveOrigin& perspectiveOrigin() const;
    inline const Style::ScrollMarginBox& scrollMarginBox() const;
    inline const Style::ScrollPaddingBox& scrollPaddingBox() const;
    inline const Style::ScrollTimelines& scrollTimelines() const;
    inline const Style::TransformOrigin& transformOrigin() const;
    inline const Style::Transitions& transitions() const;
    inline const Style::ViewTimelines& viewTimelines() const;
    inline Style::LineWidthBox borderWidth() const;

    inline void setBackgroundLayers(Style::BackgroundLayers&&);
    inline void setBorderImage(Style::BorderImage&&);
    inline void setBorderRadius(Style::BorderRadiusValue&&);
    inline void setInsetBox(Style::InsetBox&&);
    inline void setMarginBox(Style::MarginBox&&);
    inline void setMaskBorder(Style::MaskBorder&&);
    inline void setMaskLayers(Style::MaskLayers&&);
    inline void setPaddingBox(Style::PaddingBox&&);
    inline void setPerspectiveOrigin(Style::PerspectiveOrigin&&);
    inline void setTransformOrigin(Style::TransformOrigin&&);

    // MARK: - Properties/descriptors that are not yet generated

    // `caret-color`
    inline const Style::Color& caretColor() const;
    inline const Style::Color& visitedLinkCaretColor() const;
    inline bool hasAutoCaretColor() const;
    inline bool hasVisitedLinkAutoCaretColor() const;
    inline void setCaretColor(Style::Color&&);
    inline void setVisitedLinkCaretColor(Style::Color&&);
    inline void setHasAutoCaretColor();
    inline void setHasVisitedLinkAutoCaretColor();

    // `counter-*`
    const CounterDirectiveMap& counterDirectives() const;
    CounterDirectiveMap& accessCounterDirectives();

    // `@page size`
    inline const Style::PageSize& pageSize() const;
    inline void setPageSize(Style::PageSize&&);

protected:
    RenderStyleBase(RenderStyleBase&&);
    RenderStyleBase& operator=(RenderStyleBase&&);

    RenderStyleBase(CreateDefaultStyleTag);
    RenderStyleBase(const RenderStyleBase&, CloneTag);

    RenderStyleBase(RenderStyleBase&, RenderStyleBase&&);

    struct NonInheritedFlags {
        bool operator==(const NonInheritedFlags&) const = default;

        inline void copyNonInheritedFrom(const NonInheritedFlags&);

        inline bool hasAnyPublicPseudoStyles() const;
        bool hasPseudoStyle(PseudoElementType) const;
        void setHasPseudoStyles(EnumSet<PseudoElementType>);

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const NonInheritedFlags&) const;
#endif

        PREFERRED_TYPE(DisplayType) unsigned effectiveDisplay : 5;
        PREFERRED_TYPE(DisplayType) unsigned originalDisplay : 5;
        PREFERRED_TYPE(Overflow) unsigned overflowX : 3;
        PREFERRED_TYPE(Overflow) unsigned overflowY : 3;
        PREFERRED_TYPE(Clear) unsigned clear : 3;
        PREFERRED_TYPE(PositionType) unsigned position : 3;
        PREFERRED_TYPE(UnicodeBidi) unsigned unicodeBidi : 3;
        PREFERRED_TYPE(Float) unsigned floating : 3;

        PREFERRED_TYPE(bool) unsigned usesViewportUnits : 1;
        PREFERRED_TYPE(bool) unsigned usesContainerUnits : 1;
        PREFERRED_TYPE(bool) unsigned useTreeCountingFunctions : 1;
        PREFERRED_TYPE(bool) unsigned hasExplicitlyInheritedProperties : 1; // Explicitly inherits a non-inherited property.
        PREFERRED_TYPE(bool) unsigned disallowsFastPathInheritance : 1;

        // Non-property related state bits.
        PREFERRED_TYPE(bool) unsigned emptyState : 1;
        PREFERRED_TYPE(bool) unsigned firstChildState : 1;
        PREFERRED_TYPE(bool) unsigned lastChildState : 1;
        PREFERRED_TYPE(bool) unsigned isLink : 1;
        PREFERRED_TYPE(PseudoElementType) unsigned pseudoElementType : PseudoElementTypeBits;
        unsigned pseudoBits : PublicPseudoIDBits;
        unsigned textDecorationLine : TextDecorationLineBits; // Text decorations defined *only* by this element. PREFERRED_TYPE elided to avoid header inclusion.

        // If you add more style bits here, you will also need to update RenderStyleBase::NonInheritedFlags::copyNonInheritedFrom().
    };

    struct InheritedFlags {
        bool operator==(const InheritedFlags&) const = default;

#if !LOG_DISABLED
        void dumpDifferences(TextStream&, const InheritedFlags&) const;
#endif

        // Writing Mode = 8 bits (can be packed into 6 if needed)
        WritingMode writingMode;

        // Text Formatting = 19 bits aligned onto 2 bytes + 4 trailing bits
        PREFERRED_TYPE(WhiteSpaceCollapse) unsigned char whiteSpaceCollapse : 3;
        PREFERRED_TYPE(TextWrapMode) unsigned char textWrapMode : 1;
        PREFERRED_TYPE(Style::TextAlign) unsigned char textAlign : 4;
        PREFERRED_TYPE(TextWrapStyle) unsigned char textWrapStyle : 2;
        unsigned char textTransform : TextTransformBits; // PREFERRED_TYPE elided to avoid header inclusion.
        unsigned char : 1; // byte alignment
        unsigned char textDecorationLineInEffect : TextDecorationLineBits; // PREFERRED_TYPE elided to avoid header inclusion.

        // Cursors and Visibility = 13 bits aligned onto 4 bits + 1 byte + 1 bit
        PREFERRED_TYPE(PointerEvents) unsigned char pointerEvents : 4;
        PREFERRED_TYPE(Visibility) unsigned char visibility : 2;
        PREFERRED_TYPE(CursorType) unsigned char cursorType : 6;
#if ENABLE(CURSOR_VISIBILITY)
        PREFERRED_TYPE(CursorVisibility) unsigned char cursorVisibility : 1;
#endif

        // Display Type-Specific = 5 bits
        PREFERRED_TYPE(ListStylePosition) unsigned char listStylePosition : 1;
        PREFERRED_TYPE(EmptyCell) unsigned char emptyCells : 1;
        PREFERRED_TYPE(BorderCollapse) unsigned char borderCollapse : 1;
        PREFERRED_TYPE(CaptionSide) unsigned char captionSide : 2;

        // -webkit- Stuff = 2 bits
        PREFERRED_TYPE(BoxDirection) unsigned char boxDirection : 1;
        PREFERRED_TYPE(Order) unsigned char rtlOrdering : 1;

        // Color Stuff = 4 bits
        PREFERRED_TYPE(bool) unsigned char hasExplicitlySetColor : 1;
        PREFERRED_TYPE(PrintColorAdjust) unsigned char printColorAdjust : 1;
        PREFERRED_TYPE(InsideLink) unsigned char insideLink : 2;

#if ENABLE(TEXT_AUTOSIZING)
        unsigned autosizeStatus : 5;
#endif
        // Total = 56 bits (fits in 8 bytes)
    };

    // non-inherited attributes
    DataRef<StyleNonInheritedData> m_nonInheritedData;
    NonInheritedFlags m_nonInheritedFlags;

    // inherited attributes
    DataRef<StyleRareInheritedData> m_rareInheritedData;
    DataRef<StyleInheritedData> m_inheritedData;
    InheritedFlags m_inheritedFlags;

    // list of associated pseudo styles
    std::unique_ptr<PseudoStyleCache> m_cachedPseudoStyles;

    DataRef<SVGRenderStyle> m_svgStyle;

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool m_deletionHasBegun { false };
#endif
};

} // namespace WebCore
