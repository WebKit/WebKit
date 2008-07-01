/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "AffineTransform.h"
#include "SVGTransform.h"
#include "SVGSVGElement.h"
#include "SVGTransformDistance.h"
#include "SVGTransformList.h"

using namespace WebCore;

SVGTransformList::SVGTransformList(const QualifiedName& attributeName)
    : SVGPODList<SVGTransform>(attributeName)
{
}

SVGTransformList::~SVGTransformList()
{
}

SVGTransform SVGTransformList::createSVGTransformFromMatrix(const AffineTransform& matrix) const
{
    return SVGSVGElement::createSVGTransformFromMatrix(matrix);
}

SVGTransform SVGTransformList::consolidate()
{
    ExceptionCode ec = 0;
    return initialize(concatenate(), ec);
}

SVGTransform SVGTransformList::concatenate() const
{
    unsigned int length = numberOfItems();
    if (!length)
        return SVGTransform();
        
    AffineTransform matrix;
    ExceptionCode ec = 0;
    for (unsigned int i = 0; i < length; i++)
        matrix = getItem(i, ec).matrix() * matrix;

    return SVGTransform(matrix);
}

SVGTransform SVGTransformList::concatenateForType(SVGTransform::SVGTransformType type) const
{
    unsigned int length = numberOfItems();
    if (!length)
        return SVGTransform();
    
    ExceptionCode ec = 0;
    SVGTransformDistance totalTransform;
    for (unsigned int i = 0; i < length; i++) {
        const SVGTransform& transform = getItem(i, ec);
        if (transform.type() == type)
            totalTransform.addSVGTransform(transform);
    }
    
    return totalTransform.addToSVGTransform(SVGTransform());
}

#endif // ENABLE(SVG)
