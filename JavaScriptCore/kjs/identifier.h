/*
 *  Copyright (C) 2003, 2006, 2007 Apple Inc. All rights reserved.
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

#ifndef KJS_IDENTIFIER_H
#define KJS_IDENTIFIER_H

#include "ustring.h"

namespace KJS {

    class Identifier {
        friend class PropertyMap;
    public:
        Identifier() { }
        Identifier(const char* s) : _ustring(add(s)) { }
        Identifier(const UChar* s, int length) : _ustring(add(s, length)) { }
        explicit Identifier(UString::Rep* rep) : _ustring(add(rep)) { } 
        explicit Identifier(const UString& s) : _ustring(add(s.rep())) { }

        // Special constructor for cases where we overwrite an object in place.
        Identifier(PlacementNewAdoptType) : _ustring(PlacementNewAdopt) { }
        
        const UString& ustring() const { return _ustring; }
        DOM::DOMString domString() const;
        
        const UChar* data() const { return _ustring.data(); }
        int size() const { return _ustring.size(); }
        
        const char* ascii() const { return _ustring.ascii(); }
        
        static Identifier from(unsigned y) { return Identifier(UString::from(y)); }
        
        bool isNull() const { return _ustring.isNull(); }
        bool isEmpty() const { return _ustring.isEmpty(); }
        
        uint32_t toUInt32(bool* ok) const { return _ustring.toUInt32(ok); }
        uint32_t toUInt32(bool* ok, bool tolerateEmptyString) const { return _ustring.toUInt32(ok, tolerateEmptyString); };
        uint32_t toStrictUInt32(bool* ok) const { return _ustring.toStrictUInt32(ok); }
        unsigned toArrayIndex(bool* ok) const { return _ustring.toArrayIndex(ok); }
        double toDouble() const { return _ustring.toDouble(); }
        
        friend bool operator==(const Identifier&, const Identifier&);
        friend bool operator!=(const Identifier&, const Identifier&);

        friend bool operator==(const Identifier&, const char*);
    
        static void remove(UString::Rep* );

        static bool equal(const UString::Rep*, const char*);
        static bool equal(const UString::Rep*, const UChar*, int length);
        static bool equal(const UString::Rep*, const UString::Rep*);

    private:
        UString _ustring;
        
        static bool equal(const Identifier& a, const Identifier& b)
            { return a._ustring.rep() == b._ustring.rep(); }
        static bool equal(const Identifier& a, const char* b)
            { return equal(a._ustring.rep(), b); }
        
        static PassRefPtr<UString::Rep> add(const char*);
        static PassRefPtr<UString::Rep> add(const UChar*, int length);
        static PassRefPtr<UString::Rep> add(UString::Rep* r)
        {
            if (r->isIdentifier)
                return r;
            return addSlowCase(r);
        }
        static PassRefPtr<UString::Rep> addSlowCase(UString::Rep *r);
    };
    
    inline bool operator==(const Identifier& a, const Identifier& b)
        { return Identifier::equal(a, b); }

    inline bool operator!=(const Identifier& a, const Identifier& b)
        { return !Identifier::equal(a, b); }

    inline bool operator==(const Identifier& a, const char* b)
        { return Identifier::equal(a, b); }

} // namespace KJS

#endif // KJS_IDENTIFIER_H
