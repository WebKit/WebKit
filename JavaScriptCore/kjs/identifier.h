/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Apple Computer, Inc
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
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
        Identifier(const char *s) : _ustring(s) { }
        Identifier(const UString &s) : _ustring(s) { }
        
        const UString &ustring() const { return _ustring; }
        DOM::DOMString string() const;
        QString qstring() const;
        
        const UChar *data() const { return _ustring.data(); }
        int size() const { return _ustring.size(); }
        
        const char *ascii() const { return _ustring.ascii(); }
        
        static Identifier from(unsigned y) { return UString::from(y); }
        
        bool isNull() const { return _ustring.isNull(); }
        bool isEmpty() const { return _ustring.isEmpty(); }
        
        unsigned long toULong(bool *ok) const { return _ustring.toULong(ok); }
        double toDouble() const { return _ustring.toDouble(); }
        
        static Identifier null;
        
        friend bool operator==(const Identifier &, const Identifier &);
        friend bool operator!=(const Identifier &, const Identifier &);

        friend bool operator==(const Identifier &, const char *);
    
    private:
        UString _ustring;
    };
    
    inline bool operator==(const Identifier &a, const Identifier &b)
    {
        return a._ustring == b._ustring;
    }

    inline bool operator!=(const Identifier &a, const Identifier &b)
    {
        return a._ustring != b._ustring;
    }

}

#endif
