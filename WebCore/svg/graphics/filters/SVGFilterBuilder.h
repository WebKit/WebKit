/*
    Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
    
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGFilterBuilder_h
#define SVGFilterBuilder_h

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "AtomicStringHash.h"
#include "FilterEffect.h"
#include "PlatformString.h"

#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {
    
    class SVGFilterBuilder : public RefCounted<SVGFilterBuilder> {
    public:
        SVGFilterBuilder();

        void add(const AtomicString& id, RefPtr<FilterEffect> effect);

        FilterEffect* getEffectById(const AtomicString& id) const;
        FilterEffect* lastEffect() const { return m_lastEffect.get(); }

        void clearEffects();

    private:
        HashMap<AtomicString, RefPtr<FilterEffect> > m_builtinEffects;
        HashMap<AtomicString, RefPtr<FilterEffect> > m_namedEffects;

        RefPtr<FilterEffect> m_lastEffect;
    };
    
} //namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
#endif
