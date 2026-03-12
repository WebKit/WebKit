/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include <WebCore/RenderStyleProperties.h>

namespace WebCore {

class RenderStyle final : public RenderStyleProperties {
public:
    RenderStyle(RenderStyle&&);
    RenderStyle& operator=(RenderStyle&&);

    explicit RenderStyle(CreateDefaultStyleTag);
    RenderStyle(const RenderStyle&, CloneTag);

    [[nodiscard]] RenderStyle NODELETE replace(RenderStyle&&);

    static RenderStyle& defaultStyleSingleton();

    // MARK: - Initialization

    WEBCORE_EXPORT static RenderStyle create();
    static std::unique_ptr<RenderStyle> createPtr();
    static std::unique_ptr<RenderStyle> createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry&);

    static RenderStyle NODELETE clone(const RenderStyle&);
    static RenderStyle cloneIncludingPseudoElements(const RenderStyle&);
    static std::unique_ptr<RenderStyle> clonePtr(const RenderStyle&);

    static RenderStyle createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, Style::Display);
    static RenderStyle createStyleInheritingFromPseudoStyle(const RenderStyle& pseudoStyle);

    void inheritFrom(const RenderStyle&);
    void inheritIgnoringCustomPropertiesFrom(const RenderStyle&);
    void inheritUnicodeBidiFrom(const RenderStyle&);
    void inheritColumnPropertiesFrom(const RenderStyle&);
    void fastPathInheritFrom(const RenderStyle&);
    void copyNonInheritedFrom(const RenderStyle&);
    void copyContentFrom(const RenderStyle&);
    void copyPseudoElementsFrom(const RenderStyle&);
    void copyPseudoElementBitsFrom(const RenderStyle&);

    // MARK: - Specific style change queries

    bool scrollAnchoringSuppressionStyleDidChange(const RenderStyle*) const;
    bool NODELETE outOfFlowPositionStyleDidChange(const RenderStyle*) const;

    // MARK: - Comparisons

    bool operator==(const RenderStyle&) const;

    bool inheritedEqual(const RenderStyle&) const;
    bool nonInheritedEqual(const RenderStyle&) const;
    bool fastPathInheritedEqual(const RenderStyle&) const;
    bool nonFastPathInheritedEqual(const RenderStyle&) const;
    bool descendantAffectingNonInheritedPropertiesEqual(const RenderStyle&) const;
    bool borderAndBackgroundEqual(const RenderStyle&) const;
    bool containerTypeAndNamesEqual(const RenderStyle&) const;
    bool columnSpanEqual(const RenderStyle&) const;
    bool scrollPaddingEqual(const RenderStyle&) const;
    bool fontCascadeEqual(const RenderStyle&) const;
    bool scrollSnapDataEquivalent(const RenderStyle&) const;

    // MARK: - Non-property values

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

    // No setter. Set via `RenderStyleProperties::setDisplay()`.
    inline constexpr Style::Display originalDisplay() const;

    // Sets the value of `display`, but leaves the value of `originalDisplay` unchanged.
    inline void setDisplayMaintainingOriginalDisplay(Style::Display);

    inline StyleAppearance usedAppearance() const;
    inline void setUsedAppearance(StyleAppearance);

    // usedContentVisibility will return ContentVisibility::Hidden in a content-visibility: hidden subtree (overriding
    // content-visibility: auto at all times), ContentVisibility::Auto in a content-visibility: auto subtree (when the
    // content is not user relevant and thus skipped), and ContentVisibility::Visible otherwise.
    inline ContentVisibility usedContentVisibility() const;
    inline void setUsedContentVisibility(ContentVisibility);

    // 'touch-action' behavior depends on values in ancestors. We use an additional inherited property to implement that.
    inline Style::TouchAction usedTouchAction() const;
    inline void setUsedTouchAction(Style::TouchAction);

    inline Style::ZIndex usedZIndex() const;
    inline void setUsedZIndex(Style::ZIndex);

#if HAVE(CORE_MATERIAL)
    inline AppleVisualEffect usedAppleVisualEffectForSubtree() const;
    inline void setUsedAppleVisualEffectForSubtree(AppleVisualEffect);
#endif

#if ENABLE(TEXT_AUTOSIZING)
    // MARK: - Text Autosizing

    inline AutosizeStatus autosizeStatus() const;
    inline void setAutosizeStatus(AutosizeStatus);
#endif

    // MARK: - Pseudo element/style

    std::optional<PseudoElementType> pseudoElementType() const;
    const AtomString& pseudoElementNameArgument() const LIFETIME_BOUND;

    std::optional<Style::PseudoElementIdentifier> pseudoElementIdentifier() const;
    inline void setPseudoElementIdentifier(std::optional<Style::PseudoElementIdentifier>&&);

    inline bool hasAnyPublicPseudoStyles() const;
    inline bool hasPseudoStyle(PseudoElementType) const;
    inline void setHasPseudoStyles(EnumSet<PseudoElementType>);

    RenderStyle* getCachedPseudoStyle(const Style::PseudoElementIdentifier&) const LIFETIME_BOUND;
    RenderStyle* addCachedPseudoStyle(std::unique_ptr<RenderStyle>) LIFETIME_BOUND;

    bool hasCachedPseudoStyles() const { return m_computedStyle.hasCachedPseudoStyles(); }
    const Style::PseudoStyleCache& cachedPseudoStyles() const LIFETIME_BOUND { return m_computedStyle.cachedPseudoStyles(); }

    // MARK: - Custom properties

    inline const Style::CustomPropertyData& inheritedCustomProperties() const LIFETIME_BOUND;
    inline const Style::CustomPropertyData& nonInheritedCustomProperties() const LIFETIME_BOUND;
    const Style::CustomProperty* customPropertyValue(const AtomString&) const LIFETIME_BOUND;
    void setCustomPropertyValue(Ref<const Style::CustomProperty>&&, bool isInherited);
    bool customPropertyValueEqual(const RenderStyle&, const AtomString&) const;
    bool customPropertiesEqual(const RenderStyle&) const;
    void deduplicateCustomProperties(const RenderStyle&);

    // MARK: - Custom paint

    void addCustomPaintWatchProperty(const AtomString&);

    // MARK: - Zoom

    inline bool evaluationTimeZoomEnabled() const;
    inline void setEvaluationTimeZoomEnabled(bool);

    inline bool useSVGZoomRulesForLength() const;
    inline void setUseSVGZoomRulesForLength(bool);

    inline float usedZoom() const;
    inline bool setUsedZoom(float);

    inline Style::ZoomFactor usedZoomForLength() const;

    // MARK: - Fonts

    inline const FontCascade& fontCascade() const;
    inline FontCascade& mutableFontCascadeWithoutUpdate();
    inline void setFontCascade(FontCascade&&);

    inline const FontCascadeDescription& fontDescription() const LIFETIME_BOUND;
    inline FontCascadeDescription& mutableFontDescriptionWithoutUpdate() LIFETIME_BOUND;
    inline void setFontDescription(FontCascadeDescription&&);
    inline bool setFontDescriptionWithoutUpdate(FontCascadeDescription&&);

    inline const FontMetrics& metricsOfPrimaryFont() const LIFETIME_BOUND;
    inline std::pair<FontOrientation, NonCJKGlyphOrientation> fontAndGlyphOrientation();
    inline float computedFontSize() const;
    inline Style::WebkitLocale computedLocale() const;
    inline const Style::LineHeight& specifiedLineHeight() const LIFETIME_BOUND;
#if ENABLE(TEXT_AUTOSIZING)
    inline void setSpecifiedLineHeight(Style::LineHeight&&);
#endif

    inline void setLetterSpacingFromAnimation(Style::LetterSpacing&&);
    inline void setWordSpacingFromAnimation(Style::WordSpacing&&);

    inline void synchronizeLetterSpacingWithFontCascade();
    inline void synchronizeLetterSpacingWithFontCascadeWithoutUpdate();
    inline void synchronizeWordSpacingWithFontCascade();
    inline void synchronizeWordSpacingWithFontCascadeWithoutUpdate();

    inline float usedLetterSpacing() const;
    inline float usedWordSpacing() const;

    // MARK: - Used Counter Directives

    inline const CounterDirectiveMap& usedCounterDirectives() const LIFETIME_BOUND;

    // MARK: - Writing Modes

    // FIXME: Rename to something that doesn't conflict with a property name.
    // Aggregates `writing-mode`, `direction` and `text-orientation`.
    WritingMode writingMode() const { return m_computedStyle.writingMode(); }

    // MARK: - Aggregates

    inline Style::Animations& ensureAnimations() LIFETIME_BOUND;
    inline Style::BackgroundLayers& ensureBackgroundLayers() LIFETIME_BOUND;
    inline Style::MaskLayers& ensureMaskLayers() LIFETIME_BOUND;
    inline Style::Transitions& ensureTransitions() LIFETIME_BOUND;
    inline Style::ScrollTimelines& ensureScrollTimelines() LIFETIME_BOUND;
    inline Style::ViewTimelines& ensureViewTimelines() LIFETIME_BOUND;

    inline const BorderData& border() const LIFETIME_BOUND;
    inline const BorderValue& borderBottom() const LIFETIME_BOUND;
    inline const BorderValue& borderLeft() const LIFETIME_BOUND;
    inline const BorderValue& borderRight() const LIFETIME_BOUND;
    inline const BorderValue& borderTop() const LIFETIME_BOUND;
    inline const Style::Animations& animations() const LIFETIME_BOUND;
    inline const Style::BackgroundLayers& backgroundLayers() const LIFETIME_BOUND;
    inline const Style::BorderImage& borderImage() const LIFETIME_BOUND;
    inline const Style::BorderRadius& borderRadii() const LIFETIME_BOUND;
    inline const Style::InsetBox& insetBox() const LIFETIME_BOUND;
    inline const Style::MarginBox& marginBox() const LIFETIME_BOUND;
    inline const Style::MaskBorder& maskBorder() const LIFETIME_BOUND;
    inline const Style::MaskLayers& maskLayers() const LIFETIME_BOUND;
    inline const Style::PaddingBox& paddingBox() const LIFETIME_BOUND;
    inline const Style::PerspectiveOrigin& perspectiveOrigin() const LIFETIME_BOUND;
    inline const Style::ScrollMarginBox& scrollMarginBox() const LIFETIME_BOUND;
    inline const Style::ScrollPaddingBox& scrollPaddingBox() const LIFETIME_BOUND;
    inline const Style::ScrollTimelines& scrollTimelines() const LIFETIME_BOUND;
    inline const Style::TransformOrigin& transformOrigin() const LIFETIME_BOUND;
    inline const Style::Transitions& transitions() const LIFETIME_BOUND;
    inline const Style::ViewTimelines& viewTimelines() const LIFETIME_BOUND;

    inline void setBackgroundLayers(Style::BackgroundLayers&&);
    inline void setBorderImage(Style::BorderImage&&);
    inline void setBorderRadius(Style::BorderRadiusValue&&);
    inline void setBorderTop(BorderValue&&);
    inline void setBorderRight(BorderValue&&);
    inline void setBorderBottom(BorderValue&&);
    inline void setBorderLeft(BorderValue&&);
    inline void setInsetBox(Style::InsetBox&&);
    inline void setMarginBox(Style::MarginBox&&);
    inline void setMaskBorder(Style::MaskBorder&&);
    inline void setMaskLayers(Style::MaskLayers&&);
    inline void setPaddingBox(Style::PaddingBox&&);
    inline void setPerspectiveOrigin(Style::PerspectiveOrigin&&);
    inline void setTransformOrigin(Style::TransformOrigin&&);

    // MARK: - Properties/descriptors that are not yet generated

    // `cursor`
    inline CursorType cursorType() const;

    // `@page size`
    inline const Style::PageSize& pageSize() const LIFETIME_BOUND;
    inline void setPageSize(Style::PageSize&&);

    // MARK: - Style reset utilities

    inline void resetBorder();
    inline void resetBorderExceptRadius();
    inline void resetBorderTop();
    inline void resetBorderRight();
    inline void resetBorderBottom();
    inline void resetBorderLeft();
    inline void resetBorderRadius();
    inline void resetMargin();
    inline void resetPadding();

    // MARK: - Logical Values

    // Logical Inset aliases
    inline const Style::InsetEdge& logicalLeft() const LIFETIME_BOUND;
    inline const Style::InsetEdge& logicalRight() const LIFETIME_BOUND;
    inline const Style::InsetEdge& logicalTop() const LIFETIME_BOUND;
    inline const Style::InsetEdge& logicalBottom() const LIFETIME_BOUND;

    // Logical Border (aggregate)
    const BorderValue& NODELETE borderBefore(const WritingMode) const LIFETIME_BOUND;
    const BorderValue& NODELETE borderAfter(const WritingMode) const LIFETIME_BOUND;
    const BorderValue& NODELETE borderStart(const WritingMode) const LIFETIME_BOUND;
    const BorderValue& NODELETE borderEnd(const WritingMode) const LIFETIME_BOUND;
    inline const BorderValue& borderBefore() const LIFETIME_BOUND;
    inline const BorderValue& borderAfter() const LIFETIME_BOUND;
    inline const BorderValue& borderStart() const LIFETIME_BOUND;
    inline const BorderValue& borderEnd() const LIFETIME_BOUND;

    // Logical Aspect Ratio
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalWidth() const;
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalHeight() const;
    inline double logicalAspectRatio() const;
    inline BoxSizing boxSizingForAspectRatio() const;

    // Logical Grid
    inline const Style::GridTrackSizes& gridAutoList(Style::GridTrackSizingDirection) const LIFETIME_BOUND;
    inline const Style::GridTemplateList& gridTemplateList(Style::GridTrackSizingDirection) const LIFETIME_BOUND;
    inline const Style::GridPosition& gridItemStart(Style::GridTrackSizingDirection) const LIFETIME_BOUND;
    inline const Style::GridPosition& gridItemEnd(Style::GridTrackSizingDirection) const LIFETIME_BOUND;
    inline const Style::GapGutter& gap(Style::GridTrackSizingDirection) const LIFETIME_BOUND;

    // MARK: - Derived Values

    inline float computedLineHeight() const;
    inline float computeLineHeight(const Style::LineHeight&) const;
    LayoutBoxExtent imageOutsets(const Style::BorderImage&) const;
    LayoutBoxExtent imageOutsets(const Style::MaskBorder&) const;
    LayoutBoxExtent borderImageOutsets() const;
    LayoutBoxExtent maskBorderOutsets() const;
    inline bool hasBorderImageOutsets() const;

    // MARK: - Used Values

    const AtomString& hyphenString() const LIFETIME_BOUND;
    float usedStrokeWidth(const IntSize& viewportSize) const;
    Color usedStrokeColor() const;
    Color usedStrokeColorApplyingColorFilter() const;
    inline PointerEvents usedPointerEvents() const;
    inline Visibility usedVisibility() const;
    inline UserModify usedUserModify() const;
    WEBCORE_EXPORT UserSelect NODELETE usedUserSelect() const;
    Style::Contain usedContain() const;
    inline TransformStyle3D usedTransformStyle3D() const;
    inline float usedPerspective() const;
    Color usedScrollbarThumbColor() const;
    Color usedScrollbarTrackColor() const;
    Color usedAccentColor(OptionSet<StyleColorOptions>) const;
    static UsedFloat usedFloat(const RenderElement&); // Returns logical left/right (block-relative).
    static UsedClear usedClear(const RenderElement&); // Returns logical left/right (block-relative).

    Style::LineWidth usedColumnRuleWidth() const;

    Style::Length<> usedOutlineOffset() const;
    Style::LineWidth usedOutlineWidth() const;
    float usedOutlineSize() const; // used value combining `outline-width` and `outline-offset`

    inline decltype(auto) usedBorderWidths() const;
    inline Style::LineWidth usedBorderBottomWidth() const;
    inline Style::LineWidth usedBorderLeftWidth() const;
    inline Style::LineWidth usedBorderRightWidth() const;
    inline Style::LineWidth usedBorderTopWidth() const;
    inline Style::LineWidth usedBorderWidthStart(WritingMode) const;
    inline Style::LineWidth usedBorderWidthEnd(WritingMode) const;
    inline Style::LineWidth usedBorderWidthBefore(WritingMode) const;
    inline Style::LineWidth usedBorderWidthAfter(WritingMode) const;
    inline Style::LineWidth usedBorderWidthLogicalLeft(WritingMode) const;
    inline Style::LineWidth usedBorderWidthLogicalRight(WritingMode) const;
    inline Style::LineWidth usedBorderWidthStart() const;
    inline Style::LineWidth usedBorderWidthEnd() const;
    inline Style::LineWidth usedBorderWidthBefore() const;
    inline Style::LineWidth usedBorderWidthAfter() const;
    inline Style::LineWidth usedBorderWidthLogicalLeft() const;
    inline Style::LineWidth usedBorderWidthLogicalRight() const;

    // MARK: - has*()

    inline bool hasBackground() const;
    inline bool hasInlineColumnAxis() const;
    inline bool hasMarkers() const;
    inline bool hasMask() const;
    inline bool hasOutline() const;
    inline bool hasOutlineInVisualOverflow() const;
    inline bool hasPositionedMask() const;
    inline bool hasUsedAppearance() const;
    inline bool hasUsedContentNone() const;
    inline bool hasExplicitlySetBorderRadius() const;
    inline bool hasPositiveStrokeWidth() const;

    // Whether or not a positioned element requires normal flow x/y to be computed to determine its position.
    inline bool hasStaticInlinePosition(bool horizontal) const;
    inline bool hasStaticBlockPosition(bool horizontal) const;
    inline bool hasOutOfFlowPosition() const;
    inline bool hasInFlowPosition() const;
    inline bool hasViewportConstrainedPosition() const;

    // MARK: - Other predicates

    inline bool isColumnFlexDirection() const;
    inline bool isFixedTableLayout() const;
    inline bool isInterCharacterRubyPosition() const;
    inline bool isOverflowVisible() const;
    inline bool isReverseFlexDirection() const;
    inline bool isRowFlexDirection() const;
    inline bool isSkippedRootOrSkippedContent() const;

    inline bool specifiesColumns() const;

    inline bool usesStandardScrollbarStyle() const;
    inline bool usesLegacyScrollbarStyle() const;
    inline bool shouldPlaceVerticalScrollbarOnLeft() const;

    inline bool preserveNewline() const;
    inline bool collapseWhiteSpace() const;
    inline bool isCollapsibleWhiteSpace(char16_t) const;
    inline bool breakOnlyAfterWhiteSpace() const;
    inline bool breakWords() const;
    static constexpr bool preserveNewline(WhiteSpaceCollapse);
    static constexpr bool collapseWhiteSpace(WhiteSpaceCollapse);

    // Return true if any transform related property (currently transform, translate, scale, rotate, transformStyle3D or perspective)
    // indicates that we are transforming. The usedTransformStyle3D is not used here because in many cases (such as for deciding
    // whether or not to establish a containing block), the computed value is what matters.
    inline bool hasTransformRelatedProperty() const;
    inline bool affectsTransform() const;

    // MARK: - Underlying ComputedStyle

    Style::ComputedStyle& computedStyle() LIFETIME_BOUND { return m_computedStyle; }
    const Style::ComputedStyle& computedStyle() const LIFETIME_BOUND { return m_computedStyle; }

private:
    friend class Style::DifferenceFunctions;

    // This constructor is used to implement the replace operation.
    RenderStyle(RenderStyle&, RenderStyle&&);

    const Style::NonInheritedData& nonInheritedData() const LIFETIME_BOUND { return computedStyle().nonInheritedData(); }
    const Style::ComputedStyle::NonInheritedFlags& nonInheritedFlags() const LIFETIME_BOUND { return computedStyle().nonInheritedFlags(); }
    const Style::InheritedRareData& inheritedRareData() const LIFETIME_BOUND { return computedStyle().inheritedRareData(); }
    const Style::InheritedData& inheritedData() const LIFETIME_BOUND { return computedStyle().inheritedData(); }
    const Style::ComputedStyle::InheritedFlags& inheritedFlags() const LIFETIME_BOUND { return computedStyle().inheritedFlags(); }
    const Style::SVGData& svgData() const LIFETIME_BOUND { return computedStyle().svgData(); }
};

} // namespace WebCore
