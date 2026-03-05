/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2014-2021 Google Inc. All rights reserved.
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
 */

#pragma once

#include "RenderStyle+GettersInlines.h"

#define RENDER_STYLE_PROPERTIES_SETTERS_INLINES_INCLUDE_TRAP 1
#include "RenderStyleProperties+SettersInlines.h"
#undef RENDER_STYLE_PROPERTIES_SETTERS_INLINES_INCLUDE_TRAP

namespace WebCore {

// MARK: - Initialization

inline void RenderStyle::inheritFrom(const RenderStyle& other)
{
    return m_computedStyle.inheritFrom(other.m_computedStyle);
}

inline void RenderStyle::inheritIgnoringCustomPropertiesFrom(const RenderStyle& other)
{
    return m_computedStyle.inheritIgnoringCustomPropertiesFrom(other.m_computedStyle);
}

inline void RenderStyle::inheritUnicodeBidiFrom(const RenderStyle& other)
{
    return m_computedStyle.inheritUnicodeBidiFrom(other.m_computedStyle);
}

inline void RenderStyle::inheritColumnPropertiesFrom(const RenderStyle& other)
{
    return m_computedStyle.inheritColumnPropertiesFrom(other.m_computedStyle);
}

inline void RenderStyle::fastPathInheritFrom(const RenderStyle& other)
{
    return m_computedStyle.fastPathInheritFrom(other.m_computedStyle);
}

inline void RenderStyle::copyNonInheritedFrom(const RenderStyle& other)
{
    return m_computedStyle.copyNonInheritedFrom(other.m_computedStyle);
}

inline void RenderStyle::copyContentFrom(const RenderStyle& other)
{
    return m_computedStyle.copyContentFrom(other.m_computedStyle);
}

inline void RenderStyle::copyPseudoElementBitsFrom(const RenderStyle& other)
{
    return m_computedStyle.copyPseudoElementBitsFrom(other.m_computedStyle);
}

// MARK: - Non-property setters

inline void RenderStyle::setUsesViewportUnits()
{
    m_computedStyle.setUsesViewportUnits();
}

inline void RenderStyle::setUsesContainerUnits()
{
    m_computedStyle.setUsesContainerUnits();
}

inline void RenderStyle::setUsesTreeCountingFunctions()
{
    m_computedStyle.setUsesTreeCountingFunctions();
}

inline void RenderStyle::setInsideLink(InsideLink insideLink)
{
    m_computedStyle.setInsideLink(insideLink);
}

inline void RenderStyle::setIsLink(bool isLink)
{
    m_computedStyle.setIsLink(isLink);
}

inline void RenderStyle::setEmptyState(bool emptyState)
{
    m_computedStyle.setEmptyState(emptyState);
}

inline void RenderStyle::setFirstChildState()
{
    m_computedStyle.setFirstChildState();
}

inline void RenderStyle::setLastChildState()
{
    m_computedStyle.setLastChildState();
}

inline void RenderStyle::setHasExplicitlyInheritedProperties()
{
    m_computedStyle.setHasExplicitlyInheritedProperties();
}

inline void RenderStyle::setDisallowsFastPathInheritance()
{
    m_computedStyle.setDisallowsFastPathInheritance();
}

inline void RenderStyle::setEffectiveInert(bool effectiveInert)
{
    m_computedStyle.setEffectiveInert(effectiveInert);
}

inline void RenderStyle::setIsEffectivelyTransparent(bool effectivelyTransparent)
{
    m_computedStyle.setIsEffectivelyTransparent(effectivelyTransparent);
}

inline void RenderStyle::setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes)
{
    m_computedStyle.setEventListenerRegionTypes(eventListenerTypes);
}

inline void RenderStyle::setHasAttrContent()
{
    m_computedStyle.setHasAttrContent();
}

inline void RenderStyle::setHasDisplayAffectedByAnimations()
{
    m_computedStyle.setHasDisplayAffectedByAnimations();
}

inline void RenderStyle::setTransformStyleForcedToFlat(bool value)
{
    m_computedStyle.setTransformStyleForcedToFlat(value);
}

inline void RenderStyle::setUsesAnchorFunctions()
{
    m_computedStyle.setUsesAnchorFunctions();
}

inline void RenderStyle::setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis> axes)
{
    m_computedStyle.setAnchorFunctionScrollCompensatedAxes(axes);
}

inline void RenderStyle::setIsPopoverInvoker()
{
    m_computedStyle.setIsPopoverInvoker();
}

inline void RenderStyle::setNativeAppearanceDisabled(bool value)
{
    m_computedStyle.setNativeAppearanceDisabled(value);
}

inline void RenderStyle::setIsForceHidden()
{
    m_computedStyle.setIsForceHidden();
}

inline void RenderStyle::setAutoRevealsWhenFound()
{
    m_computedStyle.setAutoRevealsWhenFound();
}

inline void RenderStyle::setInsideDefaultButton(bool value)
{
    m_computedStyle.setInsideDefaultButton(value);
}

inline void RenderStyle::setInsideSubmitButton(bool value)
{
    m_computedStyle.setInsideSubmitButton(value);
}

inline void RenderStyle::setUsedPositionOptionIndex(std::optional<size_t> index)
{
    m_computedStyle.setUsedPositionOptionIndex(index);
}

inline void RenderStyle::setDisplayMaintainingOriginalDisplay(Style::Display display)
{
    m_computedStyle.setDisplayMaintainingOriginalDisplay(display);
}

// MARK: - Cache used values

inline void RenderStyle::setUsedAppearance(StyleAppearance appearance)
{
    m_computedStyle.setUsedAppearance(appearance);
}

inline void RenderStyle::setUsedTouchAction(Style::TouchAction touchAction)
{
    m_computedStyle.setUsedTouchAction(touchAction);
}

inline void RenderStyle::setUsedContentVisibility(ContentVisibility usedContentVisibility)
{
    m_computedStyle.setUsedContentVisibility(usedContentVisibility);
}

inline void RenderStyle::setUsedZIndex(Style::ZIndex index)
{
    m_computedStyle.setUsedZIndex(index);
}

#if HAVE(CORE_MATERIAL)

inline void RenderStyle::setUsedAppleVisualEffectForSubtree(AppleVisualEffect effect)
{
    m_computedStyle.setUsedAppleVisualEffectForSubtree(effect);
}

#endif

#if ENABLE(TEXT_AUTOSIZING)

inline void RenderStyle::setAutosizeStatus(AutosizeStatus autosizeStatus)
{
    m_computedStyle.setAutosizeStatus(autosizeStatus);
}

#endif // ENABLE(TEXT_AUTOSIZING)

// MARK: - Pseudo element/style

inline void RenderStyle::setHasPseudoStyles(EnumSet<PseudoElementType> set)
{
    m_computedStyle.setHasPseudoStyles(set);
}

inline void RenderStyle::setPseudoElementIdentifier(std::optional<Style::PseudoElementIdentifier>&& identifier)
{
    m_computedStyle.setPseudoElementIdentifier(WTF::move(identifier));
}

inline RenderStyle* RenderStyle::addCachedPseudoStyle(std::unique_ptr<RenderStyle> pseudo)
{
    return m_computedStyle.addCachedPseudoStyle(WTF::move(pseudo));
}

// MARK: - Custom properties

inline void RenderStyle::setCustomPropertyValue(Ref<const Style::CustomProperty>&& value, bool isInherited)
{
    m_computedStyle.setCustomPropertyValue(WTF::move(value), isInherited);
}

// MARK: - Fonts

#if ENABLE(TEXT_AUTOSIZING)

inline void RenderStyle::setSpecifiedLineHeight(Style::LineHeight&& lineHeight)
{
    m_computedStyle.setSpecifiedLineHeight(WTF::move(lineHeight));
}

#endif

inline void RenderStyle::setLetterSpacingFromAnimation(Style::LetterSpacing&& value)
{
    m_computedStyle.setLetterSpacingFromAnimation(WTF::move(value));
}

inline void RenderStyle::setWordSpacingFromAnimation(Style::WordSpacing&& value)
{
    m_computedStyle.setWordSpacingFromAnimation(WTF::move(value));
}

// MARK: - Zoom

inline void RenderStyle::setEvaluationTimeZoomEnabled(bool value)
{
    m_computedStyle.setEvaluationTimeZoomEnabled(value);
}

inline void RenderStyle::setUseSVGZoomRulesForLength(bool value)
{
    m_computedStyle.setUseSVGZoomRulesForLength(value);
}

inline bool RenderStyle::setUsedZoom(float zoomLevel)
{
    return m_computedStyle.setUsedZoom(zoomLevel);
}

// MARK: - Aggregates

inline Style::Animations& RenderStyle::ensureAnimations()
{
    return m_computedStyle.ensureAnimations();
}

inline Style::Transitions& RenderStyle::ensureTransitions()
{
    return m_computedStyle.ensureTransitions();
}

inline Style::BackgroundLayers& RenderStyle::ensureBackgroundLayers()
{
    return m_computedStyle.ensureBackgroundLayers();
}

inline Style::MaskLayers& RenderStyle::ensureMaskLayers()
{
    return m_computedStyle.ensureMaskLayers();
}

inline Style::ScrollTimelines& RenderStyle::ensureScrollTimelines()
{
    return m_computedStyle.ensureScrollTimelines();
}

inline Style::ViewTimelines& RenderStyle::ensureViewTimelines()
{
    return m_computedStyle.ensureViewTimelines();
}

inline void RenderStyle::setBackgroundLayers(Style::BackgroundLayers&& layers)
{
    m_computedStyle.setBackgroundLayers(WTF::move(layers));
}

inline void RenderStyle::setMaskLayers(Style::MaskLayers&& layers)
{
    m_computedStyle.setMaskLayers(WTF::move(layers));
}

inline void RenderStyle::setMaskBorder(Style::MaskBorder&& image)
{
    m_computedStyle.setMaskBorder(WTF::move(image));
}

inline void RenderStyle::setBorderImage(Style::BorderImage&& image)
{
    m_computedStyle.setBorderImage(WTF::move(image));
}

inline void RenderStyle::setPerspectiveOrigin(Style::PerspectiveOrigin&& origin)
{
    m_computedStyle.setPerspectiveOrigin(WTF::move(origin));
}

inline void RenderStyle::setTransformOrigin(Style::TransformOrigin&& origin)
{
    m_computedStyle.setTransformOrigin(WTF::move(origin));
}

inline void RenderStyle::setInsetBox(Style::InsetBox&& box)
{
    m_computedStyle.setInsetBox(WTF::move(box));
}

inline void RenderStyle::setMarginBox(Style::MarginBox&& box)
{
    m_computedStyle.setMarginBox(WTF::move(box));
}

inline void RenderStyle::setPaddingBox(Style::PaddingBox&& box)
{
    m_computedStyle.setPaddingBox(WTF::move(box));
}

inline void RenderStyle::setBorderRadius(Style::BorderRadiusValue&& size)
{
    m_computedStyle.setBorderRadius(WTF::move(size));
}

inline void RenderStyle::setBorderTop(BorderValue&& value)
{
    m_computedStyle.setBorderTop(WTF::move(value));
}

inline void RenderStyle::setBorderRight(BorderValue&& value)
{
    m_computedStyle.setBorderRight(WTF::move(value));
}

inline void RenderStyle::setBorderBottom(BorderValue&& value)
{
    m_computedStyle.setBorderBottom(WTF::move(value));
}

inline void RenderStyle::setBorderLeft(BorderValue&& value)
{
    m_computedStyle.setBorderLeft(WTF::move(value));
}

// MARK: - Properties/descriptors that are not yet generated

// FIXME: Support descriptors

inline void RenderStyle::setPageSize(Style::PageSize&& pageSize)
{
    m_computedStyle.setPageSize(WTF::move(pageSize));
}

// MARK: - Style reset utilities

inline void RenderStyle::resetBorderBottom()
{
    m_computedStyle.resetBorderBottom();
}

inline void RenderStyle::resetBorderLeft()
{
    m_computedStyle.resetBorderLeft();
}

inline void RenderStyle::resetBorderRight()
{
    m_computedStyle.resetBorderRight();
}

inline void RenderStyle::resetBorderTop()
{
    m_computedStyle.resetBorderTop();
}

inline void RenderStyle::resetMargin()
{
    m_computedStyle.resetMargin();
}

inline void RenderStyle::resetPadding()
{
    m_computedStyle.resetPadding();
}

inline void RenderStyle::resetBorder()
{
    m_computedStyle.resetBorder();
}

inline void RenderStyle::resetBorderExceptRadius()
{
    m_computedStyle.resetBorderExceptRadius();
}

inline void RenderStyle::resetBorderRadius()
{
    m_computedStyle.resetBorderRadius();
}

} // namespace WebCore
