/*
 * This file is part of the CSS implementation for KDE.
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
#include "config.h"
#include "csshelper.h"

#include <qfontmetrics.h>
#include <qpaintdevice.h>
#include <qpaintdevicemetrics.h>

#include <kcharsets.h>
#include <kdebug.h>

#include "rendering/render_style.h"
#include "css_valueimpl.h"
#include "dom/css_value.h"
#include "misc/helper.h"
#include "xml/dom_stringimpl.h"
#include "khtml_settings.h"

using namespace DOM;
using namespace khtml;


DOMString khtml::parseURL(const DOMString &url)
{
    DOMStringImpl* i = url.impl();
    if(!i) return DOMString();

    int o = 0;
    int l = i->l;
    while(o < l && (i->s[o] <= ' ')) { o++; l--; }
    while(l > 0 && (i->s[o+l-1] <= ' ')) l--;

    if(l >= 5 &&
       (i->s[o].lower() == 'u') &&
       (i->s[o+1].lower() == 'r') &&
       (i->s[o+2].lower() == 'l') &&
       i->s[o+3].latin1() == '(' &&
       i->s[o+l-1].latin1() == ')') {
        o += 4;
        l -= 5;
    }

    while(o < l && (i->s[o] <= ' ')) { o++; l--; }
    while(l > 0 && (i->s[o+l-1] <= ' ')) l--;

    if(l >= 2 && i->s[o] == i->s[o+l-1] &&
       (i->s[o].latin1() == '\'' || i->s[o].latin1() == '\"')) {
        o++;
        l -= 2;
    }

    while(o < l && (i->s[o] <= ' ')) { o++; l--; }
    while(l > 0 && (i->s[o+l-1] <= ' ')) l--;

    DOMStringImpl* j = new DOMStringImpl(i->s+o,l);

    int nl = 0;
    for(int k = o; k < o+l; k++)
        if(i->s[k].unicode() > '\r')
            j->s[nl++] = i->s[k];

    j->l = nl;

    return j;
}
