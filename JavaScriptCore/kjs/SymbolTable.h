// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 2007 Apple Computer, Inc.
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

#include "property_map.h"

namespace KJS {

    class JSValue;

    // SymbolTable is implemented in terms of a PropertyMap hack for now, 
    // but it should move to WTF::HashMap once that's fast enough.

    class SymbolTable : private PropertyMap {
    public:
        void set(const Identifier& name, size_t index)
        {
            JSValue* v = reinterpret_cast<JSValue*>(index + 1); // Increment index by 1 because PropertyMap uses 0 to mean "not found."
            ASSERT(v); // Check for overflow.
            PropertyMap::put(name, v, 0, false);
        }

        bool get(const Identifier& name, size_t& index)
        {
            if (JSValue* v = PropertyMap::get(name)) {
                index = reinterpret_cast<uintptr_t>(v) - 1; // Decrement index by 1 to balance the increment in set.
                return true;
            }
            return false;
        }
    };

} // namespace KJS
