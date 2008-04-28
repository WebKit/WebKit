// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_PROPERTY_NAME_ARRAY_H
#define KJS_PROPERTY_NAME_ARRAY_H

#include "identifier.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace KJS {

    class PropertyNameArray {
    public:
        typedef Identifier ValueType;
        typedef Vector<Identifier>::const_iterator const_iterator;

        void add(const Identifier& identifier) { add(identifier.ustring().rep()); }
        void add(UString::Rep*);
        void addKnownUnique(UString::Rep* identifier) { m_vector.append(identifier); }
        const_iterator begin() const { return m_vector.begin(); }
        const_iterator end() const { return m_vector.end(); }
        size_t size() const { return m_vector.size(); }

        Identifier& operator[](unsigned i) { return m_vector[i]; }
        const Identifier& operator[](unsigned i) const { return m_vector[i]; }

    private:
        typedef HashSet<UString::Rep*, PtrHash<UString::Rep*> > IdentifierSet;

        Vector<Identifier, 20> m_vector;
        IdentifierSet m_set;
    };

} // namespace KJS


#endif // KJS_PROPERTY_NAME_ARRAY_H
