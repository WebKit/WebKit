/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/
//----------------------------------------------------------------------------
//
// KDE HTML Widget -- String class

#ifndef KHTMLSTRING_H
#define KHTMLSTRING_H

#include "dom/dom_string.h"

#include <qstring.h>

#include <assert.h>

namespace khtml
{

class DOMStringIt
{
public:
    DOMStringIt()
	{ s = 0, l = 0; lines = 0; }
    DOMStringIt(QChar *str, uint len)
	{ s = str, l = len; lines = 0; }
    DOMStringIt(const QString &str)
	{ s = str.unicode(); l = str.length(); lines = 0; }
    DOMStringIt(const DOM::DOMString &str)
	{ s = str.unicode(); l = str.length(); lines = 0; }

    DOMStringIt *operator++()
    {
        if (!pushedChar1.isNull()) {
            pushedChar1 = pushedChar2;
            pushedChar2 = 0;
        } else if (l > 0) {
            if (*s == '\n')
                lines++;
	    s++, l--;
        }
	return this;
    }

    void push(const QChar& c) {
        if (pushedChar1.isNull())
            pushedChar1 = c;
        else {
            assert(pushedChar2.isNull());
            pushedChar2 = c;
        }
    }

    const QChar *current() const {
        if (!pushedChar1.isNull())
            return &pushedChar1;
        if (!pushedChar2.isNull())
            return &pushedChar2;
        return s;
    }
    
    const QChar& operator*() const { return *current(); }
    const QChar* operator->() const { return current(); }

    bool escaped() const { return !pushedChar1.isNull(); }
    uint length() const { return l + !pushedChar1.isNull() + !pushedChar2.isNull(); }

    int lineCount() const { return lines; }

protected:
    QChar pushedChar1;
    QChar pushedChar2;
    const QChar *s;
    int l;
    int lines;
};

}

#endif
