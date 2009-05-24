
/*
 *  Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  aint with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "FilterBuilder.h"

#include "FilterEffect.h"
#include "PlatformString.h"
#include "SourceAlpha.h"
#include "SourceGraphic.h"

#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

FilterBuilder::FilterBuilder()
{
    m_builtinEffects.add(SourceGraphic::effectName().impl(), SourceGraphic::create());
    m_builtinEffects.add(SourceAlpha::effectName().impl(), SourceAlpha::create());
}

void FilterBuilder::add(const AtomicString& id, RefPtr<FilterEffect> effect)
{
    if (id.isEmpty()) {
        m_lastEffect = effect.get();
        return;
    }

    AtomicStringImpl* idImpl = id.impl();
    ASSERT(idImpl);

    if (m_builtinEffects.contains(idImpl))
        return;

    m_lastEffect = effect.get();
    m_namedEffects.set(idImpl, m_lastEffect);
}

FilterEffect* FilterBuilder::getEffectById(const AtomicString& id) const
{
    bool idIsEmpty = id.isEmpty();

    if (idIsEmpty && m_lastEffect)
        return m_lastEffect.get();

    AtomicStringImpl* idImpl = id.impl();

    if (m_builtinEffects.contains(idImpl))
        return m_builtinEffects.get(idImpl).get();

    if (idIsEmpty && !m_lastEffect)
        return m_builtinEffects.get(SourceGraphic::effectName().impl()).get();

    return m_namedEffects.get(idImpl).get();
}

void FilterBuilder::clearEffects()
{
    m_lastEffect = 0;
    m_namedEffects.clear();
}

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
