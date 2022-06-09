/*
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

#include "config.h"
#include "SVGFilterBuilder.h"

#include "ElementIterator.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SourceAlpha.h"
#include "SourceGraphic.h"

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
#include "CSSComputedStyleDeclaration.h"
#include "CSSPrimitiveValueMappings.h"
#endif

namespace WebCore {

static constexpr unsigned maxTotalNumberFilterEffects = 100;
static constexpr unsigned maxCountChildNodes = 200;

void SVGFilterBuilder::setupBuiltinEffects(Ref<FilterEffect> sourceGraphic)
{
    m_builtinEffects.add(SourceGraphic::effectName(), sourceGraphic.ptr());
    m_builtinEffects.add(SourceAlpha::effectName(), SourceAlpha::create(sourceGraphic));
}

static OptionSet<FilterEffectGeometry::Flags> effectGeometryFlagsForElement(SVGElement& element)
{
    OptionSet<FilterEffectGeometry::Flags> flags;

    if (element.hasAttribute(SVGNames::xAttr))
        flags.add(FilterEffectGeometry::Flags::HasX);

    if (element.hasAttribute(SVGNames::yAttr))
        flags.add(FilterEffectGeometry::Flags::HasY);

    if (element.hasAttribute(SVGNames::widthAttr))
        flags.add(FilterEffectGeometry::Flags::HasWidth);

    if (element.hasAttribute(SVGNames::heightAttr))
        flags.add(FilterEffectGeometry::Flags::HasHeight);

    return flags;
}

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
static ColorInterpolation colorInterpolationForElement(SVGElement& element)
{
    if (auto renderer = element.renderer())
        return renderer->style().svgStyle().colorInterpolationFilters();

    // Try to determine the property value from the computed style.
    if (auto value = ComputedStyleExtractor(&element).propertyValue(CSSPropertyColorInterpolationFilters, DoNotUpdateLayout)) {
        if (is<CSSPrimitiveValue>(value))
            return downcast<CSSPrimitiveValue>(*value);
    }

    return ColorInterpolation::Auto;
}
#endif

RefPtr<FilterEffect> SVGFilterBuilder::buildFilterEffects(SVGFilterElement& filterElement)
{
    if (filterElement.countChildNodes() > maxCountChildNodes)
        return nullptr;

    RefPtr<FilterEffect> effect;

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        effect = effectElement.build(*this);
        if (!effect)
            break;

        if (auto flags = effectGeometryFlagsForElement(effectElement)) {
            auto effectBoundaries = SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(&effectElement, m_primitiveUnits, m_targetBoundingBox);
            m_effectGeometryMap.add(*effect, FilterEffectGeometry(effectBoundaries, flags));
        }

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        if (colorInterpolationForElement(effectElement) == ColorInterpolation::LinearRGB)
            effect->setOperatingColorSpace(DestinationColorSpace::LinearSRGB());
#endif

        if (auto renderer = effectElement.renderer())
            appendEffectToEffectRenderer(*effect, *renderer);

        add(effectElement.result(), effect);
    }

    return effect;
}

void SVGFilterBuilder::add(const AtomString& id, RefPtr<FilterEffect> effect)
{
    if (id.isEmpty()) {
        m_lastEffect = effect;
        return;
    }

    if (m_builtinEffects.contains(id))
        return;

    m_lastEffect = effect;
    m_namedEffects.set(id, m_lastEffect);
}

RefPtr<FilterEffect> SVGFilterBuilder::getEffectById(const AtomString& id) const
{
    if (id.isEmpty()) {
        if (m_lastEffect)
            return m_lastEffect;

        return m_builtinEffects.get(SourceGraphic::effectName());
    }

    if (m_builtinEffects.contains(id))
        return m_builtinEffects.get(id);

    return m_namedEffects.get(id);
}

void SVGFilterBuilder::appendEffectToEffectRenderer(FilterEffect& effect, RenderObject& object)
{
    m_effectRenderer.add(&object, &effect);
}

std::optional<FilterEffectGeometry> SVGFilterBuilder::effectGeometry(FilterEffect& effect) const
{
    auto it = m_effectGeometryMap.find(effect);
    if (it != m_effectGeometryMap.end())
        return it->value;
    return std::nullopt;
}

bool SVGFilterBuilder::buildEffectExpression(FilterEffect& effect, FilterEffectVector& stack, unsigned level, SVGFilterExpression& expression) const
{
    // A cycle is detected.
    if (stack.contains(effect))
        return false;

    stack.append(effect);
    
    expression.append({ effect, effectGeometry(effect), level });

    for (auto& inputEffect : effect.inputEffects()) {
        if (!buildEffectExpression(inputEffect, stack, level + 1, expression))
            return false;
    }

    ASSERT(!stack.isEmpty());
    ASSERT(stack.last() == effect);

    stack.removeLast();
    return true;
}

bool SVGFilterBuilder::buildExpression(SVGFilterExpression& expression) const
{
    if (!m_lastEffect)
        return false;

    FilterEffectVector stack;
    if (!buildEffectExpression(*m_lastEffect, stack, 0, expression))
        return false;

    if (expression.size() > maxTotalNumberFilterEffects)
        return false;

    expression.reverse();
    expression.shrinkToFit();
    return true;
}

} // namespace WebCore
