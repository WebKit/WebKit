// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2006 Apple Computer, Inc
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
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_PROPERTY_NAME_ARRAY_H
#define KJS_PROPERTY_NAME_ARRAY_H

#include "identifier.h"

#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace KJS {

    class PropertyNameArray;

    typedef Vector<Identifier>::const_iterator PropertyNameArrayIterator;

    class PropertyNameArray {
    public:
        typedef PropertyNameArrayIterator iterator;

        void add(const Identifier&);
        iterator begin() const { return m_vector.begin(); }
        iterator end() const { return m_vector.end(); }
        int size() const { return m_vector.size(); }

        Identifier& operator[](unsigned i) { return m_vector[i]; }
        const Identifier& operator[](unsigned i) const { return m_vector[i]; }
            
    private:
        typedef HashSet<UString::Rep*, PtrHash<UString::Rep*> > IdentifierSet;
        IdentifierSet m_set;
        Vector<Identifier> m_vector;
    };


} // namespace KJS


#endif // KJS_PROPERTY_NAME_ARRAY_H
