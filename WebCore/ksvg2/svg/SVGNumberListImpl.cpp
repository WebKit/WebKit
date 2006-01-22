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
#include <qstringlist.h>

#include "SVGMatrixImpl.h"
#include "SVGNumberImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGNumberListImpl.h"

using namespace KSVG;

SVGNumberListImpl::SVGNumberListImpl(const SVGStyledElementImpl *context)
: SVGList<SVGNumberImpl>(context)
{
}

SVGNumberListImpl::~SVGNumberListImpl()
{
}

void SVGNumberListImpl::parse(const QString &value, const SVGStyledElementImpl *context)
{
    QStringList numbers = QStringList::split(' ', value);
    for(unsigned int i = 0;i < numbers.count();i++)
    {
        SVGNumberImpl *number = new SVGNumberImpl(context);
        number->setValue(numbers[i].toDouble());
        appendItem(number);
    }
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

