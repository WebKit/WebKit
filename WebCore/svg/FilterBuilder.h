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

#ifndef FilterBuilder_h
#define FilterBuilder_h

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "PlatformString.h"
#include "FilterEffect.h"
#include "Filter.h"

#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {
    
    class FilterBuilder : public RefCounted<FilterBuilder> {
    public:
        void add(const String& id, PassRefPtr<FilterEffect> effect) { m_namedEffects.set(id.impl(), effect); }
        FilterEffect* getEffectById(const String& id) const { return m_namedEffects.get(id.impl()).get(); }
        
        PassRefPtr<Filter> filter() const { return m_filter; }
        
    private:
        HashMap<StringImpl*, RefPtr<FilterEffect> > m_namedEffects;
        
        RefPtr<Filter> m_filter;
    };
    
} //namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
#endif
