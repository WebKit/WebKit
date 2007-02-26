/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

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

#if ENABLE(SVG) && ENABLE(SVG_EXPERIMENTAL_FEATURES)
#include "SVGElementInstanceList.h"

namespace WebCore {

SVGElementInstanceList::SVGElementInstanceList(PassRefPtr<SVGElementInstance> rootInstance)
    : m_rootInstance(rootInstance)
{
}

SVGElementInstanceList::~SVGElementInstanceList()
{
}

unsigned int SVGElementInstanceList::length() const
{
    // NOTE: We could use the same caching facilities, "ChildNodeList" uses.
    unsigned length = 0;
    SVGElementInstance* instance;
    for (instance = m_rootInstance->firstChild(); instance; instance = instance->nextSibling())
        length++;

    return length;
}

RefPtr<SVGElementInstance> SVGElementInstanceList::item(unsigned int index)
{
    unsigned int pos = 0;
    SVGElementInstance* instance = m_rootInstance->firstChild();

    while (instance && pos < index) {
        instance = instance->nextSibling();
        pos++;
    }

    return instance;
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet
