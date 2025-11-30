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

#include "RenderStyleBaseInlines.h"

#define SET_STYLE_PROPERTY_BASE(read, value, write) do { if (!compareEqual(read, value)) write; } while (0)
#define SET_STYLE_PROPERTY(read, write, value) SET_STYLE_PROPERTY_BASE(read, value, write = value)
#define SET(group, variable, value) SET_STYLE_PROPERTY(group->variable, group.access().variable, value)
#define SET_NESTED(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent->variable, group.access().parent.access().variable, value)
#define SET_DOUBLY_NESTED(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent->variable, group.access().grandparent.access().parent.access().variable, value)
#define SET_NESTED_STRUCT(group, parent, variable, value) SET_STYLE_PROPERTY(group->parent.variable, group.access().parent.variable, value)
#define SET_DOUBLY_NESTED_STRUCT(group, grandparent, parent, variable, value) SET_STYLE_PROPERTY(group->grandparent->parent.variable, group.access().grandparent.access().parent.variable, value)
#define SET_STYLE_PROPERTY_PAIR(read, write, variable1, value1, variable2, value2) do { auto& readable = *read; if (!compareEqual(readable.variable1, value1) || !compareEqual(readable.variable2, value2)) { auto& writable = write; writable.variable1 = value1; writable.variable2 = value2; } } while (0)
#define SET_PAIR(group, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group, group.access(), variable1, value1, variable2, value2)
#define SET_NESTED_PAIR(group, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->parent, group.access().parent.access(), variable1, value1, variable2, value2)
#define SET_DOUBLY_NESTED_PAIR(group, grandparent, parent, variable1, value1, variable2, value2) SET_STYLE_PROPERTY_PAIR(group->grandparent->parent, group.access().grandparent.access().parent.access(), variable1, value1, variable2, value2)

namespace WebCore {

template<typename T, typename U> inline bool compareEqual(const T& a, const U& b) { return a == b; }

// MARK: - Non-properties

inline bool RenderStyleBase::setUsedZoom(float zoomLevel)
{
    if (compareEqual(m_rareInheritedData->usedZoom, zoomLevel))
        return false;
    m_rareInheritedData.access().usedZoom = zoomLevel;
    return true;
}

// MARK: - Aggregates

inline Style::Animations& RenderStyleBase::ensureAnimations()
{
    return m_nonInheritedData.access().miscData.access().animations.access();
}

inline Style::Transitions& RenderStyleBase::ensureTransitions()
{
    return m_nonInheritedData.access().miscData.access().transitions.access();
}

inline Style::BackgroundLayers& RenderStyleBase::ensureBackgroundLayers()
{
    return m_nonInheritedData.access().backgroundData.access().background.access();
}

inline Style::MaskLayers& RenderStyleBase::ensureMaskLayers()
{
    return m_nonInheritedData.access().miscData.access().mask.access();
}

inline void RenderStyleBase::setBackgroundLayers(Style::BackgroundLayers&& layers)
{
    SET_NESTED(m_nonInheritedData, backgroundData, background, WTFMove(layers));
}

inline void RenderStyleBase::setMaskLayers(Style::MaskLayers&& layers)
{
    SET_NESTED(m_nonInheritedData, miscData, mask, WTFMove(layers));
}

inline void RenderStyleBase::setMaskBorder(Style::MaskBorder&& image)
{
    SET_NESTED(m_nonInheritedData, rareData, maskBorder, WTFMove(image));
}

inline void RenderStyleBase::setBorderImage(Style::BorderImage&& image)
{
    SET_NESTED(m_nonInheritedData, surroundData, border.image(), WTFMove(image));
}

inline void RenderStyleBase::setPerspectiveOrigin(Style::PerspectiveOrigin&& origin)
{
    SET_NESTED(m_nonInheritedData, rareData, perspectiveOrigin, WTFMove(origin));
}

inline void RenderStyleBase::setTransformOrigin(Style::TransformOrigin&& origin)
{
    SET_DOUBLY_NESTED(m_nonInheritedData, miscData, transform, origin, WTFMove(origin));
}

inline void RenderStyleBase::setInsetBox(Style::InsetBox&& box)
{
    SET_NESTED(m_nonInheritedData, surroundData, inset, WTFMove(box));
}

inline void RenderStyleBase::setMarginBox(Style::MarginBox&& box)
{
    SET_NESTED(m_nonInheritedData, surroundData, margin, WTFMove(box));
}

inline void RenderStyleBase::setPaddingBox(Style::PaddingBox&& box)
{
    SET_NESTED(m_nonInheritedData, surroundData, padding, WTFMove(box));
}

inline void RenderStyleBase::setBorderRadius(Style::BorderRadiusValue&& size)
{
    SET_DOUBLY_NESTED_STRUCT(m_nonInheritedData, surroundData, border, radii(), Style::BorderRadius { WTFMove(size) });
}

// MARK: - Properties/descriptors that are not yet generated

// FIXME: Support descriptors

inline void RenderStyleBase::setPageSize(Style::PageSize&& pageSize)
{
    SET_NESTED(m_nonInheritedData, rareData, pageSize, WTFMove(pageSize));
}

// FIXME: Add a type that encapsulates both caretColor() and hasAutoCaretColor().

inline void RenderStyleBase::setCaretColor(Style::Color&& color)
{
    SET_PAIR(m_rareInheritedData, caretColor, WTFMove(color), hasAutoCaretColor, false);
}

inline void RenderStyleBase::setHasAutoCaretColor()
{
    SET_PAIR(m_rareInheritedData, hasAutoCaretColor, true, caretColor, Style::Color::currentColor());
}

inline void RenderStyleBase::setVisitedLinkCaretColor(Style::Color&& value)
{
    SET_PAIR(m_rareInheritedData, visitedLinkCaretColor, WTFMove(value), hasVisitedLinkAutoCaretColor, false);
}

inline void RenderStyleBase::setHasVisitedLinkAutoCaretColor()
{
    SET_PAIR(m_rareInheritedData, hasVisitedLinkAutoCaretColor, true, visitedLinkCaretColor, Style::Color::currentColor());
}

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
