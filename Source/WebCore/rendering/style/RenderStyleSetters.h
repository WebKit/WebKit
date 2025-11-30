/**
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include "PathOperation.h"
#include "RenderStyleInlines.h"

#define RENDER_STYLE_PROPERTIES_SETTERS_INCLUDE_TRAP 1
#include "RenderStylePropertiesSetters.h"
#undef RENDER_STYLE_PROPERTIES_SETTERS_INCLUDE_TRAP

namespace WebCore {

#define SET_STYLE_PROPERTY_BASE(read, value, write) do { if (!compareEqual(read, value)) write; } while (0)
#define SET_STYLE_PROPERTY(read, write, value) SET_STYLE_PROPERTY_BASE(read, value, write = value)
#define SET(group, variable, value) SET_STYLE_PROPERTY(group->variable, group.access().variable, value)
#define SET_NESTED(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent->variable, group.access().parent.access().variable, value)
#define SET_DOUBLY_NESTED(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent->variable, group.access().grandparent.access().parent.access().variable, value)
#define SET_NESTED_STRUCT(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent.variable, group.access().parent.variable, value)
#define SET_STYLE_PROPERTY_PAIR(read, write, variable1, value1, variable2, value2) do { auto& readable = *read; if (!compareEqual(readable.variable1, value1) || !compareEqual(readable.variable2, value2)) { auto& writable = write; writable.variable1 = value1; writable.variable2 = value2; } } while (0)
#define SET_PAIR(group, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group, group.access(), variable1, value1, variable2, value2)
#define SET_NESTED_PAIR(group, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->parent, group.access().parent.access(), variable1, value1, variable2, value2)
#define SET_DOUBLY_NESTED_PAIR(group, grandparent, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->grandparent->parent, group.access().grandparent.access().parent.access(), variable1, value1, variable2, value2)

// MARK: - Non-property setters

inline void RenderStyle::setEffectiveInert(bool effectiveInert) { SET(m_rareInheritedData, effectiveInert, effectiveInert); }
inline void RenderStyle::setIsEffectivelyTransparent(bool effectivelyTransparent) { SET(m_rareInheritedData, effectivelyTransparent, effectivelyTransparent); }
inline void RenderStyle::setEventListenerRegionTypes(OptionSet<EventListenerRegionType> eventListenerTypes) { SET(m_rareInheritedData, eventListenerRegionTypes, eventListenerTypes); }
inline void RenderStyle::setHasAttrContent() { SET_NESTED(m_nonInheritedData, miscData, hasAttrContent, true); }
inline void RenderStyle::setHasDisplayAffectedByAnimations() { SET_NESTED(m_nonInheritedData, miscData, hasDisplayAffectedByAnimations, true); }
inline void RenderStyle::setHasPseudoStyles(EnumSet<PseudoElementType> set) { m_nonInheritedFlags.setHasPseudoStyles(set); }
inline void RenderStyle::setTransformStyleForcedToFlat(bool b) { SET_NESTED(m_nonInheritedData, rareData, transformStyleForcedToFlat, static_cast<unsigned>(b)); }
inline void RenderStyle::setUsesAnchorFunctions() { SET_NESTED(m_nonInheritedData, rareData, usesAnchorFunctions, true); }
inline void RenderStyle::setAnchorFunctionScrollCompensatedAxes(EnumSet<BoxAxis> axes) { SET_NESTED(m_nonInheritedData, rareData, anchorFunctionScrollCompensatedAxes, axes.toRaw()); }
inline void RenderStyle::setIsPopoverInvoker() { SET_NESTED(m_nonInheritedData, rareData, isPopoverInvoker, true); }
inline void RenderStyle::setNativeAppearanceDisabled(bool value) { SET_NESTED(m_nonInheritedData, rareData, nativeAppearanceDisabled, value); }
inline void RenderStyle::setIsForceHidden() { SET(m_rareInheritedData, isForceHidden, true); }
inline void RenderStyle::setAutoRevealsWhenFound() { SET(m_rareInheritedData, autoRevealsWhenFound, true); }
inline void RenderStyle::setInsideDefaultButton(bool value) { SET(m_rareInheritedData, insideDefaultButton, value); }
inline void RenderStyle::setInsideSubmitButton(bool value) { SET(m_rareInheritedData, insideSubmitButton, value); }
inline void RenderStyle::addToTextDecorationLineInEffect(Style::TextDecorationLine value) { m_inheritedFlags.textDecorationLineInEffect = textDecorationLineInEffect().addOrReplaceIfNotNone(value); }
inline void RenderStyle::inheritColumnPropertiesFrom(const RenderStyle& parent) { m_nonInheritedData.access().miscData.access().multiCol = parent.m_nonInheritedData->miscData->multiCol; }

inline void RenderStyle::NonInheritedFlags::setHasPseudoStyles(EnumSet<PseudoElementType> pseudoElementSet)
{
    ASSERT(pseudoElementSet);
    ASSERT(pseudoElementSet.containsOnly(allPublicPseudoElementTypes));
    pseudoBits = pseudoElementSet.toRaw();
}

inline void RenderStyle::setPseudoElementIdentifier(std::optional<Style::PseudoElementIdentifier>&& identifier)
{
    if (identifier) {
        m_nonInheritedFlags.pseudoElementType = enumToUnderlyingType(identifier->type) + 1;
        SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, WTFMove(identifier->nameArgument));
    } else {
        m_nonInheritedFlags.pseudoElementType = 0;
        SET_NESTED(m_nonInheritedData, rareData, pseudoElementNameArgument, nullAtom());
    }
}

// MARK: - Style adjustment utilities

inline void RenderStyle::containIntrinsicWidthAddAuto()
{
    setContainIntrinsicWidth(containIntrinsicWidth().addingAuto());
}

inline void RenderStyle::containIntrinsicHeightAddAuto()
{
    setContainIntrinsicHeight(containIntrinsicHeight().addingAuto());
}

inline void RenderStyle::setGridAutoFlowDirection(Style::GridAutoFlow::Direction direction)
{
    if (!compareEqual(m_nonInheritedData->rareData->grid->gridAutoFlow.direction(), direction))
        m_nonInheritedData.access().rareData.access().grid.access().gridAutoFlow.setDirection(direction);
}

// MARK: - Cache used values

inline void RenderStyle::setUsedAppearance(StyleAppearance a) { SET_NESTED(m_nonInheritedData, miscData, usedAppearance, static_cast<unsigned>(a)); }
inline void RenderStyle::setUsedTouchAction(Style::TouchAction touchAction) { SET(m_rareInheritedData, usedTouchAction, touchAction); }
inline void RenderStyle::setUsedContentVisibility(ContentVisibility usedContentVisibility) { SET(m_rareInheritedData, usedContentVisibility, static_cast<unsigned>(usedContentVisibility)); }
inline void RenderStyle::setUsedZIndex(Style::ZIndex index) { SET_NESTED_PAIR(m_nonInheritedData, boxData, hasAutoUsedZIndex, static_cast<uint8_t>(index.m_isAuto), usedZIndexValue, index.m_value); }
#if HAVE(CORE_MATERIAL)
inline void RenderStyle::setUsedAppleVisualEffectForSubtree(AppleVisualEffect effect) { SET(m_rareInheritedData, usedAppleVisualEffectForSubtree, static_cast<unsigned>(effect)); }
#endif

// MARK: - reset*()

inline void RenderStyle::resetBorderBottom() { SET_NESTED(m_nonInheritedData, surroundData, border.edges().bottom(), BorderValue()); }
inline void RenderStyle::resetBorderBottomLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.radii().bottomLeft(), initialBorderRadius()); }
inline void RenderStyle::resetBorderBottomRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.radii().bottomRight(), initialBorderRadius()); }
inline void RenderStyle::resetBorderImage() { SET_NESTED(m_nonInheritedData, surroundData, border.image(), Style::BorderImage()); }
inline void RenderStyle::resetBorderLeft() { SET_NESTED(m_nonInheritedData, surroundData, border.edges().left(), BorderValue()); }
inline void RenderStyle::resetBorderRight() { SET_NESTED(m_nonInheritedData, surroundData, border.edges().right(), BorderValue()); }
inline void RenderStyle::resetBorderTop() { SET_NESTED(m_nonInheritedData, surroundData, border.edges().top(), BorderValue { }); }
inline void RenderStyle::resetBorderTopLeftRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.radii().topLeft(), initialBorderRadius()); }
inline void RenderStyle::resetBorderTopRightRadius() { SET_NESTED(m_nonInheritedData, surroundData, border.radii().topRight(), initialBorderRadius()); }
inline void RenderStyle::resetColumnRule() { SET_DOUBLY_NESTED(m_nonInheritedData, miscData, multiCol, columnRule, BorderValue()); }
inline void RenderStyle::resetMargin() { SET_NESTED(m_nonInheritedData, surroundData, margin, Style::MarginBox { 0_css_px }); }
inline void RenderStyle::resetPadding() { SET_NESTED(m_nonInheritedData, surroundData, padding, Style::PaddingBox { 0_css_px }); }
inline void RenderStyle::resetPageSize() { SET_NESTED(m_nonInheritedData, rareData, pageSize, Style::PageSize { CSS::Keyword::Auto { } }); }

inline void RenderStyle::resetBorder()
{
    resetBorderExceptRadius();
    resetBorderRadius();
}

inline void RenderStyle::resetBorderExceptRadius()
{
    resetBorderImage();
    resetBorderTop();
    resetBorderRight();
    resetBorderBottom();
    resetBorderLeft();
}

inline void RenderStyle::resetBorderRadius()
{
    resetBorderTopLeftRadius();
    resetBorderTopRightRadius();
    resetBorderBottomLeftRadius();
    resetBorderBottomRightRadius();
}

// MARK: - Logical setters

inline void RenderStyle::setLogicalHeight(Style::PreferredSize&& height)
{
    if (writingMode().isHorizontal())
        setHeight(WTFMove(height));
    else
        setWidth(WTFMove(height));
}

inline void RenderStyle::setLogicalWidth(Style::PreferredSize&& width)
{
    if (writingMode().isHorizontal())
        setWidth(WTFMove(width));
    else
        setHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMinWidth(Style::MinimumSize&& width)
{
    if (writingMode().isHorizontal())
        setMinWidth(WTFMove(width));
    else
        setMinHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMaxWidth(Style::MaximumSize&& width)
{
    if (writingMode().isHorizontal())
        setMaxWidth(WTFMove(width));
    else
        setMaxHeight(WTFMove(width));
}

inline void RenderStyle::setLogicalMinHeight(Style::MinimumSize&& height)
{
    if (writingMode().isHorizontal())
        setMinHeight(WTFMove(height));
    else
        setMinWidth(WTFMove(height));
}

inline void RenderStyle::setLogicalMaxHeight(Style::MaximumSize&& height)
{
    if (writingMode().isHorizontal())
        setMaxHeight(WTFMove(height));
    else
        setMaxWidth(WTFMove(height));
}

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

} // namespace WebCore
