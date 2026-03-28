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

    WARN_UNUSED_RETURN RenderStyle replace(RenderStyle&&);

    static RenderStyle& defaultStyleSingleton();

    // MARK: - Initialization

    WEBCORE_EXPORT static RenderStyle create();
    static std::unique_ptr<RenderStyle> createPtr();
    static std::unique_ptr<RenderStyle> createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry&);

    static RenderStyle clone(const RenderStyle&);
    static RenderStyle cloneIncludingPseudoElements(const RenderStyle&);
    static std::unique_ptr<RenderStyle> clonePtr(const RenderStyle&);

    static RenderStyle createAnonymousStyleWithDisplay(const RenderStyle& parentStyle, DisplayType);
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
    bool outOfFlowPositionStyleDidChange(const RenderStyle*) const;

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

    // MARK: - Style adjustment utilities

    inline void setPageScaleTransform(float);
    inline void setColumnStylesFromPaginationMode(PaginationMode);
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
    inline void resetBorderRadius();
    inline void resetMargin();
    inline void resetPadding();

#if ENABLE(TEXT_AUTOSIZING)
    // MARK: - Text autosizing

    uint32_t hashForTextAutosizing() const;
    bool equalForTextAutosizing(const RenderStyle&) const;

    bool isIdempotentTextAutosizingCandidate() const;
    bool isIdempotentTextAutosizingCandidate(AutosizeStatus overrideStatus) const;
#endif

    // MARK: - Logical Values

    // Logical Inset aliases
    inline const Style::InsetEdge& logicalLeft() const;
    inline const Style::InsetEdge& logicalRight() const;
    inline const Style::InsetEdge& logicalTop() const;
    inline const Style::InsetEdge& logicalBottom() const;

    // Logical Border (aggregate)
    const BorderValue& borderBefore(const WritingMode) const;
    const BorderValue& borderAfter(const WritingMode) const;
    const BorderValue& borderStart(const WritingMode) const;
    const BorderValue& borderEnd(const WritingMode) const;
    inline const BorderValue& borderBefore() const;
    inline const BorderValue& borderAfter() const;
    inline const BorderValue& borderStart() const;
    inline const BorderValue& borderEnd() const;

    // Logical Aspect Ratio
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalWidth() const;
    inline Style::Number<CSS::Nonnegative> aspectRatioLogicalHeight() const;
    inline double logicalAspectRatio() const;
    inline BoxSizing boxSizingForAspectRatio() const;

    // Logical Grid
    inline const Style::GridTrackSizes& gridAutoList(Style::GridTrackSizingDirection) const;
    inline const Style::GridTemplateList& gridTemplateList(Style::GridTrackSizingDirection) const;
    inline const Style::GridPosition& gridItemStart(Style::GridTrackSizingDirection) const;
    inline const Style::GridPosition& gridItemEnd(Style::GridTrackSizingDirection) const;
    inline const Style::GapGutter& gap(Style::GridTrackSizingDirection) const;

    // MARK: - Derived Values

    inline float computedLineHeight() const;
    LayoutBoxExtent imageOutsets(const Style::BorderImage&) const;
    LayoutBoxExtent imageOutsets(const Style::MaskBorder&) const;
    LayoutBoxExtent borderImageOutsets() const;
    LayoutBoxExtent maskBorderOutsets() const;
    inline bool hasBorderImageOutsets() const;

    // MARK: - Used Values

    String altFromContent() const;
    const AtomString& hyphenString() const;
    float usedStrokeWidth(const IntSize& viewportSize) const;
    Color usedStrokeColor() const;
    Color usedStrokeColorApplyingColorFilter() const;
    inline PointerEvents usedPointerEvents() const;
    inline Visibility usedVisibility() const;
    inline UserModify usedUserModify() const;
    WEBCORE_EXPORT UserSelect usedUserSelect() const;
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

    inline bool hasAnimations() const;
    inline bool hasAnimationsOrTransitions() const;
    inline bool hasAppearance() const;
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
    inline bool hasExplicitlySetBorderRadius() const;
    inline bool hasPositiveStrokeWidth() const;
#if HAVE(CORE_MATERIAL)
    inline bool hasAppleVisualEffect() const;
    inline bool hasAppleVisualEffectRequiringBackdropFilter() const;
#endif

    // Whether or not a positioned element requires normal flow x/y to be computed to determine its position.
    inline bool hasStaticInlinePosition(bool horizontal) const;
    inline bool hasStaticBlockPosition(bool horizontal) const;
    inline bool hasOutOfFlowPosition() const;
    inline bool hasInFlowPosition() const;
    inline bool hasViewportConstrainedPosition() const;

    // MARK: - Other predicates

    inline bool isColumnFlexDirection() const;
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

    constexpr bool doesDisplayGenerateBlockContainer() const;

    inline bool specifiesColumns() const;

    inline bool usesStandardScrollbarStyle() const;
    inline bool usesLegacyScrollbarStyle() const;
    inline bool shouldPlaceVerticalScrollbarOnLeft() const;

    inline bool autoWrap() const;
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
    inline bool preserves3D() const;
    inline bool affectsTransform() const;

private:
    // This constructor is used to implement the replace operation.
    RenderStyle(RenderStyle&, RenderStyle&&);

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

#if ENABLE(TEXT_AUTOSIZING)

// MARK: - Text autosizing

inline unsigned RenderStyle::hashForTextAutosizing() const
{
    return m_computedStyle.hashForTextAutosizing();
}

inline bool RenderStyle::equalForTextAutosizing(const RenderStyle& other) const
{
    return m_computedStyle.equalForTextAutosizing(other.m_computedStyle);
}

#endif // ENABLE(TEXT_AUTOSIZING)

} // namespace WebCore
