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

#include <WebCore/RenderStyleProperties.h>

namespace WebCore {

class RenderStyle final : public RenderStyleProperties {
public:
    RenderStyle(RenderStyle&&);
    RenderStyle& operator=(RenderStyle&&);

    explicit RenderStyle(CreateDefaultStyleTag);
    RenderStyle(const RenderStyle&, CloneTag);

    RenderStyle replace(RenderStyle&&) WARN_UNUSED_RETURN;

    static RenderStyle& defaultStyleSingleton();

    WEBCORE_EXPORT static RenderStyle create();
    static std::unique_ptr<RenderStyle> createPtr();
    static std::unique_ptr<RenderStyle> createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry&);

    static RenderStyle clone(const RenderStyle&);
    static RenderStyle cloneIncludingPseudoElements(const RenderStyle&);
    static std::unique_ptr<RenderStyle> clonePtr(const RenderStyle&);

    static RenderStyle createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType);
    static RenderStyle createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle);

#if ASSERT_ENABLED || ENABLE(SECURITY_ASSERTIONS)
    bool deletionHasBegun() const { return m_deletionHasBegun; }
#endif

    bool operator==(const RenderStyle&) const;

    void inheritFrom(const RenderStyle&);
    void inheritIgnoringCustomPropertiesFrom(const RenderStyle&);
    void inheritUnicodeBidiFrom(const RenderStyle* parent) { m_nonInheritedFlags.unicodeBidi = parent->m_nonInheritedFlags.unicodeBidi; }
    inline void inheritColumnPropertiesFrom(const RenderStyle& parent);
    void fastPathInheritFrom(const RenderStyle&);
    void copyNonInheritedFrom(const RenderStyle&);
    void copyContentFrom(const RenderStyle&);
    void copyPseudoElementsFrom(const RenderStyle&);
    void copyPseudoElementBitsFrom(const RenderStyle&);

    bool scrollAnchoringSuppressionStyleDidChange(const RenderStyle*) const;
    bool outOfFlowPositionStyleDidChange(const RenderStyle*) const;

    void setColumnStylesFromPaginationMode(PaginationMode);

    std::optional<PseudoElementType> pseudoElementType() const;
    const AtomString& pseudoElementNameArgument() const;

    std::optional<Style::PseudoElementIdentifier> pseudoElementIdentifier() const;
    void setPseudoElementIdentifier(std::optional<Style::PseudoElementIdentifier>&&);

    RenderStyle* getCachedPseudoStyle(const Style::PseudoElementIdentifier&) const;
    RenderStyle* addCachedPseudoStyle(std::unique_ptr<RenderStyle>);

    bool hasCachedPseudoStyles() const { return m_cachedPseudoStyles && m_cachedPseudoStyles->styles.size(); }
    const PseudoStyleCache* cachedPseudoStyles() const { return m_cachedPseudoStyles.get(); }

    inline const Style::CustomPropertyData& inheritedCustomProperties() const;
    inline const Style::CustomPropertyData& nonInheritedCustomProperties() const;
    const Style::CustomProperty* customPropertyValue(const AtomString&) const;
    bool customPropertyValueEqual(const RenderStyle&, const AtomString&) const;

    void deduplicateCustomProperties(const RenderStyle&);
    void setCustomPropertyValue(Ref<const Style::CustomProperty>&&, bool isInherited);
    bool customPropertiesEqual(const RenderStyle&) const;

    void addCustomPaintWatchProperty(const AtomString&);

    void setUsesViewportUnits() { m_nonInheritedFlags.usesViewportUnits = true; }
    bool usesViewportUnits() const { return m_nonInheritedFlags.usesViewportUnits; }
    void setUsesContainerUnits() { m_nonInheritedFlags.usesContainerUnits = true; }
    bool usesContainerUnits() const { return m_nonInheritedFlags.usesContainerUnits; }
    void setUsesTreeCountingFunctions() { m_nonInheritedFlags.useTreeCountingFunctions = true; }
    bool useTreeCountingFunctions() const { return m_nonInheritedFlags.useTreeCountingFunctions; }
    void setUsesAnchorFunctions();
    bool usesAnchorFunctions() const;
    void setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis>);
    EnumSet<BoxAxis> anchorFunctionScrollCompensatedAxes() const;

    void setIsPopoverInvoker();
    bool isPopoverInvoker() const;

    inline bool nativeAppearanceDisabled() const;
    inline void setNativeAppearanceDisabled(bool);
    static bool initialNativeAppearanceDisabled() { return false; }

    inline bool isInSubtreeWithBlendMode() const;

    inline bool insideDefaultButton() const;
    inline void setInsideDefaultButton(bool);

    inline bool insideSubmitButton() const;
    inline void setInsideSubmitButton(bool);

    InsideLink insideLink() const { return static_cast<InsideLink>(m_inheritedFlags.insideLink); }
    void setInsideLink(InsideLink insideLink) { m_inheritedFlags.insideLink = static_cast<unsigned>(insideLink); }

    bool isLink() const { return m_nonInheritedFlags.isLink; }
    void setIsLink(bool v) { m_nonInheritedFlags.isLink = v; }

    bool emptyState() const { return m_nonInheritedFlags.emptyState; }
    void setEmptyState(bool v) { m_nonInheritedFlags.emptyState = v; }

    bool firstChildState() const { return m_nonInheritedFlags.firstChildState; }
    void setFirstChildState() { m_nonInheritedFlags.firstChildState = true; }

    bool lastChildState() const { return m_nonInheritedFlags.lastChildState; }
    void setLastChildState() { m_nonInheritedFlags.lastChildState = true; }

    void setHasExplicitlyInheritedProperties() { m_nonInheritedFlags.hasExplicitlyInheritedProperties = true; }
    bool hasExplicitlyInheritedProperties() const { return m_nonInheritedFlags.hasExplicitlyInheritedProperties; }

    bool disallowsFastPathInheritance() const { return m_nonInheritedFlags.disallowsFastPathInheritance; }
    void setDisallowsFastPathInheritance() { m_nonInheritedFlags.disallowsFastPathInheritance = true; }

    inline OptionSet<EventListenerRegionType> eventListenerRegionTypes() const;
    inline void setEventListenerRegionTypes(OptionSet<EventListenerRegionType>);

    inline void setIsForceHidden();
    inline bool isForceHidden() const;

    inline void setAutoRevealsWhenFound();
    inline bool autoRevealsWhenFound() const;

    inline bool hasAttrContent() const;
    inline void setHasAttrContent();

    inline void setEffectiveInert(bool);
    inline void setIsEffectivelyTransparent(bool);

    std::optional<size_t> usedPositionOptionIndex() const;
    void setUsedPositionOptionIndex(std::optional<size_t>);

#if ENABLE(TEXT_AUTOSIZING)
    AutosizeStatus autosizeStatus() const;
    void setAutosizeStatus(AutosizeStatus);

    uint32_t hashForTextAutosizing() const;
    bool equalForTextAutosizing(const RenderStyle&) const;

    bool isIdempotentTextAutosizingCandidate() const;
    bool isIdempotentTextAutosizingCandidate(AutosizeStatus overrideStatus) const;
#endif

    LayoutBoxExtent imageOutsets(const Style::BorderImage&) const;
    LayoutBoxExtent imageOutsets(const Style::MaskBorder&) const;
    inline bool hasBorderImageOutsets() const;
    inline LayoutBoxExtent borderImageOutsets() const;
    inline LayoutBoxExtent maskBorderOutsets() const;

    bool isStyleAvailable() const;

    inline bool hasAnyPublicPseudoStyles() const;
    inline bool hasPseudoStyle(PseudoElementType) const;
    inline void setHasPseudoStyles(EnumSet<PseudoElementType>);

    inline bool hasDisplayAffectedByAnimations() const;
    inline void setHasDisplayAffectedByAnimations();

    // Whether or not a positioned element requires normal flow x/y to be computed to determine its position.
    inline bool hasStaticInlinePosition(bool horizontal) const;
    inline bool hasStaticBlockPosition(bool horizontal) const;

    inline bool hasOutOfFlowPosition() const;
    inline bool hasInFlowPosition() const;
    inline bool hasViewportConstrainedPosition() const;

    bool inheritedEqual(const RenderStyle&) const;
    bool nonInheritedEqual(const RenderStyle&) const;
    bool fastPathInheritedEqual(const RenderStyle&) const;
    bool nonFastPathInheritedEqual(const RenderStyle&) const;
    bool descendantAffectingNonInheritedPropertiesEqual(const RenderStyle&) const;
    bool borderAndBackgroundEqual(const RenderStyle&) const;
    inline bool containerTypeAndNamesEqual(const RenderStyle&) const;
    inline bool columnSpanEqual(const RenderStyle&) const;
    inline bool scrollPaddingEqual(const RenderStyle&) const;
    inline bool fontCascadeEqual(const RenderStyle&) const;

    bool scrollSnapDataEquivalent(const RenderStyle&) const;
    inline bool borderIsEquivalentForPainting(const RenderStyle&) const;

    // MARK: - Diffing

    StyleDifference diff(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool diffRequiresLayerRepaint(const RenderStyle&, bool isComposited) const;
    void conservativelyCollectChangedAnimatableProperties(const RenderStyle&, CSSPropertiesBitSet&) const;

#if !LOG_DISABLED
    void dumpDifferences(TextStream&, const RenderStyle&) const;
#endif

    // MARK: - Style adjustment utilities

    inline void addToTextDecorationLineInEffect(Style::TextDecorationLine);
    inline void containIntrinsicWidthAddAuto();
    inline void containIntrinsicHeightAddAuto();
    inline void setGridAutoFlowDirection(Style::GridAutoFlow::Direction);

    void adjustAnimations();
    void adjustTransitions();
    void adjustBackgroundLayers();
    void adjustMaskLayers();
    void adjustScrollTimelines();
    void adjustViewTimelines();

    inline void resetBorder();
    inline void resetBorderExceptRadius();
    inline void resetBorderTop();
    inline void resetBorderRight();
    inline void resetBorderBottom();
    inline void resetBorderLeft();
    inline void resetBorderImage();
    inline void resetBorderRadius();
    inline void resetBorderTopLeftRadius();
    inline void resetBorderTopRightRadius();
    inline void resetBorderBottomLeftRadius();
    inline void resetBorderBottomRightRadius();
    inline void resetMargin();
    inline void resetPadding();
    inline void resetColumnRule();
    inline void resetPageSize();

    // MARK: - Derived values

    float outlineSize() const; // used value combining `outline-width` and `outline-offset`

    String altFromContent() const;

    const AtomString& hyphenString() const;

    WEBCORE_EXPORT float computedLineHeight() const;
    float computeLineHeight(const Style::LineHeight&) const;

    inline bool hasExplicitlySetBorderRadius() const;
    inline bool hasExplicitlySetPadding() const;

    // MARK: - Logical

    // Logical Inset
    inline const Style::InsetEdge& logicalLeft() const;
    inline const Style::InsetEdge& logicalRight() const;
    inline const Style::InsetEdge& logicalTop() const;
    inline const Style::InsetEdge& logicalBottom() const;

    // Logical Sizing
    inline const Style::PreferredSize& logicalWidth(const WritingMode) const;
    inline const Style::PreferredSize& logicalHeight(const WritingMode) const;
    inline const Style::MinimumSize& logicalMinWidth(const WritingMode) const;
    inline const Style::MinimumSize& logicalMinHeight(const WritingMode) const;
    inline const Style::MaximumSize& logicalMaxWidth(const WritingMode) const;
    inline const Style::MaximumSize& logicalMaxHeight(const WritingMode) const;
    inline const Style::PreferredSize& logicalWidth() const;
    inline const Style::PreferredSize& logicalHeight() const;
    inline const Style::MinimumSize& logicalMinWidth() const;
    inline const Style::MinimumSize& logicalMinHeight() const;
    inline const Style::MaximumSize& logicalMaxWidth() const;
    inline const Style::MaximumSize& logicalMaxHeight() const;
    inline void setLogicalWidth(Style::PreferredSize&&);
    inline void setLogicalHeight(Style::PreferredSize&&);
    inline void setLogicalMinWidth(Style::MinimumSize&&);
    inline void setLogicalMinHeight(Style::MinimumSize&&);
    inline void setLogicalMaxWidth(Style::MaximumSize&&);
    inline void setLogicalMaxHeight(Style::MaximumSize&&);

    // Logical Border
    const BorderValue& borderBefore(const WritingMode) const;
    const BorderValue& borderAfter(const WritingMode) const;
    const BorderValue& borderStart(const WritingMode) const;
    const BorderValue& borderEnd(const WritingMode) const;
    const BorderValue& borderBefore() const { return borderBefore(writingMode()); }
    const BorderValue& borderAfter() const { return borderAfter(writingMode()); }
    const BorderValue& borderStart() const { return borderStart(writingMode()); }
    const BorderValue& borderEnd() const { return borderEnd(writingMode()); }
    Style::LineWidth borderBeforeWidth(const WritingMode) const;
    Style::LineWidth borderAfterWidth(const WritingMode) const;
    Style::LineWidth borderStartWidth(const WritingMode) const;
    Style::LineWidth borderEndWidth(const WritingMode) const;
    inline Style::LineWidth borderBeforeWidth() const;
    inline Style::LineWidth borderAfterWidth() const;
    inline Style::LineWidth borderStartWidth() const;
    inline Style::LineWidth borderEndWidth() const;

    // Logical Margin
    inline const Style::MarginEdge& marginStart(const WritingMode) const;
    inline const Style::MarginEdge& marginEnd(const WritingMode) const;
    inline const Style::MarginEdge& marginBefore(const WritingMode) const;
    inline const Style::MarginEdge& marginAfter(const WritingMode) const;
    inline const Style::MarginEdge& marginBefore() const;
    inline const Style::MarginEdge& marginAfter() const;
    inline const Style::MarginEdge& marginStart() const;
    inline const Style::MarginEdge& marginEnd() const;
    void setMarginStart(Style::MarginEdge&&);
    void setMarginEnd(Style::MarginEdge&&);
    void setMarginBefore(Style::MarginEdge&&);
    void setMarginAfter(Style::MarginEdge&&);

    // Logical Padding
    inline const Style::PaddingEdge& paddingBefore(const WritingMode) const;
    inline const Style::PaddingEdge& paddingAfter(const WritingMode) const;
    inline const Style::PaddingEdge& paddingStart(const WritingMode) const;
    inline const Style::PaddingEdge& paddingEnd(const WritingMode) const;
    inline const Style::PaddingEdge& paddingBefore() const;
    inline const Style::PaddingEdge& paddingAfter() const;
    inline const Style::PaddingEdge& paddingStart() const;
    inline const Style::PaddingEdge& paddingEnd() const;
    void setPaddingStart(Style::PaddingEdge&&);
    void setPaddingEnd(Style::PaddingEdge&&);
    void setPaddingBefore(Style::PaddingEdge&&);
    void setPaddingAfter(Style::PaddingEdge&&);

    // Logical Aspect Ratio
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalWidth() const;
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalHeight() const;
    inline double logicalAspectRatio() const;
    inline BoxSizing boxSizingForAspectRatio() const;

    // Logical ContainIntrinsicSize
    inline const Style::ContainIntrinsicSize& containIntrinsicLogicalWidth() const;
    inline const Style::ContainIntrinsicSize& containIntrinsicLogicalHeight() const;

    // Logical Grid
    inline const Style::GridTrackSizes& gridAutoList(Style::GridTrackSizingDirection) const;
    inline const Style::GridTemplateList& gridTemplateList(Style::GridTrackSizingDirection) const;
    inline const Style::GridPosition& gridItemStart(Style::GridTrackSizingDirection) const;
    inline const Style::GridPosition& gridItemEnd(Style::GridTrackSizingDirection) const;
    inline const Style::GapGutter& gap(Style::GridTrackSizingDirection) const;

    // MARK: - Used

    inline float usedLetterSpacing() const;
    inline float usedWordSpacing() const;
    inline PointerEvents usedPointerEvents() const;
    inline StyleAppearance usedAppearance() const;
    inline void setUsedAppearance(StyleAppearance);
    inline Visibility usedVisibility() const;
    inline UserModify usedUserModify() const;
    WEBCORE_EXPORT UserSelect usedUserSelect() const;
    inline Style::Contain usedContain() const;
    // usedContentVisibility will return ContentVisibility::Hidden in a content-visibility: hidden subtree (overriding
    // content-visibility: auto at all times), ContentVisibility::Auto in a content-visibility: auto subtree (when the
    // content is not user relevant and thus skipped), and ContentVisibility::Visible otherwise.
    inline ContentVisibility usedContentVisibility() const;
    inline void setUsedContentVisibility(ContentVisibility);
    inline TransformStyle3D usedTransformStyle3D() const;
    inline void setTransformStyleForcedToFlat(bool);
    inline float usedPerspective() const;
    // 'touch-action' behavior depends on values in ancestors. We use an additional inherited property to implement that.
    inline Style::TouchAction usedTouchAction() const;
    inline void setUsedTouchAction(Style::TouchAction);
    Color usedScrollbarThumbColor() const;
    Color usedScrollbarTrackColor() const;
    static UsedFloat usedFloat(const RenderElement&); // Returns logical left/right (block-relative).
    static UsedClear usedClear(const RenderElement&); // Returns logical left/right (block-relative).
    inline Style::ZIndex usedZIndex() const;
    inline void setUsedZIndex(Style::ZIndex);
    float computedStrokeWidth(const IntSize& viewportSize) const;
    Color computedStrokeColor() const;
    inline CSSPropertyID usedStrokeColorProperty() const;
#if HAVE(CORE_MATERIAL)
    inline AppleVisualEffect usedAppleVisualEffectForSubtree() const;
    inline void setUsedAppleVisualEffectForSubtree(AppleVisualEffect);
#endif
    void setEffectiveDisplay(DisplayType v) { m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(v); }
    inline Style::WebkitLocale computedLocale() const;

    // MARK: - has*()

    inline bool hasAnimations() const;
    inline bool hasAnimationsOrTransitions() const;
    inline bool hasAppearance() const;
    inline bool hasAppleColorFilter() const;
    inline bool hasAspectRatio() const;
    inline bool hasAutoLengthContainIntrinsicSize() const;
    inline bool hasBackdropFilter() const;
    inline bool hasBackground() const;
    inline bool hasBackgroundImage() const;
    inline bool hasBlendMode() const;
    inline bool hasBorder() const;
    inline bool hasBorderImage() const;
    inline bool hasBorderRadius() const;
    inline bool hasBoxReflect() const;
    inline bool hasBoxShadow() const;
    inline bool hasClip() const;
    inline bool hasClipPath() const;
    inline bool hasContent() const;
    inline bool hasFill() const;
    inline bool hasFilter() const;
    inline bool hasInlineColumnAxis() const;
    inline bool hasIsolation() const;
    inline bool hasMarkers() const;
    inline bool hasMask() const;
    inline bool hasOffsetPath() const;
    inline bool hasOpacity() const;
    inline bool hasOutline() const;
    inline bool hasOutlineInVisualOverflow() const;
    inline bool hasPerspective() const;
    inline bool hasPositionedMask() const;
    inline bool hasRotate() const;
    inline bool hasScale() const;
    inline bool hasScrollTimelines() const;
    inline bool hasSnapPosition() const;
    inline bool hasStroke() const;
    inline bool hasTextCombine() const;
    inline bool hasTextShadow() const;
    inline bool hasTransform() const;
    inline bool hasTransitions() const;
    inline bool hasTranslate() const;
    inline bool hasUsedAppearance() const;
    inline bool hasUsedContentNone() const;
    inline bool hasViewTimelines() const;
    inline bool hasVisibleBorder() const;
    inline bool hasVisibleBorderDecoration() const;
    bool hasPositiveStrokeWidth() const;
#if HAVE(CORE_MATERIAL)
    inline bool hasAppleVisualEffect() const;
    inline bool hasAppleVisualEffectRequiringBackdropFilter() const;
#endif

    // MARK: - Other predicates

    inline bool isColumnFlexDirection() const;
    inline bool isEffectivelyTransparent() const; // This or any ancestor has opacity 0.
    inline bool isFixedTableLayout() const;
    inline bool isFloating() const;
    inline bool isInterCharacterRubyPosition() const;
    inline bool isOverflowVisible() const;
    inline bool isReverseFlexDirection() const;
    inline bool isRowFlexDirection() const;
    inline bool isSkippedRootOrSkippedContent() const;

    constexpr bool isDisplayInlineType() const;
    constexpr bool isOriginalDisplayInlineType() const;
    constexpr bool isDisplayFlexibleOrGridFormattingContextBox() const;
    constexpr bool isDisplayDeprecatedFlexibleBox() const;
    constexpr bool isDisplayFlexibleBoxIncludingDeprecatedOrGridFormattingContextBox() const;
    constexpr bool isDisplayRegionType() const;
    constexpr bool isDisplayBlockLevel() const;
    constexpr bool isOriginalDisplayBlockType() const;
    constexpr bool isDisplayTableOrTablePart() const;
    constexpr bool isInternalTableBox() const;
    constexpr bool isRubyContainerOrInternalRubyBox() const;
    constexpr bool isOriginalDisplayListItemType() const;

    bool isLeftToRightDirection() const { return writingMode().isBidiLTR(); } // deprecated, because of confusion between physical inline directions and bidi / line-relative directions

    constexpr bool doesDisplayGenerateBlockContainer() const;

    inline bool affectsTransform() const;
    inline bool specifiesColumns() const;
    inline bool columnRuleIsTransparent() const;
    inline bool borderLeftIsTransparent() const;
    inline bool borderRightIsTransparent() const;
    inline bool borderTopIsTransparent() const;
    inline bool borderBottomIsTransparent() const;
    inline bool preserves3D() const;
    inline bool effectiveInert() const;
    inline bool usesStandardScrollbarStyle() const;
    inline bool usesLegacyScrollbarStyle() const;

    bool shouldPlaceVerticalScrollbarOnLeft() const;

    inline bool autoWrap() const;
    inline bool preserveNewline() const;
    inline bool collapseWhiteSpace() const;
    inline bool isCollapsibleWhiteSpace(char16_t) const;
    inline bool breakOnlyAfterWhiteSpace() const;
    inline bool breakWords() const;
    static constexpr bool preserveNewline(WhiteSpaceCollapse);
    static constexpr bool collapseWhiteSpace(WhiteSpaceCollapse);

    // MARK: - Transforms

    // Return true if any transform related property (currently transform, translate, scale, rotate, transformStyle3D or perspective)
    // indicates that we are transforming. The usedTransformStyle3D is not used here because in many cases (such as for deciding
    // whether or not to establish a containing block), the computed value is what matters.
    inline bool hasTransformRelatedProperty() const;

    enum class TransformOperationOption : uint8_t {
        TransformOrigin = 1 << 0,
        Translate       = 1 << 1,
        Rotate          = 1 << 2,
        Scale           = 1 << 3,
        Offset          = 1 << 4
    };

    static constexpr OptionSet<TransformOperationOption> allTransformOperations();
    static constexpr OptionSet<TransformOperationOption> individualTransformOperations();

    bool affectedByTransformOrigin() const;

    FloatPoint computePerspectiveOrigin(const FloatRect& boundingBox) const;
    void applyPerspective(TransformationMatrix&, const FloatPoint& originTranslate) const;

    FloatPoint3D computeTransformOrigin(const FloatRect& boundingBox) const;
    void applyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;
    void unapplyTransformOrigin(TransformationMatrix&, const FloatPoint3D& originTranslate) const;

    // applyTransform calls applyTransformOrigin(), then applyCSSTransform(), followed by unapplyTransformOrigin().
    void applyTransform(TransformationMatrix&, const TransformOperationData& boundingBox) const;
    void applyTransform(TransformationMatrix&, const TransformOperationData& boundingBox, OptionSet<TransformOperationOption>) const;
    void applyCSSTransform(TransformationMatrix&, const TransformOperationData& boundingBox) const;
    void applyCSSTransform(TransformationMatrix&, const TransformOperationData& boundingBox, OptionSet<TransformOperationOption>) const;
    void setPageScaleTransform(float);

    // MARK: - Colors

    Color colorResolvingCurrentColor(CSSPropertyID colorProperty, bool visitedLink) const;

    // Resolves the currentColor keyword, but must not be used for the "color" property which has a different semantic.
    WEBCORE_EXPORT Color colorResolvingCurrentColor(const Style::Color&, bool visitedLink = false) const;

    WEBCORE_EXPORT Color visitedDependentColor(CSSPropertyID, OptionSet<PaintBehavior> paintBehavior = { }) const;
    WEBCORE_EXPORT Color visitedDependentColorWithColorFilter(CSSPropertyID, OptionSet<PaintBehavior> paintBehavior = { }) const;

    WEBCORE_EXPORT Color colorByApplyingColorFilter(const Color&) const;
    WEBCORE_EXPORT Color colorWithColorFilter(const Style::Color&) const;

    Color usedAccentColor(OptionSet<StyleColorOptions>) const;

    // MARK: - Initial

    static inline Style::BorderImage initialBorderImage();
    static inline Style::BackgroundLayers initialBackgroundLayers();
    static inline Style::MaskLayers initialMaskLayers();
    static inline Style::MaskBorder initialMaskBorder();
    static inline Style::NameScope initialTimelineScope();
    static inline Style::Animations initialAnimations();
    static inline Style::Transitions initialTransitions();
    static constexpr Style::SVGPaintOrder initialPaintOrder();
    static constexpr LineCap initialCapStyle();
    static constexpr LineJoin initialJoinStyle();
    static inline Style::StrokeWidth initialStrokeWidth();
    static inline Style::Color initialStrokeColor();
    static constexpr Style::StrokeMiterlimit initialStrokeMiterLimit();
    static inline Style::SVGPaint initialFill();
    static constexpr Style::Opacity initialFillOpacity();
    static inline Style::SVGPaint initialStroke();
    static constexpr Style::Opacity initialStrokeOpacity();
    static inline Style::SVGStrokeDasharray initialStrokeDashArray();
    static inline Style::SVGStrokeDashoffset initialStrokeDashOffset();
    static inline Style::SVGPathData initialD();
    static constexpr Style::Opacity initialFloodOpacity();
    static constexpr Style::Opacity initialStopOpacity();
    static inline Style::Color initialStopColor();
    static inline Style::Color initialFloodColor();
    static inline Style::Color initialLightingColor();
    static inline Style::SVGBaselineShift initialBaselineShift();
    static inline Style::ShapeOutside initialShapeOutside();
    static inline Style::ShapeMargin initialShapeMargin();
    static constexpr Style::ShapeImageThreshold initialShapeImageThreshold();
    static inline Style::ClipPath initialClipPath();
    static constexpr Overflow initialOverflowX();
    static constexpr Overflow initialOverflowY();
    static constexpr OverscrollBehavior initialOverscrollBehaviorX();
    static constexpr OverscrollBehavior initialOverscrollBehaviorY();
    static constexpr Style::AlignContent initialAlignContent();
    static constexpr Style::AlignItems initialAlignItems();
    static constexpr Style::AlignSelf initialAlignSelf();
    static constexpr Clear initialClear();
    static inline Style::Clip initialClip();
    static inline Style::SVGCenterCoordinateComponent initialCx();
    static inline Style::SVGCenterCoordinateComponent initialCy();
    static constexpr DisplayType initialDisplay();
    static constexpr UnicodeBidi initialUnicodeBidi();
    static constexpr PositionType initialPosition();
    static inline Style::VerticalAlign initialVerticalAlign();
    static constexpr Float initialFloating();
    static constexpr FontOpticalSizing initialFontOpticalSizing();
    static inline Style::FontFeatureSettings initialFontFeatureSettings();
    static inline Style::FontVariationSettings initialFontVariationSettings();
    static inline Style::FontPalette initialFontPalette();
    static inline Style::FontSizeAdjust initialFontSizeAdjust();
    static inline Style::FontStyle initialFontStyle();
    static inline Style::FontWeight initialFontWeight();
    static inline Style::FontWidth initialFontWidth();
    static constexpr Kerning initialFontKerning();
    static constexpr FontSmoothingMode initialFontSmoothing();
    static constexpr FontSynthesisLonghandValue initialFontSynthesisSmallCaps();
    static constexpr FontSynthesisLonghandValue initialFontSynthesisStyle();
    static constexpr FontSynthesisLonghandValue initialFontSynthesisWeight();
    static inline Style::FontVariantAlternates initialFontVariantAlternates();
    static constexpr FontVariantCaps initialFontVariantCaps();
    static constexpr Style::FontVariantEastAsian initialFontVariantEastAsian();
    static constexpr FontVariantEmoji initialFontVariantEmoji();
    static constexpr Style::FontVariantLigatures initialFontVariantLigatures();
    static constexpr Style::FontVariantNumeric initialFontVariantNumeric();
    static constexpr FontVariantPosition initialFontVariantPosition();
    static inline Style::WebkitLocale initialLocale();
    static constexpr Style::TextAutospace initialTextAutospace();
    static constexpr TextRenderingMode initialTextRendering();
    static constexpr Style::TextSpacingTrim initialTextSpacingTrim();
    static constexpr BreakBetween initialBreakBetween();
    static constexpr BreakInside initialBreakInside();
    static constexpr Style::HangingPunctuation initialHangingPunctuation();
    static constexpr TableLayoutType initialTableLayout();
    static constexpr BorderCollapse initialBorderCollapse();
    static constexpr BorderStyle initialBorderStyle();
    static inline Style::BorderRadiusValue initialBorderRadius();
    static constexpr Style::CornerShapeValue initialCornerShapeValue();
    static constexpr CaptionSide initialCaptionSide();
    static constexpr ColumnAxis initialColumnAxis();
    static constexpr ColumnProgression initialColumnProgression();
    static constexpr TextDirection initialDirection();
    static constexpr StyleWritingMode initialWritingMode();
    static constexpr TextCombine initialTextCombine();
    static constexpr TextOrientation initialTextOrientation();
    static constexpr ObjectFit initialObjectFit();
    static inline Style::ObjectPosition initialObjectPosition();
    static constexpr EmptyCell initialEmptyCells();
    static constexpr ListStylePosition initialListStylePosition();
    static inline Style::ListStyleType initialListStyleType();
    static constexpr Style::TextTransform initialTextTransform();
    static inline Style::ViewTransitionClasses initialViewTransitionClasses();
    static inline Style::ViewTransitionName initialViewTransitionName();
    static constexpr Visibility initialVisibility();
    static constexpr WhiteSpaceCollapse initialWhiteSpaceCollapse();
    static constexpr Style::WebkitBorderSpacing initialBorderHorizontalSpacing();
    static constexpr Style::WebkitBorderSpacing initialBorderVerticalSpacing();
    static inline Style::Cursor initialCursor();
    static inline Color initialColor();
    static inline Style::Color initialBorderBottomColor();
    static inline Style::Color initialBorderLeftColor();
    static inline Style::Color initialBorderRightColor();
    static inline Style::Color initialBorderTopColor();
    static inline Style::Color initialColumnRuleColor();
    static inline Style::Color initialOutlineColor();
    static inline Style::Color initialTextDecorationColor();
    static inline Style::Color initialTextFillColor();
    static inline Style::Color initialTextStrokeColor();
    static inline Style::AccentColor initialAccentColor();
    static inline Style::ImageOrNone initialListStyleImage();
    static constexpr Style::LineWidth initialBorderWidth();
    static constexpr Style::LineWidth initialColumnRuleWidth();
    static constexpr Style::LineWidth initialOutlineWidth();
    static inline Style::LetterSpacing initialLetterSpacing();
    static inline Style::WordSpacing initialWordSpacing();
    static inline Style::PreferredSize initialSize();
    static inline Style::MinimumSize initialMinSize();
    static inline Style::MaximumSize initialMaxSize();
    static inline Style::InsetEdge initialInset();
    static inline Style::SVGRadius initialR();
    static inline Style::SVGRadiusComponent initialRx();
    static inline Style::SVGRadiusComponent initialRy();
    static inline Style::MarginEdge initialMargin();
    static constexpr Style::MarginTrim initialMarginTrim();
    static inline Style::PaddingEdge initialPadding();
    static inline Style::PageSize initialPageSize();
    static inline Style::TextIndent initialTextIndent();
    static constexpr TextBoxTrim initialTextBoxTrim();
    static constexpr Style::TextBoxEdge initialTextBoxEdge();
    static constexpr Style::LineFitEdge initialLineFitEdge();
    static constexpr Style::Widows initialWidows();
    static constexpr Style::Orphans initialOrphans();
    static inline Style::LineHeight initialLineHeight();
    static constexpr Style::TextAlign initialTextAlign();
    static constexpr Style::TextAlignLast initialTextAlignLast();
    static constexpr TextGroupAlign initialTextGroupAlign();
    static constexpr Style::TextDecorationLine initialTextDecorationLine();
    static constexpr Style::TextDecorationLine initialTextDecorationLineInEffect();
    static constexpr TextDecorationStyle initialTextDecorationStyle();
    static constexpr TextDecorationSkipInk initialTextDecorationSkipInk();
    static constexpr Style::TextUnderlinePosition initialTextUnderlinePosition();
    static inline Style::TextUnderlineOffset initialTextUnderlineOffset();
    static inline Style::TextDecorationThickness initialTextDecorationThickness();
    static constexpr Style::ZIndex initialSpecifiedZIndex();
    static constexpr Style::ZIndex initialUsedZIndex();
    static constexpr float initialZoom() { return 1.0f; }
    static constexpr TextZoom initialTextZoom();
    static constexpr Style::Length<> initialOutlineOffset();
    static constexpr Style::Opacity initialOpacity();
    static constexpr BoxAlignment initialBoxAlign();
    static constexpr BoxDecorationBreak initialBoxDecorationBreak();
    static constexpr BoxDirection initialBoxDirection();
    static constexpr BoxLines initialBoxLines();
    static constexpr BoxOrient initialBoxOrient();
    static constexpr BoxPack initialBoxPack();
    static constexpr Style::WebkitBoxFlex initialBoxFlex();
    static constexpr Style::WebkitBoxFlexGroup initialBoxFlexGroup();
    static constexpr Style::WebkitBoxOrdinalGroup initialBoxOrdinalGroup();
    static inline Style::BoxShadows initialBoxShadow();
    static constexpr BoxSizing initialBoxSizing();
    static inline Style::WebkitBoxReflect initialBoxReflect();
    static constexpr Style::FlexGrow initialFlexGrow();
    static constexpr Style::FlexShrink initialFlexShrink();
    static inline Style::FlexBasis initialFlexBasis();
    static constexpr Style::Order initialOrder();
    static constexpr Style::JustifyContent initialJustifyContent();
    static constexpr Style::JustifyItems initialJustifyItems();
    static constexpr Style::JustifySelf initialJustifySelf();
    static constexpr FlexDirection initialFlexDirection();
    static constexpr FlexWrap initialFlexWrap();
    static constexpr MarqueeBehavior initialMarqueeBehavior();
    static constexpr MarqueeDirection initialMarqueeDirection();
    static inline Style::WebkitMarqueeIncrement initialMarqueeIncrement();
    static constexpr Style::WebkitMarqueeRepetition initialMarqueeRepetition();
    static constexpr Style::WebkitMarqueeSpeed initialMarqueeSpeed();
    static constexpr UserModify initialUserModify();
    static constexpr UserDrag initialUserDrag();
    static constexpr UserSelect initialUserSelect();
    static constexpr TextOverflow initialTextOverflow();
    static inline Style::TextShadows initialTextShadow();
    static constexpr TextWrapMode initialTextWrapMode();
    static constexpr TextWrapStyle initialTextWrapStyle();
    static constexpr WordBreak initialWordBreak();
    static constexpr OutlineStyle initialOutlineStyle();
    static constexpr OverflowWrap initialOverflowWrap();
    static constexpr NBSPMode initialNBSPMode();
    static constexpr LineBreak initialLineBreak();
    static constexpr Style::SpeakAs initialSpeakAs();
    static constexpr Hyphens initialHyphens();
    static constexpr Style::HyphenateLimitEdge initialHyphenateLimitBefore();
    static constexpr Style::HyphenateLimitEdge initialHyphenateLimitAfter();
    static constexpr Style::HyphenateLimitLines initialHyphenateLimitLines();
    static inline Style::HyphenateCharacter initialHyphenateCharacter();
    static constexpr Style::Resize initialResize();
    static constexpr StyleAppearance initialAppearance();
    static inline Style::AspectRatio initialAspectRatio();
    static constexpr Style::Contain initialContain();
    static constexpr ContainerType initialContainerType();
    static Style::ContainerNames initialContainerNames();
    static inline Style::Content initialContent();
    static constexpr ContentVisibility initialContentVisibility();
    static inline Style::ContainIntrinsicSize initialContainIntrinsicWidth();
    static inline Style::ContainIntrinsicSize initialContainIntrinsicHeight();
    static constexpr Order initialRTLOrdering();
    static constexpr Style::WebkitTextStrokeWidth initialTextStrokeWidth();
    static constexpr Style::ColumnCount initialColumnCount();
    static constexpr ColumnFill initialColumnFill();
    static constexpr ColumnSpan initialColumnSpan();
    static inline Style::GapGutter initialColumnGap();
    static constexpr Style::ColumnWidth initialColumnWidth();
    static inline Style::GapGutter initialRowGap();
    static inline Style::ItemTolerance initialItemTolerance();
    static inline Style::Transform initialTransform();
    static inline Style::TransformOrigin initialTransformOrigin();
    static inline Style::TransformOriginX initialTransformOriginX();
    static inline Style::TransformOriginY initialTransformOriginY();
    static inline Style::TransformOriginZ initialTransformOriginZ();
    static constexpr TransformBox initialTransformBox();
    static inline Style::Rotate initialRotate();
    static inline Style::Scale initialScale();
    static inline Style::Translate initialTranslate();
    static constexpr PointerEvents initialPointerEvents();
    static constexpr TransformStyle3D initialTransformStyle3D();
    static constexpr BackfaceVisibility initialBackfaceVisibility();
    static inline Style::Perspective initialPerspective();
    static inline Style::PerspectiveOrigin initialPerspectiveOrigin();
    static inline Style::PerspectiveOriginX initialPerspectiveOriginX();
    static inline Style::PerspectiveOriginY initialPerspectiveOriginY();
    static inline Style::Color initialBackgroundColor();
    static inline Style::Color initialTextEmphasisColor();
    static inline Style::TextEmphasisStyle initialTextEmphasisStyle();
    static constexpr Style::TextEmphasisPosition initialTextEmphasisPosition();
    static constexpr RubyPosition initialRubyPosition();
    static constexpr RubyAlign initialRubyAlign();
    static constexpr RubyOverhang initialRubyOverhang();
    static constexpr Style::WebkitLineBoxContain initialLineBoxContain();
    static constexpr Style::ImageOrientation initialImageOrientation();
    static constexpr ImageRendering initialImageRendering();
    static inline Style::BorderImageSource initialBorderImageSource();
    static inline Style::MaskBorderSource initialMaskBorderSource();
    static constexpr PrintColorAdjust initialPrintColorAdjust();
    static inline Style::Quotes initialQuotes();
    static inline Style::SVGCoordinateComponent initialX();
    static inline Style::SVGCoordinateComponent initialY();
    static inline Style::DynamicRangeLimit initialDynamicRangeLimit();
    static constexpr TextJustify initialTextJustify();
    static inline Style::WillChange initialWillChange();
    static constexpr Style::TouchAction initialTouchAction();
    static constexpr FieldSizing initialFieldSizing();
    static inline Style::ScrollMarginEdge initialScrollMargin();
    static inline Style::ScrollPaddingEdge initialScrollPadding();
    static constexpr Style::ScrollSnapType initialScrollSnapType();
    static constexpr Style::ScrollSnapAlign initialScrollSnapAlign();
    static constexpr ScrollSnapStop initialScrollSnapStop();
    static inline Style::ProgressTimelineAxes initialScrollTimelineAxes();
    static inline Style::ProgressTimelineNames initialScrollTimelineNames();
    static inline Style::ProgressTimelineAxes initialViewTimelineAxes();
    static inline Style::ProgressTimelineNames initialViewTimelineNames();
    static inline Style::ViewTimelineInsets initialViewTimelineInsets();
    static inline Style::ScrollbarColor initialScrollbarColor();
    static constexpr Style::ScrollbarGutter initialScrollbarGutter();
    static constexpr Style::ScrollbarWidth initialScrollbarWidth();
    static constexpr Style::GridAutoFlow initialGridAutoFlow();
    static inline Style::GridTrackSizes initialGridAutoColumns();
    static inline Style::GridTrackSizes initialGridAutoRows();
    static inline Style::GridTemplateAreas initialGridTemplateAreas();
    static inline Style::GridTemplateList initialGridTemplateColumns();
    static inline Style::GridTemplateList initialGridTemplateRows();
    static inline Style::GridPosition initialGridItemColumnStart();
    static inline Style::GridPosition initialGridItemColumnEnd();
    static inline Style::GridPosition initialGridItemRowStart();
    static inline Style::GridPosition initialGridItemRowEnd();
    static constexpr Style::TabSize initialTabSize();
    static inline Style::WebkitLineGrid initialLineGrid();
    static constexpr LineSnap initialLineSnap();
    static constexpr LineAlign initialLineAlign();
    static constexpr Style::WebkitInitialLetter initialInitialLetter();
    static constexpr Style::WebkitLineClamp initialLineClamp();
    static inline Style::BlockEllipsis initialBlockEllipsis();
    static OverflowContinue initialOverflowContinue();
    static constexpr Style::MaximumLines initialMaxLines();
    static constexpr TextSecurity initialTextSecurity();
    static constexpr InputSecurity initialInputSecurity();
    static constexpr Style::ScrollBehavior initialScrollBehavior();
    static inline Style::Filter initialFilter();
    static inline Style::Filter initialBackdropFilter();
    static inline Style::AppleColorFilter initialAppleColorFilter();
    static constexpr BlendMode initialBlendMode();
    static constexpr Isolation initialIsolation();
    static constexpr Style::MathDepth initialMathDepth();
    static constexpr MathShift initialMathShift();
    static constexpr MathStyle initialMathStyle();
    static inline Style::OffsetPath initialOffsetPath();
    static inline Style::OffsetDistance initialOffsetDistance();
    static inline Style::OffsetPosition initialOffsetPosition();
    static inline Style::OffsetAnchor initialOffsetAnchor();
    static constexpr Style::OffsetRotate initialOffsetRotate();
    static constexpr OverflowAnchor initialOverflowAnchor();
    static inline Style::BlockStepSize initialBlockStepSize();
    static constexpr BlockStepAlign initialBlockStepAlign();
    static constexpr BlockStepInsert initialBlockStepInsert();
    static constexpr BlockStepRound initialBlockStepRound();
    static inline Style::AnchorNames initialAnchorNames();
    static inline Style::NameScope initialAnchorScope();
    static inline Style::PositionAnchor initialPositionAnchor();
    static inline Style::PositionArea initialPositionArea();
    static constexpr Style::PositionTryOrder initialPositionTryOrder();
    static inline Style::PositionTryFallbacks initialPositionTryFallbacks();
    static constexpr Style::PositionVisibility initialPositionVisibility();
    static constexpr AlignmentBaseline initialAlignmentBaseline();
    static constexpr DominantBaseline initialDominantBaseline();
    static constexpr VectorEffect initialVectorEffect();
    static constexpr BufferedRendering initialBufferedRendering();
    static constexpr WindRule initialClipRule();
    static constexpr ColorInterpolation initialColorInterpolation();
    static constexpr ColorInterpolation initialColorInterpolationFilters();
    static constexpr WindRule initialFillRule();
    static constexpr ShapeRendering initialShapeRendering();
    static constexpr TextAnchor initialTextAnchor();
    static constexpr Style::SVGGlyphOrientationHorizontal initialGlyphOrientationHorizontal();
    static constexpr Style::SVGGlyphOrientationVertical initialGlyphOrientationVertical();
    static constexpr MaskType initialMaskType();
    static inline Style::SVGMarkerResource initialMarkerStart();
    static inline Style::SVGMarkerResource initialMarkerMid();
    static inline Style::SVGMarkerResource initialMarkerEnd();
#if ENABLE(DARK_MODE_CSS)
    static inline Style::ColorScheme initialColorScheme();
#endif
#if ENABLE(CURSOR_VISIBILITY)
    static constexpr CursorVisibility initialCursorVisibility();
#endif
#if ENABLE(TEXT_AUTOSIZING)
    static inline Style::LineHeight initialSpecifiedLineHeight();
    static constexpr Style::TextSizeAdjust initialTextSizeAdjust();
#endif
#if ENABLE(APPLE_PAY)
    static constexpr ApplePayButtonStyle initialApplePayButtonStyle();
    static constexpr ApplePayButtonType initialApplePayButtonType();
#endif
#if HAVE(CORE_MATERIAL)
    static constexpr AppleVisualEffect initialAppleVisualEffect();
#endif
#if ENABLE(WEBKIT_TOUCH_CALLOUT_CSS_PROPERTY)
    static constexpr Style::WebkitTouchCallout initialTouchCallout();
#endif
#if ENABLE(TOUCH_EVENTS)
    static Style::Color initialTapHighlightColor();
#endif
#if ENABLE(WEBKIT_OVERFLOW_SCROLLING_CSS_PROPERTY)
    static constexpr Style::WebkitOverflowScrolling initialOverflowScrolling();
#endif

private:
    // This constructor is used to implement the replace operation.
    RenderStyle(RenderStyle&, RenderStyle&&);

    constexpr DisplayType originalDisplay() const { return static_cast<DisplayType>(m_nonInheritedFlags.originalDisplay); }

    const Style::Color& unresolvedColorForProperty(CSSPropertyID, bool visitedLink = false) const;

    inline bool hasAutoLeftAndRight() const;
    inline bool hasAutoTopAndBottom() const;

    static constexpr bool isDisplayInlineType(DisplayType);
    static constexpr bool isDisplayBlockType(DisplayType);
    static constexpr bool isDisplayFlexibleBox(DisplayType);
    static constexpr bool isDisplayGridFormattingContextBox(DisplayType);
    static constexpr bool isDisplayGridBox(DisplayType);
    static constexpr bool isDisplayGridLanesBox(DisplayType);
    static constexpr bool isDisplayFlexibleOrGridFormattingContextBox(DisplayType);
    static constexpr bool isDisplayDeprecatedFlexibleBox(DisplayType);
    static constexpr bool isDisplayListItemType(DisplayType);
    static constexpr bool isDisplayTableOrTablePart(DisplayType);
    static constexpr bool isInternalTableBox(DisplayType);
    static constexpr bool isRubyContainerOrInternalRubyBox(DisplayType);

    bool changeAffectsVisualOverflow(const RenderStyle&) const;
    bool changeRequiresLayout(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresOutOfFlowMovementLayoutOnly(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresLayerRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaint(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRepaintIfText(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
    bool changeRequiresRecompositeLayer(const RenderStyle&, OptionSet<StyleDifferenceContextSensitiveProperty>& changedContextSensitiveProperties) const;
};

// Map from computed style values (which take zoom into account) to web-exposed values, which are zoom-independent.
inline int adjustForAbsoluteZoom(int, const RenderStyle&);
inline float adjustFloatForAbsoluteZoom(float, const RenderStyle&);
inline LayoutUnit adjustLayoutUnitForAbsoluteZoom(LayoutUnit, const RenderStyle&);
inline LayoutSize adjustLayoutSizeForAbsoluteZoom(LayoutSize, const RenderStyle&);

// Map from zoom-independent style values to computed style values (which take zoom into account).
inline float applyZoom(float, const RenderStyle&);

constexpr BorderStyle collapsedBorderStyle(BorderStyle);

inline bool pseudoElementRendererIsNeeded(const RenderStyle*);
inline bool generatesBox(const RenderStyle&);
inline bool isNonVisibleOverflow(Overflow);

inline bool isVisibleToHitTesting(const RenderStyle&, const HitTestRequest&);

inline bool shouldApplyLayoutContainment(const RenderStyle&, const Element&);
inline bool shouldApplySizeContainment(const RenderStyle&, const Element&);
inline bool shouldApplyInlineSizeContainment(const RenderStyle&, const Element&);
inline bool shouldApplyStyleContainment(const RenderStyle&, const Element&);
inline bool shouldApplyPaintContainment(const RenderStyle&, const Element&);
inline bool isSkippedContentRoot(const RenderStyle&, const Element&);

} // namespace WebCore
