/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef _DOM_DOMStringImpl_h_
#define _DOM_DOMStringImpl_h_

#include <qstring.h>

#include "dom/dom_misc.h"
#include "misc/khtmllayout.h"
#include "misc/shared.h"

#define QT_ALLOC_QCHAR_VEC( N ) (QChar*) new char[ sizeof(QChar)*( N ) ]
#define QT_DELETE_QCHAR_VEC( P ) delete[] ((char*)( P ))

namespace DOM {

class DOMStringImpl : public khtml::Shared<DOMStringImpl>
{
protected:
    DOMStringImpl() { s = 0, l = 0; }
public:
    DOMStringImpl(const QChar *str, unsigned int len) {
	bool havestr = str && len;
	s = QT_ALLOC_QCHAR_VEC( havestr ? len : 1 );
	if(str && len) {
	    memcpy( s, str, len * sizeof(QChar) );
	    l = len;
	} else {
	    // crash protection
	    s[0] = 0x0;
	    l = 0;
	}
    }

    DOMStringImpl(const char *str);
    DOMStringImpl(const QChar &ch) {
	s = QT_ALLOC_QCHAR_VEC( 1 );
	s[0] = ch;
	l = 1;
    }
    ~DOMStringImpl() {
	if(s) QT_DELETE_QCHAR_VEC(s);
    }

    void append(DOMStringImpl *str);
    void insert(DOMStringImpl *str, unsigned int pos);
    void truncate(int len);
    void remove(unsigned int pos, int len=1);
    DOMStringImpl *split(unsigned int pos);
    DOMStringImpl *copy() const {
        return new DOMStringImpl(s,l);
    };

    DOMStringImpl *substring(unsigned int pos, unsigned int len);

    const QChar &operator [] (int pos)
	{ return *(s+pos); }

    khtml::Length toLength() const;
    
    bool containsOnlyWhitespace() const;
    
    // ignores trailing garbage, unlike QString
    int toInt(bool* ok=0) const;

    khtml::Length* toLengthArray(int& len) const;
    bool isLower() const;
    DOMStringImpl *lower() const;
    DOMStringImpl *upper() const;
    DOMStringImpl *capitalize() const;

    // This modifies the string in place if there is only one ref, makes a new string otherwise.
    DOMStringImpl *replace(QChar, QChar);

    unsigned int l;
    QChar *s;
};

};
#endif
