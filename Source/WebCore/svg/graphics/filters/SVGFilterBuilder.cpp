/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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
#include "ImageBuffer.h"
#include "SVGFilterElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SourceAlpha.h"
#include "SourceGraphic.h"
#include <wtf/text/StringConcatenateNumbers.h>

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
#include "CSSComputedStyleDeclaration.h"
#include "CSSPrimitiveValueMappings.h"
#endif

namespace WebCore {

void SVGFilterBuilder::setupBuiltinEffects(Ref<FilterEffect> sourceGraphic)
{
    m_builtinEffects.add(SourceGraphic::effectName(), sourceGraphic.ptr());
    m_builtinEffects.add(SourceAlpha::effectName(), SourceAlpha::create(sourceGraphic));
    addBuiltinEffects();
}

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
static ColorInterpolation colorInterpolationForElement(SVGElement& element)
{
    if (auto renderer = element.renderer())
        return renderer->style().svgStyle().colorInterpolationFilters();

    // Try to determine the property value from the computed style.
    if (auto value = ComputedStyleExtractor(&element).propertyValue(CSSPropertyColorInterpolationFilters)) {
        if (is<CSSPrimitiveValue>(value))
            return downcast<CSSPrimitiveValue>(*value);
    }

    return ColorInterpolation::Auto;
}
#endif

static unsigned collectEffects(const FilterEffect* effect, HashSet<const FilterEffect*>& allEffects)
{
    allEffects.add(effect);
    unsigned size = effect->numberOfEffectInputs();
    for (unsigned i = 0; i < size; ++i) {
        FilterEffect* in = effect->inputEffect(i);
        collectEffects(in, allEffects);
    }
    return allEffects.size();
}

static unsigned totalNumberFilterEffects(const FilterEffect& lastEffect)
{
    HashSet<const FilterEffect*> allEffects;
    return collectEffects(&lastEffect, allEffects);
}

RefPtr<FilterEffect> SVGFilterBuilder::buildFilterEffects(SVGFilterElement& filterElement)
{
    static constexpr unsigned maxCountChildNodes = 200;
    static constexpr unsigned maxTotalNumberFilterEffects = 100;

    if (filterElement.countChildNodes() > maxCountChildNodes)
        return nullptr;

    RefPtr<FilterEffect> effect;

    for (auto& effectElement : childrenOfType<SVGFilterPrimitiveStandardAttributes>(filterElement)) {
        effect = effectElement.build(*this);
        if (!effect) {
            clearEffects();
            return nullptr;
        }

        effectElement.setStandardAttributes(effect.get());
        effect->setEffectBoundaries(SVGLengthContext::resolveRectangle<SVGFilterPrimitiveStandardAttributes>(&effectElement, m_primitiveUnits, m_targetBoundingBox));

#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        if (colorInterpolationForElement(effectElement) == ColorInterpolation::LinearRGB)
            effect->setOperatingColorSpace(DestinationColorSpace::LinearSRGB());
#endif
        if (auto renderer = effectElement.renderer())
            appendEffectToEffectReferences(effect.copyRef(), renderer);

        add(effectElement.result(), effect);
    }

    if (!effect || totalNumberFilterEffects(*effect) > maxTotalNumberFilterEffects)
        return nullptr;

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

void SVGFilterBuilder::appendEffectToEffectReferences(RefPtr<FilterEffect>&& effect, RenderObject* object)
{
    // The effect must be a newly created filter effect.
    ASSERT(!m_effectReferences.contains(effect));
    ASSERT(!object || !m_effectRenderer.contains(object));
    m_effectReferences.add(effect, FilterEffectSet());

    unsigned numberOfInputEffects = effect->inputEffects().size();

    // It is not possible to add the same value to a set twice.
    for (unsigned i = 0; i < numberOfInputEffects; ++i)
        effectReferences(effect->inputEffect(i)).add(effect.get());

    // If object is null, that means the element isn't attached for some
    // reason, which in turn mean that certain types of invalidation will not
    // work (the LayoutObject -> FilterEffect mapping will not be defined).
    if (object)
        m_effectRenderer.add(object, effect.get());
}

void SVGFilterBuilder::clearEffects()
{
    m_lastEffect = nullptr;
    m_namedEffects.clear();
    m_effectReferences.clear();
    m_effectRenderer.clear();
    addBuiltinEffects();
}

void SVGFilterBuilder::clearResultsRecursive(FilterEffect* effect)
{
    if (!effect->hasResult())
        return;

    effect->clearResult();

    for (auto& reference : effectReferences(effect))
        clearResultsRecursive(reference);
}

} // namespace WebCore
