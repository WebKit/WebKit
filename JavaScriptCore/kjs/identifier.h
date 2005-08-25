/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc
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

#ifndef KJS_IDENTIFIER_H
#define KJS_IDENTIFIER_H

#include "ustring.h"

namespace KJS {

    class Identifier {
        friend class PropertyMap;
    public:
        static void init();

        Identifier() { }
        Identifier(const char *s) : _ustring(add(s)) { }
        Identifier(const UChar *s, int length) : _ustring(add(s, length)) { }
        explicit Identifier(const UString &s) : _ustring(add(s.rep)) { }
        
        const UString &ustring() const { return _ustring; }
        DOM::DOMString domString() const;
        QString qstring() const;
        
        const UChar *data() const { return _ustring.data(); }
        int size() const { return _ustring.size(); }
        
        const char *ascii() const { return _ustring.ascii(); }
        
        static Identifier from(unsigned y) { return Identifier(UString::from(y)); }
        
        bool isNull() const { return _ustring.isNull(); }
        bool isEmpty() const { return _ustring.isEmpty(); }
        
        unsigned long toULong(bool *ok) const { return _ustring.toULong(ok); }
        uint32_t toUInt32(bool *ok) const { return _ustring.toUInt32(ok); }
        uint32_t toStrictUInt32(bool *ok) const { return _ustring.toStrictUInt32(ok); }
        unsigned toArrayIndex(bool *ok) const { return _ustring.toArrayIndex(ok); }
        double toDouble() const { return _ustring.toDouble(); }
        
        static const Identifier &null();
        
        friend bool operator==(const Identifier &, const Identifier &);
        friend bool operator!=(const Identifier &, const Identifier &);

        friend bool operator==(const Identifier &, const char *);
    
        static void remove(UString::Rep *);

    private:
        UString _ustring;
        
        static bool equal(UString::Rep *, const char *);
        static bool equal(UString::Rep *, const UChar *, int length);
        static bool equal(UString::Rep *, UString::Rep *);

        static bool equal(const Identifier &a, const Identifier &b)
            { return a._ustring.rep == b._ustring.rep; }
        static bool equal(const Identifier &a, const char *b)
            { return equal(a._ustring.rep, b); }
        
        static UString::Rep *add(const char *);
        static UString::Rep *add(const UChar *, int length);
        static UString::Rep *add(UString::Rep *);
        
        static void insert(UString::Rep *);
        
        static void rehash(int newTableSize);
        static void expand();
        static void shrink();

        static UString::Rep **_table;
        static int _tableSize;
        static int _tableSizeMask;
        static int _keyCount;
    };
    
#if !KJS_IDENTIFIER_HIDE_GLOBALS
    extern const Identifier nullIdentifier;

    inline const Identifier &Identifier::null()
        { return nullIdentifier; }
#endif

    inline bool operator==(const Identifier &a, const Identifier &b)
        { return Identifier::equal(a, b); }

    inline bool operator!=(const Identifier &a, const Identifier &b)
        { return !Identifier::equal(a, b); }

    inline bool operator==(const Identifier &a, const char *b)
        { return Identifier::equal(a, b); }

    // List of property names, passed to a macro so we can do set them up various
    // ways without repeating the list.
    #define KJS_IDENTIFIER_EACH_PROPERTY_NAME_GLOBAL(macro) \
        macro(arguments) \
        macro(callee) \
        macro(constructor) \
        macro(length) \
        macro(message) \
        macro(name) \
        macro(prototype) \
        macro(toLocaleString) \
        macro(toString) \
        macro(toFixed) \
        macro(toExponential) \
        macro(toPrecision) \
        macro(valueOf)

    // Define external global variables for all property names above (and one more).
#if !KJS_IDENTIFIER_HIDE_GLOBALS
    extern const Identifier specialPrototypePropertyName;

    #define KJS_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL(name) extern const Identifier name ## PropertyName;
    KJS_IDENTIFIER_EACH_PROPERTY_NAME_GLOBAL(KJS_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL)
    #undef KJS_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL
#endif

}

#endif
