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

#include "FilterEffect.h"
#include "FilterEffectVector.h"
#include "SVGFilterExpression.h"
#include "SVGUnitTypes.h"
#include <wtf/HashMap.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FilterEffect;
class RenderObject;
class SVGFilterElement;

class SVGFilterBuilder {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGFilterBuilder() = default;

    void setTargetBoundingBox(const FloatRect& r) { m_targetBoundingBox = r; }
    FloatRect targetBoundingBox() const { return m_targetBoundingBox; }
    
    SVGUnitTypes::SVGUnitType primitiveUnits() const { return m_primitiveUnits; }
    void setPrimitiveUnits(SVGUnitTypes::SVGUnitType units) { m_primitiveUnits = units; }

    // Required to change the attributes of a filter during an svgAttributeChanged.
    void appendEffectToEffectRenderer(FilterEffect&, RenderObject&);
    inline FilterEffect* effectByRenderer(RenderObject* object) { return m_effectRenderer.get(object); }

    void setupBuiltinEffects(Ref<FilterEffect> sourceGraphic);
    RefPtr<FilterEffect> buildFilterEffects(SVGFilterElement&);
    bool buildExpression(SVGFilterExpression&) const;

private:
    FilterEffect& sourceGraphic() const;
    FilterEffect& sourceAlpha() const;

    RefPtr<FilterEffect> namedEffect(const AtomString&) const;
    std::optional<FilterEffectVector> namedEffects(Span<const AtomString>) const;

    void addNamedEffect(const AtomString& id, Ref<FilterEffect>&&);
    void setEffectInputs(FilterEffect&, FilterEffectVector&& inputs);

    std::optional<FilterEffectGeometry> effectGeometry(FilterEffect&) const;
    bool buildEffectExpression(FilterEffect&, FilterEffectVector& stack, unsigned level, SVGFilterExpression&) const;

    HashMap<AtomString, Ref<FilterEffect>> m_builtinEffects;
    HashMap<AtomString, Ref<FilterEffect>> m_namedEffects;
    HashMap<RenderObject*, FilterEffect*> m_effectRenderer;
    HashMap<Ref<FilterEffect>, FilterEffectVector> m_inputsMap;

    RefPtr<FilterEffect> m_lastEffect;

    FloatRect m_targetBoundingBox;
    SVGUnitTypes::SVGUnitType m_primitiveUnits { SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE };
    FilterEffectGeometryMap m_effectGeometryMap;
};
    
} // namespace WebCore
