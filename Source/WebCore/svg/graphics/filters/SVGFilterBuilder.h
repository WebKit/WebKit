/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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

#include "SVGFilterExpression.h"
#include "SVGUnitTypes.h"
#include <wtf/HashMap.h>

namespace WebCore {

class FilterEffect;
class GraphicsContext;
class RenderObject;
class SVGFilter;
class SVGFilterElement;

class SVGFilterBuilder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGFilterBuilder() = default;

    std::optional<SVGFilterExpression> buildFilterExpression(SVGFilterElement&, const SVGFilter&, const GraphicsContext& destinationContext);
    static IntOutsets calculateFilterOutsets(SVGFilterElement&, const FloatRect& targetBoundingBox);

    // Required to change the attributes of a filter during an svgAttributeChanged.
    FilterEffect* effectByRenderer(RenderObject* object) { return m_effectRenderer.get(object); }

private:
    HashMap<RenderObject*, FilterEffect*> m_effectRenderer;
};
    
} // namespace WebCore
