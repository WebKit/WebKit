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

#include <WebCore/StyleComputedStyle.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RenderStyleBase);
class RenderStyleBase {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(RenderStyleBase, RenderStyleBase);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderStyleBase);
public:
    ~RenderStyleBase() = default;

    // Delegation to `Style::ComputedStyle` for `CheckedPtr` support.
    ALWAYS_INLINE uint32_t checkedPtrCount() const { return m_computedStyle.checkedPtrCount(); }
    ALWAYS_INLINE void incrementCheckedPtrCount() const { m_computedStyle.incrementCheckedPtrCount(); }
    ALWAYS_INLINE void decrementCheckedPtrCount() const { m_computedStyle.decrementCheckedPtrCount(); }
    ALWAYS_INLINE uint32_t checkedPtrCountWithoutThreadCheck() const { return m_computedStyle.checkedPtrCountWithoutThreadCheck(); }
    ALWAYS_INLINE void setDidBeginCheckedPtrDeletion() { m_computedStyle.setDidBeginCheckedPtrDeletion(); }

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

    // No setter. Set via `RenderStyleProperties::setBlendMode()`.
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

    // No setter. Set via `RenderStyleProperties::setDisplay()`.
    inline constexpr DisplayType originalDisplay() const;

    // `effectiveDisplay()` getter is an alias of `RenderStyleProperties::display()`.
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
    const AtomString& pseudoElementNameArgument() const;

    std::optional<Style::PseudoElementIdentifier> pseudoElementIdentifier() const;
    inline void setPseudoElementIdentifier(std::optional<Style::PseudoElementIdentifier>&&);

    inline bool hasAnyPublicPseudoStyles() const;
    inline bool hasPseudoStyle(PseudoElementType) const;
    inline void setHasPseudoStyles(EnumSet<PseudoElementType>);

    RenderStyle* getCachedPseudoStyle(const Style::PseudoElementIdentifier&) const;
    RenderStyle* addCachedPseudoStyle(std::unique_ptr<RenderStyle>);

    bool hasCachedPseudoStyles() const { return m_computedStyle.m_cachedPseudoStyles && m_computedStyle.m_cachedPseudoStyles->styles.size(); }
    const Style::PseudoStyleCache* cachedPseudoStyles() const { return m_computedStyle.m_cachedPseudoStyles.get(); }

    // MARK: - Custom properties

    inline const Style::CustomPropertyData& inheritedCustomProperties() const;
    inline const Style::CustomPropertyData& nonInheritedCustomProperties() const;
    const Style::CustomProperty* customPropertyValue(const AtomString&) const;
    void setCustomPropertyValue(Ref<const Style::CustomProperty>&&, bool isInherited);
    bool customPropertyValueEqual(const RenderStyleBase&, const AtomString&) const;
    bool customPropertiesEqual(const RenderStyleBase&) const;
    void deduplicateCustomProperties(const RenderStyleBase&);

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

    inline Style::ZoomFactor usedZoomForLength() const;

    // MARK: - Fonts

    inline const FontCascade& fontCascade() const;
    inline CheckedRef<const FontCascade> checkedFontCascade() const;
    inline FontCascade& mutableFontCascadeWithoutUpdate();
    inline void setFontCascade(FontCascade&&);

    inline const FontCascadeDescription& fontDescription() const;
    inline FontCascadeDescription& mutableFontDescriptionWithoutUpdate();
    inline void setFontDescription(FontCascadeDescription&&);
    inline bool setFontDescriptionWithoutUpdate(FontCascadeDescription&&);

    inline const FontMetrics& metricsOfPrimaryFont() const;
    inline std::pair<FontOrientation, NonCJKGlyphOrientation> fontAndGlyphOrientation();
    inline float computedFontSize() const;
    inline Style::WebkitLocale computedLocale() const;
    inline const Style::LineHeight& specifiedLineHeight() const;
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

    inline const CounterDirectiveMap& usedCounterDirectives() const;

    // MARK: - Writing Modes

    // FIXME: Rename to something that doesn't conflict with a property name.
    // Aggregates `writing-mode`, `direction` and `text-orientation`.
    WritingMode writingMode() const { return m_computedStyle.writingMode(); }

    // FIXME: *Deprecated* Deprecated due to confusion between physical inline directions and bidi / line-relative directions.
    bool isLeftToRightDirection() const { return writingMode().isBidiLTR(); }

    // MARK: - Aggregates

    inline Style::Animations& ensureAnimations();
    inline Style::BackgroundLayers& ensureBackgroundLayers();
    inline Style::MaskLayers& ensureMaskLayers();
    inline Style::Transitions& ensureTransitions();

    inline const BorderData& border() const;
    inline const BorderValue& borderBottom() const;
    inline const BorderValue& borderLeft() const;
    inline const BorderValue& borderRight() const;
    inline const BorderValue& borderTop() const;
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
    inline const Style::PageSize& pageSize() const;
    inline void setPageSize(Style::PageSize&&);

    // MARK: - Underlying ComputedStyle

    Style::ComputedStyle& computedStyle() { return m_computedStyle; }
    const Style::ComputedStyle& computedStyle() const { return m_computedStyle; }

protected:
    friend class RenderStyle;
    friend class Style::DifferenceFunctions;

    const Style::NonInheritedData& nonInheritedData() const { return computedStyle().nonInheritedData(); }
    const Style::ComputedStyle::NonInheritedFlags& nonInheritedFlags() const { return computedStyle().nonInheritedFlags(); }

    const Style::InheritedRareData& inheritedRareData() const { return computedStyle().inheritedRareData(); }
    const Style::InheritedData& inheritedData() const { return computedStyle().inheritedData(); }
    const Style::ComputedStyle::InheritedFlags& inheritedFlags() const { return computedStyle().inheritedFlags(); }

    const Style::SVGData& svgData() const { return computedStyle().svgData(); }

    enum CloneTag { Clone };
    enum CreateDefaultStyleTag { CreateDefaultStyle };

    RenderStyleBase(RenderStyleBase&&);
    RenderStyleBase& operator=(RenderStyleBase&&);

    RenderStyleBase(CreateDefaultStyleTag);
    RenderStyleBase(const RenderStyleBase&, CloneTag);

    RenderStyleBase(RenderStyleBase&, RenderStyleBase&&);

    Style::ComputedStyle m_computedStyle;
};

} // namespace WebCore
