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

#include "StyleComputedStyleBase+GettersInlines.h"

#define SET_STYLE_PROPERTY_BASE(read, value, write) do { if (!compareEqual(read, value)) write; } while (0)
#define SET_STYLE_PROPERTY(read, write, value) SET_STYLE_PROPERTY_BASE(read, value, write = value)
#define SET(group, variable, value) SET_STYLE_PROPERTY(group->variable, group.access().variable, value)
#define SET_NESTED(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent->variable, group.access().parent.access().variable, value)
#define SET_DOUBLY_NESTED(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent->variable, group.access().grandparent.access().parent.access().variable, value)
#define SET_NESTED_STRUCT(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent.variable, group.access().parent.variable, value)
#define SET_STYLE_PROPERTY_PAIR(read, write, variable1, value1, variable2, value2) do { Ref readable = Ref { *read }; if (!compareEqual(readable->variable1, value1) || !compareEqual(readable->variable2, value2)) { Ref writable = write; writable->variable1 = value1; writable->variable2 = value2; } } while (0)
#define SET_PAIR(group, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group, group.access(), variable1, value1, variable2, value2)
#define SET_NESTED_PAIR(group, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->parent, group.access().parent.access(), variable1, value1, variable2, value2)
#define SET_DOUBLY_NESTED_PAIR(group, grandparent, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->grandparent->parent, group.access().grandparent.access().parent.access(), variable1, value1, variable2, value2)

namespace WebCore {
namespace Style {

template<typename T, typename U> inline bool compareEqual(const T& a, const U& b)
{
     return a == b;
}

// MARK: - ComputedStyleBase::NonInheritedFlags

inline void ComputedStyleBase::NonInheritedFlags::setHasPseudoStyles(EnumSet<PseudoElementType> pseudoElementSet)
{
    ASSERT(pseudoElementSet);
    ASSERT(pseudoElementSet.containsOnly(allPublicPseudoElementTypes));
    pseudoBits = pseudoElementSet.toRaw();
}

// MARK: - Non-property setters

inline void ComputedStyleBase::setUsesViewportUnits()
{
    m_nonInheritedFlags.usesViewportUnits = true;
}

inline void ComputedStyleBase::setUsesContainerUnits()
{
    m_nonInheritedFlags.usesContainerUnits = true;
}

inline void ComputedStyleBase::setUsesTreeCountingFunctions()
{
    m_nonInheritedFlags.useTreeCountingFunctions = true;
}

inline void ComputedStyleBase::setInsideLink(InsideLink insideLink)
{
    m_inheritedFlags.insideLink = static_cast<unsigned>(insideLink);
}

inline void ComputedStyleBase::setIsLink(bool isLink)
{
    m_nonInheritedFlags.isLink = isLink;
}

inline void ComputedStyleBase::setEmptyState(bool emptyState)
{
    m_nonInheritedFlags.emptyState = emptyState;
}

inline void ComputedStyleBase::setFirstChildState()
{
    m_nonInheritedFlags.firstChildState = true;
}

inline void ComputedStyleBase::setLastChildState()
{
    m_nonInheritedFlags.lastChildState = true;
}

inline void ComputedStyleBase::setHasExplicitlyInheritedProperties()
{
    m_nonInheritedFlags.hasExplicitlyInheritedProperties = true;
}

inline void ComputedStyleBase::setDisallowsFastPathInheritance()
{
    m_nonInheritedFlags.disallowsFastPathInheritance = true;
}

inline void ComputedStyleBase::setEffectiveInert(bool effectiveInert)
{
    SET(m_inheritedRareData, effectiveInert, effectiveInert);
}

inline void ComputedStyleBase::setIsEffectivelyTransparent(bool effectivelyTransparent)
{
    SET(m_inheritedRareData, effectivelyTransparent, effectivelyTransparent);
}

inline void ComputedStyleBase::setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes)
{
    SET(m_inheritedRareData, eventListenerRegionTypes, eventListenerTypes);
}

inline void ComputedStyleBase::setHasAttrContent()
{
    SET_NESTED(m_nonInheritedData, miscData, hasAttrContent, true);
}

inline void ComputedStyleBase::setHasDisplayAffectedByAnimations()
{
    SET_NESTED(m_nonInheritedData, miscData, hasDisplayAffectedByAnimations, true);
}

inline void ComputedStyleBase::setTransformStyleForcedToFlat(bool b)
{
    SET_NESTED(m_nonInheritedData, rareData, transformStyleForcedToFlat, static_cast<unsigned>(b));
}

inline void ComputedStyleBase::setUsesAnchorFunctions()
{
    SET_NESTED(m_nonInheritedData, rareData, usesAnchorFunctions, true);
}

inline void ComputedStyleBase::setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis> axes)
{
    SET_NESTED(m_nonInheritedData, rareData, anchorFunctionScrollCompensatedAxes, axes.toRaw());
}

inline void ComputedStyleBase::setIsPopoverInvoker()
{
    SET_NESTED(m_nonInheritedData, rareData, isPopoverInvoker, true);
}

inline void ComputedStyleBase::setNativeAppearanceDisabled(bool value)
{
    SET_NESTED(m_nonInheritedData, rareData, nativeAppearanceDisabled, value);
}

inline void ComputedStyleBase::setIsForceHidden()
{
    SET(m_inheritedRareData, isForceHidden, true);
}

inline void ComputedStyleBase::setAutoRevealsWhenFound()
{
    SET(m_inheritedRareData, autoRevealsWhenFound, true);
}

inline void ComputedStyleBase::setInsideDefaultButton(bool value)
{
    SET(m_inheritedRareData, insideDefaultButton, value);
}

inline void ComputedStyleBase::setInsideSubmitButton(bool value)
{
    SET(m_inheritedRareData, insideSubmitButton, value);
}

inline void ComputedStyleBase::setUsedPositionOptionIndex(std::optional<size_t> index)
{
    SET_NESTED(m_nonInheritedData, rareData, usedPositionOptionIndex, index);
}

inline void ComputedStyleBase::setEffectiveDisplay(DisplayType effectiveDisplay)
{
    m_nonInheritedFlags.effectiveDisplay = static_cast<unsigned>(effectiveDisplay);
}

inline void ComputedStyleBase::setUsedAppearance(StyleAppearance a)
{
    SET_NESTED(m_nonInheritedData, miscData, usedAppearance, static_cast<unsigned>(a));
}

inline void ComputedStyleBase::setUsedContentVisibility(ContentVisibility usedContentVisibility)
{
    SET(m_inheritedRareData, usedContentVisibility, static_cast<unsigned>(usedContentVisibility));
}

inline void ComputedStyleBase::setUsedTouchAction(TouchAction touchAction)
{
    SET(m_inheritedRareData, usedTouchAction, touchAction);
}

inline void ComputedStyleBase::setUsedZIndex(ZIndex index)
{
    SET_NESTED_PAIR(m_nonInheritedData, boxData, hasAutoUsedZIndex, static_cast<uint8_t>(index.m_isAuto), usedZIndexValue, index.m_value);
}

#if HAVE(CORE_MATERIAL)

inline void ComputedStyleBase::setUsedAppleVisualEffectForSubtree(AppleVisualEffect effect)
{
    SET(m_inheritedRareData, usedAppleVisualEffectForSubtree, static_cast<unsigned>(effect));
}

#endif

// MARK: - Pseudo element/style

inline void ComputedStyleBase::setHasPseudoStyles(EnumSet<PseudoElementType> set)
{
    m_nonInheritedFlags.setHasPseudoStyles(set);
}

inline void ComputedStyleBase::setPseudoElementIdentifier(std::optional<PseudoElementIdentifier>&& identifier)
{
    if (identifier) {
        m_nonInheritedFlags.pseudoElementType = enumToUnderlyingType(identifier->type) + 1;
        SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, WTF::move(identifier->nameArgument));
    } else {
        m_nonInheritedFlags.pseudoElementType = 0;
        SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, nullAtom());
    }
}

// MARK: - Zoom

inline void ComputedStyleBase::setEvaluationTimeZoomEnabled(bool value)
{
    SET(m_inheritedRareData, evaluationTimeZoomEnabled, value);
}

inline void ComputedStyleBase::setDeviceScaleFactor(float value)
{
    SET(m_inheritedRareData, deviceScaleFactor, value);
}

inline void ComputedStyleBase::setUseSVGZoomRulesForLength(bool value)
{
    SET_NESTED(m_nonInheritedData, rareData, useSVGZoomRulesForLength, value);
}

inline bool ComputedStyleBase::setUsedZoom(float zoomLevel)
{
    if (compareEqual(m_inheritedRareData->usedZoom, zoomLevel))
        return false;
    m_inheritedFlags.isZoomed = zoomLevel != 1.0f;
    m_inheritedRareData.access().usedZoom = zoomLevel;
    return true;
}

// MARK: - Aggregates

inline Animations& ComputedStyleBase::ensureAnimations()
{
    return m_nonInheritedData.access().miscData.access().animations.access();
}

inline Transitions& ComputedStyleBase::ensureTransitions()
{
    return m_nonInheritedData.access().miscData.access().transitions.access();
}

inline BackgroundLayers& ComputedStyleBase::ensureBackgroundLayers()
{
    return m_nonInheritedData.access().backgroundData.access().background.access();
}

inline MaskLayers& ComputedStyleBase::ensureMaskLayers()
{
    return m_nonInheritedData.access().miscData.access().mask.access();
}

inline void ComputedStyleBase::setBackgroundLayers(BackgroundLayers&& layers)
{
    SET_NESTED(m_nonInheritedData, backgroundData, background, WTF::move(layers));
}

inline void ComputedStyleBase::setMaskLayers(MaskLayers&& layers)
{
    SET_NESTED(m_nonInheritedData, miscData, mask, WTF::move(layers));
}

inline void ComputedStyleBase::setMaskBorder(MaskBorder&& image)
{
    SET_DOUBLY_NESTED(m_nonInheritedData, rareData, maskBorder, maskBorder, WTF::move(image));
}

inline void ComputedStyleBase::setBorderImage(BorderImage&& image)
{
    SET_DOUBLY_NESTED(m_nonInheritedData, surroundData, border.borderImage, borderImage, WTF::move(image));
}

inline void ComputedStyleBase::setPerspectiveOrigin(PerspectiveOrigin&& origin)
{
    SET_NESTED(m_nonInheritedData, rareData, perspectiveOrigin, WTF::move(origin));
}

inline void ComputedStyleBase::setTransformOrigin(TransformOrigin&& origin)
{
    SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, origin, WTF::move(origin));
}

inline void ComputedStyleBase::setInsetBox(InsetBox&& box)
{
    SET_NESTED(m_nonInheritedData, surroundData, inset, WTF::move(box));
}

inline void ComputedStyleBase::setMarginBox(MarginBox&& box)
{
    SET_NESTED(m_nonInheritedData, surroundData, margin, WTF::move(box));
}

inline void ComputedStyleBase::setPaddingBox(PaddingBox&& box)
{
    SET_NESTED(m_nonInheritedData, surroundData, padding, WTF::move(box));
}

inline void ComputedStyleBase::setBorderRadius(BorderRadiusValue&& size)
{
    SET_NESTED(m_nonInheritedData, surroundData, border.topLeftRadius(), size);
    SET_NESTED(m_nonInheritedData, surroundData, border.topRightRadius(), size);
    SET_NESTED(m_nonInheritedData, surroundData, border.bottomLeftRadius(), size);
    SET_NESTED(m_nonInheritedData, surroundData, border.bottomRightRadius(), WTF::move(size));
}

void ComputedStyleBase::setBorderTop(BorderValue&& value)
{
    SET_NESTED(m_nonInheritedData, surroundData, border.edges.top(), WTF::move(value));
}

void ComputedStyleBase::setBorderRight(BorderValue&& value)
{
    SET_NESTED(m_nonInheritedData, surroundData, border.edges.right(), WTF::move(value));
}

void ComputedStyleBase::setBorderBottom(BorderValue&& value)
{
    SET_NESTED(m_nonInheritedData, surroundData, border.edges.bottom(), WTF::move(value));
}

void ComputedStyleBase::setBorderLeft(BorderValue&& value)
{
    SET_NESTED(m_nonInheritedData, surroundData, border.edges.left(), WTF::move(value));
}

// MARK: - Properties/descriptors that are not yet generated

// FIXME: Support descriptors

inline void ComputedStyleBase::setPageSize(PageSize&& pageSize)
{
    SET_NESTED(m_nonInheritedData, rareData, pageSize, WTF::move(pageSize));
}

} // namespace Style
} // namespace WebCore

#undef SET
#undef SET_DOUBLY_NESTED
#undef SET_DOUBLY_NESTED_PAIR
#undef SET_NESTED
#undef SET_NESTED_PAIR
#undef SET_NESTED_STRUCT
#undef SET_PAIR
#undef SET_STYLE_PROPERTY
#undef SET_STYLE_PROPERTY_BASE
#undef SET_STYLE_PROPERTY_PAIR
