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

#include <WebCore/StyleComputedStyleProperties.h>

namespace WebCore {
namespace Style {

class ComputedStyle final : public ComputedStyleProperties {
public:
    ComputedStyle(ComputedStyle&&);
    ComputedStyle& operator=(ComputedStyle&&);

    explicit ComputedStyle(CreateDefaultStyleTag);
    ComputedStyle(const ComputedStyle&, CloneTag);

    [[nodiscard]] ComputedStyle LEGACY_NODELETE replace(ComputedStyle&&);

    static ComputedStyle& LEGACY_NODELETE defaultStyleSingleton();

    // MARK: - Initialization

    WEBCORE_EXPORT static ComputedStyle LEGACY_NODELETE create();
    static std::unique_ptr<ComputedStyle> createPtr();
    static std::unique_ptr<ComputedStyle> createPtrWithRegisteredInitialValues(const Style::CustomPropertyRegistry&);

    static ComputedStyle LEGACY_NODELETE clone(const ComputedStyle&);
    static ComputedStyle cloneIncludingPseudoElements(const ComputedStyle&);
    static std::unique_ptr<ComputedStyle> clonePtr(const ComputedStyle&);

    static ComputedStyle createAnonymousStyleWithDisplay(const ComputedStyle& parentStyle, Style::Display);
    static ComputedStyle createStyleInheritingFromPseudoStyle(const ComputedStyle& pseudoStyle);

    void inheritFrom(const ComputedStyle&);
    void inheritIgnoringCustomPropertiesFrom(const ComputedStyle&);
    void LEGACY_NODELETE inheritUnicodeBidiFrom(const ComputedStyle&);
    inline void inheritColumnPropertiesFrom(const ComputedStyle&);
    void fastPathInheritFrom(const ComputedStyle&);
    void copyNonInheritedFrom(const ComputedStyle&);
    void copyContentFrom(const ComputedStyle&);
    void copyPseudoElementsFrom(const ComputedStyle&);
    void LEGACY_NODELETE copyPseudoElementBitsFrom(const ComputedStyle&);

    // MARK: - Specific style change queries

    bool scrollAnchoringSuppressionStyleDidChange(const ComputedStyle*) const;
    bool LEGACY_NODELETE outOfFlowPositionStyleDidChange(const ComputedStyle*) const;

    // MARK: - Comparisons

    bool operator==(const ComputedStyle&) const;

    bool inheritedEqual(const ComputedStyle&) const;
    bool nonInheritedEqual(const ComputedStyle&) const;
    bool LEGACY_NODELETE fastPathInheritedEqual(const ComputedStyle&) const;
    bool nonFastPathInheritedEqual(const ComputedStyle&) const;
    bool LEGACY_NODELETE descendantAffectingNonInheritedPropertiesEqual(const ComputedStyle&) const;
    bool borderAndBackgroundEqual(const ComputedStyle&) const;
    inline bool containerTypeAndNamesEqual(const ComputedStyle&) const;
    inline bool columnSpanEqual(const ComputedStyle&) const;
    inline bool scrollPaddingEqual(const ComputedStyle&) const;
    inline bool fontCascadeEqual(const ComputedStyle&) const;
    bool LEGACY_NODELETE scrollSnapDataEquivalent(const ComputedStyle&) const;

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

    // MARK: - Non-property initial values.

    static inline PageSize initialPageSize();
    static constexpr ZIndex initialUsedZIndex();

    // MARK: - Logical Values

    // Logical Inset aliases
    inline const Style::InsetEdge& logicalLeft() const LIFETIME_BOUND;
    inline const Style::InsetEdge& logicalRight() const LIFETIME_BOUND;
    inline const Style::InsetEdge& logicalTop() const LIFETIME_BOUND;
    inline const Style::InsetEdge& logicalBottom() const LIFETIME_BOUND;

    // Logical Border (aggregate)
    const BorderValue& LEGACY_NODELETE borderBefore(const WritingMode) const LIFETIME_BOUND;
    const BorderValue& LEGACY_NODELETE borderAfter(const WritingMode) const LIFETIME_BOUND;
    const BorderValue& LEGACY_NODELETE borderStart(const WritingMode) const LIFETIME_BOUND;
    const BorderValue& LEGACY_NODELETE borderEnd(const WritingMode) const LIFETIME_BOUND;
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

    LayoutBoxExtent imageOutsets(const Style::BorderImage&, float deviceScaleFactor) const;
    LayoutBoxExtent imageOutsets(const Style::MaskBorder&, float deviceScaleFactor) const;
    LayoutBoxExtent borderImageOutsets(float deviceScaleFactor) const;
    LayoutBoxExtent maskBorderOutsets(float deviceScaleFactor) const;
    inline bool hasBorderImageOutsets() const;

    // MARK: - Used Values

    WEBCORE_EXPORT float usedLineHeight() const;
    const WTF::String& hyphenString() const LIFETIME_BOUND;
    float usedStrokeWidth(const IntSize& viewportSize) const;
    WebCore::Color usedStrokeColor() const;
    WebCore::Color usedStrokeColorApplyingColorFilter() const;
    inline PointerEvents usedPointerEvents() const;
    inline UserModify usedUserModify() const;
    WEBCORE_EXPORT UserSelect LEGACY_NODELETE usedUserSelect() const;
    Style::Contain usedContain() const;
    inline TransformStyle3D usedTransformStyle3D() const;
    WebCore::Color usedScrollbarThumbColor() const;
    WebCore::Color usedScrollbarTrackColor() const;
    WebCore::Color usedAccentColor(OptionSet<StyleColorOptions>) const;
    BlendMode LEGACY_NODELETE usedBlendMode() const;

    // MARK: - Used background color

    Color usedBackgroundColor() const;
    WEBCORE_EXPORT WebCore::Color usedBackgroundColorResolvingCurrentColor() const;
    WebCore::Color usedBackgroundColorResolvingCurrentColorApplyingColorFilter() const;
    WebCore::Color visitedDependentUsedBackgroundColor(OptionSet<PaintBehavior> = { }) const;
    WEBCORE_EXPORT WebCore::Color visitedDependentUsedBackgroundColorApplyingColorFilter(OptionSet<PaintBehavior> = { }) const;

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
    inline bool isRowFlexDirection() const;
    inline bool isReverseFlexDirection() const;
    inline FlowMode flexFlowMode() const; // This is not trivial so cache it.

    inline bool isFixedTableLayout() const;
    inline bool isInterCharacterRubyPosition() const;
    inline bool isOverflowVisible() const;
    inline bool isSkippedRootOrSkippedContent() const;

    bool LEGACY_NODELETE isListItemType() const;

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

    inline bool has3DTransformOperation() const;

private:
    ComputedStyle(ComputedStyle&, ComputedStyle&&);
};

// The outline and column rule widths after the used-value rules: zero for style none (and hidden for
// column rules), the platform focus ring width for outline-style auto. Unzoomed style values.
// Code with a renderer reads the resolved values through UsedStyle.
LineWidth LEGACY_NODELETE effectiveColumnRuleWidth(const ComputedStyle&);
LineWidth effectiveOutlineWidth(const ComputedStyle&);
// outline-offset with `inset` resolved against the effective outline width.
WEBCORE_EXPORT UsedOutlineOffset effectiveOutlineOffset(const ComputedStyle&);
// How far the outline reaches outside the border box.
float effectiveOutlineSize(const ComputedStyle&, ZoomFactor, float deviceScaleFactor);

} // namespace Style
} // namespace WebCore
