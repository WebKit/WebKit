/*
    Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
                  2009 Dirk Schulze <krit@webkit.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SourceGraphic_h
#define SourceGrahpic_h

#if ENABLE(FILTERS)
#include "FilterEffect.h"

#include "Filter.h"
#include "PlatformString.h"

namespace WebCore {

    class SourceGraphic : public FilterEffect {
    public:        
        static PassRefPtr<SourceGraphic> create();

        static const AtomicString& effectName();

        void apply(Filter*);
        void dump();
        TextStream& externalRepresentation(TextStream&, int indent) const;

        virtual bool isSourceInput() const { return true; }
        virtual FloatRect determineFilterPrimitiveSubregion(Filter*);

    private:
        SourceGraphic() { }
    };
} //namespace WebCore

#endif // ENABLE(FILTERS)

#endif // SourceGraphic_h
