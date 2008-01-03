/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "CSSHelper.h"

#include "PlatformString.h"
#include <wtf/Vector.h>

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
    while (l > 0 && (*i)[o + l - 1] <= ' ')
        --l;

    if (l >= 5
            && ((*i)[o] == 'u' || (*i)[o] == 'U')
            && ((*i)[o + 1] == 'r' || (*i)[o + 1] == 'R')
            && ((*i)[o + 2] == 'l' || (*i)[o + 2] == 'L')
            && (*i)[o + 3] == '('
            && (*i)[o + l - 1] == ')') {
        o += 4;
        l -= 5;
    }

    while (o < l && (*i)[o] <= ' ') {
        ++o;
        --l;
    }
    while (l > 0 && (*i)[o + l - 1] <= ' ')
        --l;

    if (l >= 2 && (*i)[o] == (*i)[o + l - 1] && ((*i)[o] == '\'' || (*i)[o] == '\"')) {
        o++;
        l -= 2;
    }

    while (o < l && (*i)[o] <= ' ') {
        ++o;
        --l;
    }
    while (l > 0 && (*i)[o + l - 1] <= ' ')
        --l;

    Vector<UChar, 2048> buffer(l);

    int nl = 0;
    for (int k = o; k < o + l; k++) {
        UChar c = (*i)[k];
        if (c > '\r')
            buffer[nl++] = c;
    }

    return String(buffer.data(), nl);
}

} // namespace WebCore
