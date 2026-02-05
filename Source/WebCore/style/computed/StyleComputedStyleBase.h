/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

namespace WebCore {

class AutosizeStatus;
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
class RenderElement;
class RenderStyle;
class RenderStyleBase;
class RenderStyleProperties;
class ScrollTimeline;
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

struct BorderData;
struct BorderValue;
struct CSSPropertiesBitSet;
struct CounterDirectiveMap;
struct GridTrackList;
struct OutlineValue;
struct TransformOperationData;

template<typename> class RectEdges;
template<typename> class RectCorners;
template<typename> struct MinimallySerializingSpaceSeparatedRectEdges;
template<typename> struct MinimallySerializingSpaceSeparatedSize;

using IntOutsets = RectEdges<int>;

namespace Style {
class ChangedAnimatablePropertiesFunctions;
class CustomProperty;
class CustomPropertyData;
class CustomPropertyRegistry;
class DifferenceFunctions;
class InheritedData;
class NonInheritedData;
class InheritedRareData;
class SVGData;

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
struct CaretColor;
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
struct CounterIncrement;
struct CounterReset;
struct CounterSet;
struct Cursor;
struct DynamicRangeLimit;
struct Filter;
struct FlexBasis;
struct FlowTolerance;
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
struct Zoom;
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

template<typename> struct ColorPropertyTraits;
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

constexpr auto PublicPseudoIDBits = 18;
constexpr auto TextDecorationLineBits = 5;
constexpr auto TextTransformBits = 6;
constexpr auto PseudoElementTypeBits = 5;

using PseudoStyleCache = HashMap<PseudoElementIdentifier, std::unique_ptr<RenderStyle>>;

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ComputedStyleBase);
class ComputedStyleBase : public CanMakeCheckedPtr<ComputedStyleBase, WTF::DefaultedOperatorEqual::No, WTF::CheckedPtrDeleteCheckException::Yes> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(ComputedStyleBase, ComputedStyleBase);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ComputedStyleBase);
public:
    enum CloneTag { Clone };
    enum CreateDefaultStyleTag { CreateDefaultStyle };

    WEBCORE_EXPORT ~ComputedStyleBase();

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionHasBegun() const { return m_deletionHasBegun; }
#endif

    inline bool usesViewportUnits() const;
    inline void setUsesViewportUnits();

    inline bool usesContainerUnits() const;
    inline void setUsesContainerUnits();

    inline bool useTreeCountingFunctions() const;
    inline void setUsesTreeCountingFunctions();

    inline InsideLink insideLink() const;
    inline void setInsideLink(InsideLink);

    inline bool isLink() const;
    inline void setIsLink(bool);

    inline bool emptyState() const;
    inline void setEmptyState(bool);

    inline bool firstChildState() const;
    inline void setFirstChildState();

    inline bool lastChildState() const;
    inline void setLastChildState();

    inline bool hasExplicitlyInheritedProperties() const;
    inline void setHasExplicitlyInheritedProperties();

    inline bool disallowsFastPathInheritance() const;
    inline void setDisallowsFastPathInheritance();

    inline bool hasDisplayAffectedByAnimations() const;
    inline void setHasDisplayAffectedByAnimations();

    inline bool transformStyleForcedToFlat() const;
    inline void setTransformStyleForcedToFlat(bool);

    inline void setUsesAnchorFunctions();
    inline bool usesAnchorFunctions() const;

    inline void setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis>);
    inline EnumSet<BoxAxis> anchorFunctionScrollCompensatedAxes() const;

    inline void setIsPopoverInvoker();
    inline bool isPopoverInvoker() const;

    inline bool nativeAppearanceDisabled() const;
    inline void setNativeAppearanceDisabled(bool);

    inline bool insideDefaultButton() const;
    inline void setInsideDefaultButton(bool);

    inline bool insideSubmitButton() const;
    inline void setInsideSubmitButton(bool);

    inline OptionSet<EventListenerRegionType> eventListenerRegionTypes() const;
    inline void setEventListenerRegionTypes(OptionSet<EventListenerRegionType>);

    // No setter. Set via `ComputedStyleProperties::setBlendMode()`.
    inline bool isInSubtreeWithBlendMode() const;

    inline bool isForceHidden() const;
    inline void setIsForceHidden();

    inline bool autoRevealsWhenFound() const;
    inline void setAutoRevealsWhenFound();

    inline bool hasAttrContent() const;
    inline void setHasAttrContent();

    inline std::optional<size_t> usedPositionOptionIndex() const;
    inline void setUsedPositionOptionIndex(std::optional<size_t>);

    inline bool effectiveInert() const;
    inline void setEffectiveInert(bool);

    inline bool isEffectivelyTransparent() const; // This or any ancestor has opacity 0.
    inline void setIsEffectivelyTransparent(bool);

    // No setter. Set via `ComputedStyleProperties::setDisplay()`.
    inline constexpr DisplayType originalDisplay() const;

    // `effectiveDisplay()` getter is an alias of `ComputedStyleProperties::display()`.
    inline DisplayType effectiveDisplay() const;
    inline void setEffectiveDisplay(DisplayType);

    inline StyleAppearance usedAppearance() const;
    inline void setUsedAppearance(StyleAppearance);

    // usedContentVisibility will return ContentVisibility::Hidden in a content-visibility: hidden subtree (overriding
    // content-visibility: auto at all times), ContentVisibility::Auto in a content-visibility: auto subtree (when the
    // content is not user relevant and thus skipped), and ContentVisibility::Visible otherwise.
    inline ContentVisibility usedContentVisibility() const;
    inline void setUsedContentVisibility(ContentVisibility);

    // 'touch-action' behavior depends on values in ancestors. We use an additional inherited property to implement that.
    inline TouchAction usedTouchAction() const;
    inline void setUsedTouchAction(TouchAction);

    inline ZIndex usedZIndex() const;
    inline void setUsedZIndex(ZIndex);

#if HAVE(CORE_MATERIAL)
    inline AppleVisualEffect usedAppleVisualEffectForSubtree() const;
    inline void setUsedAppleVisualEffectForSubtree(AppleVisualEffect);
#endif

#if ENABLE(TEXT_AUTOSIZING)
    // MARK: - Text Autosizing

    AutosizeStatus autosizeStatus() const;
    void setAutosizeStatus(AutosizeStatus);

#endif

    // MARK: - Pseudo element/style

    inline std::optional<PseudoElementType> pseudoElementType() const;
    const AtomString& pseudoElementNameArgument() const;

    std::optional<PseudoElementIdentifier> pseudoElementIdentifier() const;
    void setPseudoElementIdentifier(std::optional<PseudoElementIdentifier>&&);

    inline bool hasAnyPublicPseudoStyles() const;
    inline bool hasPseudoStyle(PseudoElementType) const;
    inline void setHasPseudoStyles(EnumSet<PseudoElementType>);

    RenderStyle* getCachedPseudoStyle(const PseudoElementIdentifier&) const;
    RenderStyle* addCachedPseudoStyle(std::unique_ptr<RenderStyle>);

    bool hasCachedPseudoStyles() const { return !m_cachedPseudoStyles.isEmpty(); }
    const PseudoStyleCache& cachedPseudoStyles() const { return m_cachedPseudoStyles; }

    // MARK: - Custom properties

    inline const CustomPropertyData& inheritedCustomProperties() const;
    inline const CustomPropertyData& nonInheritedCustomProperties() const;
    const CustomProperty* customPropertyValue(const AtomString&) const;
    void setCustomPropertyValue(Ref<const CustomProperty>&&, bool isInherited);
    bool customPropertyValueEqual(const ComputedStyleBase&, const AtomString&) const;
    bool customPropertiesEqual(const ComputedStyleBase&) const;
    void deduplicateCustomProperties(const ComputedStyleBase&);

    // MARK: - Custom paint

    void addCustomPaintWatchProperty(const AtomString&);

    // MARK: - Zoom

    inline bool evaluationTimeZoomEnabled() const;
    inline void setEvaluationTimeZoomEnabled(bool);

    inline float deviceScaleFactor() const;
    inline void setDeviceScaleFactor(float);

    inline bool useSVGZoomRulesForLength() const;
    inline void setUseSVGZoomRulesForLength(bool);

    inline float usedZoom() const;
    inline bool setUsedZoom(float);

    inline ZoomFactor usedZoomForLength() const;

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
    inline WebkitLocale computedLocale() const;
    const LineHeight& specifiedLineHeight() const;
#if ENABLE(TEXT_AUTOSIZING)
    void setSpecifiedLineHeight(LineHeight&&);
#endif

    void setLetterSpacingFromAnimation(LetterSpacing&&);
    void setWordSpacingFromAnimation(WordSpacing&&);

    void synchronizeLetterSpacingWithFontCascade();
    void synchronizeLetterSpacingWithFontCascadeWithoutUpdate();
    void synchronizeWordSpacingWithFontCascade();
    void synchronizeWordSpacingWithFontCascadeWithoutUpdate();

    inline float usedLetterSpacing() const;
    inline float usedWordSpacing() const;

    // MARK: - Used Counter Directives

    const CounterDirectiveMap& usedCounterDirectives() const;
    void updateUsedCounterIncrementDirectives();
    void updateUsedCounterResetDirectives();
    void updateUsedCounterSetDirectives();

    // MARK: - Writing Modes

    // FIXME: Rename to something that doesn't conflict with a property name.
    // Aggregates `writing-mode`, `direction` and `text-orientation`.
    WritingMode writingMode() const
    {
        return m_inheritedFlags.writingMode;
    }

    // FIXME: *Deprecated* Deprecated due to confusion between physical inline directions and bidi / line-relative directions.
    bool isLeftToRightDirection() const
    {
        return writingMode().isBidiLTR();
    }

    // MARK: - Aggregates

    inline Animations& ensureAnimations();
    inline BackgroundLayers& ensureBackgroundLayers();
    inline MaskLayers& ensureMaskLayers();
    inline Transitions& ensureTransitions();

    inline const BorderData& border() const;
    inline const BorderValue& borderBottom() const;
    inline const BorderValue& borderLeft() const;
    inline const BorderValue& borderRight() const;
    inline const BorderValue& borderTop() const;
    inline const BorderValue& columnRule() const;
    inline const OutlineValue& outline() const;
    inline const Animations& animations() const;
    inline const BackgroundLayers& backgroundLayers() const;
    inline const BorderImage& borderImage() const;
    inline const BorderRadius& borderRadii() const;
    inline const InsetBox& insetBox() const;
    inline const MarginBox& marginBox() const;
    inline const MaskBorder& maskBorder() const;
    inline const MaskLayers& maskLayers() const;
    inline const PaddingBox& paddingBox() const;
    inline const PerspectiveOrigin& perspectiveOrigin() const;
    inline const ScrollMarginBox& scrollMarginBox() const;
    inline const ScrollPaddingBox& scrollPaddingBox() const;
    inline const ScrollTimelines& scrollTimelines() const;
    inline const TransformOrigin& transformOrigin() const;
    inline const Transitions& transitions() const;
    inline const ViewTimelines& viewTimelines() const;

    inline void setBackgroundLayers(BackgroundLayers&&);
    inline void setBorderImage(BorderImage&&);
    inline void setBorderRadius(BorderRadiusValue&&);
    inline void setBorderTop(BorderValue&&);
    inline void setBorderRight(BorderValue&&);
    inline void setBorderBottom(BorderValue&&);
    inline void setBorderLeft(BorderValue&&);
    inline void setInsetBox(InsetBox&&);
    inline void setMarginBox(MarginBox&&);
    inline void setMaskBorder(MaskBorder&&);
    inline void setMaskLayers(MaskLayers&&);
    inline void setPaddingBox(PaddingBox&&);
    inline void setPerspectiveOrigin(PerspectiveOrigin&&);
    inline void setTransformOrigin(TransformOrigin&&);

    // MARK: - Properties/descriptors that are not yet generated

    // `cursor`
    inline CursorType cursorType() const;

    // `@page size`
    inline const PageSize& pageSize() const;
    inline void setPageSize(PageSize&&);

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

        // If you add more style bits here, you will also need to update ComputedStyleBase::NonInheritedFlags::copyNonInheritedFrom().
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
        PREFERRED_TYPE(TextAlign) unsigned char textAlign : 4;
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
        PREFERRED_TYPE(WebCore::Order) unsigned char rtlOrdering : 1;

        // Color Stuff = 4 bits
        PREFERRED_TYPE(bool) unsigned char hasExplicitlySetColor : 1;
        PREFERRED_TYPE(PrintColorAdjust) unsigned char printColorAdjust : 1;
        PREFERRED_TYPE(InsideLink) unsigned char insideLink : 2;

        PREFERRED_TYPE(bool) unsigned char isZoomed : 1;

#if ENABLE(TEXT_AUTOSIZING)
        unsigned autosizeStatus : 5;
#endif
        // Total = 63 bits (fits in 8 bytes)
    };

protected:
    friend class ChangedAnimatablePropertiesFunctions;
    friend class DifferenceFunctions;
    friend class WebCore::RenderStyle;
    friend class WebCore::RenderStyleBase;
    friend class WebCore::RenderStyleProperties;

    ComputedStyleBase(ComputedStyleBase&&);
    ComputedStyleBase& operator=(ComputedStyleBase&&);

    ComputedStyleBase(CreateDefaultStyleTag);
    ComputedStyleBase(const ComputedStyleBase&, CloneTag);

    ComputedStyleBase(ComputedStyleBase&, ComputedStyleBase&&);

    const NonInheritedFlags& nonInheritedFlags() const { return m_nonInheritedFlags; }
    const NonInheritedData& nonInheritedData() const { return m_nonInheritedData; }

    const InheritedFlags& inheritedFlags() const { return m_inheritedFlags; }
    const InheritedData& inheritedData() const { return m_inheritedData; }
    const InheritedRareData& inheritedRareData() const { return m_inheritedRareData; }

    const SVGData& svgData() const { return m_svgData; }

    // Non-inherited and inherited flags
    NonInheritedFlags m_nonInheritedFlags;
    InheritedFlags m_inheritedFlags;

    // Non-inherited data
    DataRef<NonInheritedData> m_nonInheritedData;

    // Inherited data
    DataRef<InheritedRareData> m_inheritedRareData;
    DataRef<InheritedData> m_inheritedData;

    // Non-inherited and inherited data specialized to SVG
    DataRef<SVGData> m_svgData;

    // Associated pseudo styles
    PseudoStyleCache m_cachedPseudoStyles;

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool m_deletionHasBegun { false };
#endif
};

} // namespace Style
} // namespace WebCore
