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

#include "misc/khtmllayout.h"
#include "misc/shared.h"
#include "misc/main_thread_malloc.h"

#define QT_ALLOC_QCHAR_VEC( N ) static_cast<QChar*>(main_thread_malloc( sizeof(QChar)*( N ) ))
#define QT_DELETE_QCHAR_VEC( P ) main_thread_free ((void*)( P ))

namespace DOM {

class DOMStringImpl : public khtml::Shared<DOMStringImpl>
{
private:
    struct WithOneRef { };
    DOMStringImpl(WithOneRef) { s = 0; l = 0; _hash = 0; _inTable = false; ref(); }

protected:
    DOMStringImpl() { s = 0, l = 0; _hash = 0; _inTable = false; }
public:
    DOMStringImpl(const QChar *str, unsigned int len);
    DOMStringImpl(const char *str);
    DOMStringImpl(const char *str, unsigned int len);
    DOMStringImpl(const QChar &ch);
    ~DOMStringImpl();
    
    MAIN_THREAD_ALLOCATED;

    unsigned hash() const { if (_hash == 0) _hash = computeHash(s, l); return _hash; }
    static unsigned computeHash(const QChar *, int length);
    static unsigned computeHash(const char *);
    
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
    bool containsOnlyWhitespace(unsigned int from, unsigned int len) const;
    
    // ignores trailing garbage, unlike QString
    int toInt(bool* ok=0) const;

    khtml::Length* toCoordsArray(int& len) const;
    khtml::Length* toLengthArray(int& len) const;
    bool isLower() const;
    DOMStringImpl *lower() const;
    DOMStringImpl *upper() const;
    DOMStringImpl *capitalize() const;

    // This modifies the string in place if there is only one ref, makes a new string otherwise.
    DOMStringImpl *replace(QChar, QChar);

    static DOMStringImpl* empty();

    // For debugging only, leaks memory.
    const char *ascii() const;

    unsigned int l;
    QChar *s;
    mutable unsigned _hash;
    bool _inTable;
};

};
#endif
