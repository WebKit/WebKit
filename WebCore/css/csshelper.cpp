/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "config.h"
#include "csshelper.h"

#include "PlatformString.h"
#include <kxmlcore/Vector.h>

namespace WebCore {

String parseURL(const String& url)
{
    StringImpl* i = url.impl();
    if (!i)
        return String();

    int o = 0;
    int l = i->length();

    while (o < l && (*i)[o] <= ' ') {
        ++o;
        --l;
    }
    while (l > 0 && (*i)[o+l-1] <= ' ')
        --l;

    if (l >= 5
            && (*i)[o].lower() == 'u'
            && (*i)[o + 1].lower() == 'r'
            && (*i)[o + 2].lower() == 'l'
            && (*i)[o + 3] == '('
            && (*i)[o + l - 1] == ')') {
        o += 4;
        l -= 5;
    }

    while (o < l && (*i)[o] <= ' ') {
        ++o;
        --l;
    }
    while (l > 0 && (*i)[o+l-1] <= ' ')
        --l;

    if (l >= 2 && (*i)[o] == (*i)[o+l-1] && ((*i)[o] == '\'' || (*i)[o] == '\"')) {
        o++;
        l -= 2;
    }

    while (o < l && (*i)[o] <= ' ') {
        ++o;
        --l;
    }
    while (l > 0 && (*i)[o+l-1] <= ' ')
        --l;

    Vector<unsigned short, 2048> buffer;

    int nl = 0;
    for (int k = o; k < o + l; k++) {
        unsigned short c = (*i)[k].unicode();
        if (c > '\r')
            buffer[nl++] = c;
    }

    return new StringImpl(reinterpret_cast<QChar*>(buffer.data()), nl);
}

}
