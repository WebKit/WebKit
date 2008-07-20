/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGPointList.h"
#include "PlatformString.h"

namespace WebCore {

SVGPointList::SVGPointList(const QualifiedName& attributeName)
    : SVGPODList<FloatPoint>(attributeName)
{
}

SVGPointList::~SVGPointList()
{
}

String SVGPointList::valueAsString() const
{
    String result;

    ExceptionCode ec = 0;
    for (unsigned int i = 0; i < numberOfItems(); ++i) {
        if (i > 0)
            result += " ";

        FloatPoint point = getItem(i, ec);
        ASSERT(ec == 0);

        result += String::format("%.6lg %.6lg", point.x(), point.y());
    }

    return result;
}

}

#endif // ENABLE(SVG)
