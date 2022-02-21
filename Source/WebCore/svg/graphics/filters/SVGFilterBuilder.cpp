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
    m_builtinEffects.add(SourceGraphic::effectName(), sourceGraphic);
    m_builtinEffects.add(SourceAlpha::effectName(), SourceAlpha::create(sourceGraphic->operatingColorSpace()));
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

    setEffectInputs(sourceAlpha(), FilterEffectVector { sourceGraphic() });

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        auto inputs = namedEffects(effectElement.filterEffectInputsNames());
        if (!inputs)
            return nullptr;

        auto effect = effectElement.filterEffect(*this, *inputs);
        if (!effect)
            return nullptr;

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

        addNamedEffect(effectElement.result(), { *effect });
        setEffectInputs(*effect, WTFMove(*inputs));
    }

    return m_lastEffect;
}

FilterEffect& SVGFilterBuilder::sourceGraphic() const
{
    return *m_builtinEffects.get(FilterEffect::sourceGraphicName());
}

FilterEffect& SVGFilterBuilder::sourceAlpha() const
{
    return *m_builtinEffects.get(FilterEffect::sourceAlphaName());
}

void SVGFilterBuilder::addNamedEffect(const AtomString& id, Ref<FilterEffect>&& effect)
{
    if (id.isEmpty()) {
        m_lastEffect = WTFMove(effect);
        return;
    }

    if (m_builtinEffects.contains(id))
        return;

    m_lastEffect = WTFMove(effect);
    m_namedEffects.set(id, Ref { *m_lastEffect });
}

RefPtr<FilterEffect> SVGFilterBuilder::namedEffect(const AtomString& id) const
{
    if (id.isEmpty()) {
        if (m_lastEffect)
            return m_lastEffect;

        return &sourceGraphic();
    }

    if (m_builtinEffects.contains(id))
        return m_builtinEffects.get(id);

    return m_namedEffects.get(id);
}

std::optional<FilterEffectVector> SVGFilterBuilder::namedEffects(Span<const AtomString> names) const
{
    FilterEffectVector effects;

    effects.reserveInitialCapacity(names.size());

    for (auto& name : names) {
        auto effect = namedEffect(name);
        if (!effect)
            return std::nullopt;

        effects.uncheckedAppend(effect.releaseNonNull());
    }

    return effects;
}

void SVGFilterBuilder::setEffectInputs(FilterEffect& effect, FilterEffectVector&& inputs)
{
    m_inputsMap.set({ effect }, WTFMove(inputs));
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

    for (auto& input : m_inputsMap.get(effect)) {
        if (!buildEffectExpression(input, stack, level + 1, expression))
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
