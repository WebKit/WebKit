/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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

#include "config.h"
#if SVG_SUPPORT
#include "DeprecatedStringList.h"

#include "SVGStringList.h"

using namespace WebCore;

SVGStringList::SVGStringList(const SVGStyledElement *context)
: SVGList<StringImpl>(context)
{
}

SVGStringList::~SVGStringList()
{
}

void SVGStringList::reset(const DeprecatedString &str)
{
    DeprecatedStringList list = DeprecatedStringList::split(' ', str);
    if (list.count() == 0) {
        StringImpl *item = new StringImpl(str);
        item->ref();
        appendItem(item);
    } else for(DeprecatedStringList::Iterator it = list.begin(); it != list.end(); ++it) {
        StringImpl *item = new StringImpl((*it));
        item->ref();
        appendItem(item);
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

