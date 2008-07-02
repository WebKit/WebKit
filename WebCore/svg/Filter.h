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

#ifndef Filter_h
#define Filter_h

#include "config.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "FilterEffect.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

    class Filter : public RefCounted<Filter> {
    public:    
        static PassRefPtr<Filter> create(FilterEffect*);
        
    private:
        Filter(FilterEffect* effect);
    
        RefPtr<FilterEffect> m_effect;
    };
    
} //namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
#endif
