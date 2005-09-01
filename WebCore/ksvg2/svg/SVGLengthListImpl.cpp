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

#include <qstringlist.h>

#include "SVGMatrixImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGLengthListImpl.h"

using namespace KSVG;

SVGLengthListImpl::SVGLengthListImpl(const SVGStyledElementImpl *context)
: SVGList<SVGLengthImpl>(context)
{
}

SVGLengthListImpl::~SVGLengthListImpl()
{
}

void SVGLengthListImpl::parse(const QString &value, const SVGStyledElementImpl *context, LengthMode mode)
{
    QStringList lengths = QStringList::split(' ', value);
    for(unsigned int i = 0;i < lengths.count();i++)
    {
        SVGLengthImpl *length = new SVGLengthImpl(context, mode);
        length->setValueAsString(KDOM::DOMString(lengths[i]).handle());
        appendItem(length);
    }
}

// vim:ts=4:noet
