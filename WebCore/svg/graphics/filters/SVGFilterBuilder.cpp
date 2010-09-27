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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFilterBuilder.h"

#include "FilterEffect.h"
#include "PlatformString.h"
#include "SourceAlpha.h"
#include "SourceGraphic.h"

#include <wtf/PassRefPtr.h>

namespace WebCore {

SVGFilterBuilder::SVGFilterBuilder()
    : m_lastEffect(0)
{
    m_builtinEffects.add(SourceGraphic::effectName(), SourceGraphic::create());
    m_builtinEffects.add(SourceAlpha::effectName(), SourceAlpha::create());
    addBuiltinEffects();
}

void SVGFilterBuilder::add(const AtomicString& id, RefPtr<FilterEffect> effect)
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

FilterEffect* SVGFilterBuilder::getEffectById(const AtomicString& id) const
{
    if (id.isEmpty()) {
        if (m_lastEffect)
            return m_lastEffect.get();

        return m_builtinEffects.get(SourceGraphic::effectName()).get();
    }

    if (m_builtinEffects.contains(id))
        return m_builtinEffects.get(id).get();

    return m_namedEffects.get(id).get();
}

void SVGFilterBuilder::appendEffectToEffectReferences(RefPtr<FilterEffect> effectReference)
{
    // The effect must be a newly created filter effect.
    ASSERT(!m_effectReferences.contains(effectReference));
    m_effectReferences.add(effectReference, FilterEffectSet());

    FilterEffect* effect = effectReference.get();
    unsigned numberOfInputEffects = effect->inputEffects().size();

    // It is not possible to add the same value to a set twice.
    for (unsigned i = 0; i < numberOfInputEffects; ++i)
        getEffectReferences(effect->inputEffect(i)).add(effect);
}

void SVGFilterBuilder::clearEffects()
{
    m_lastEffect = 0;
    m_namedEffects.clear();
    m_effectReferences.clear();
    addBuiltinEffects();
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
