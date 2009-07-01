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
#include "TransformationMatrix.h"
#include "SVGTransform.h"
#include "SVGSVGElement.h"
#include "SVGTransformList.h"

using namespace WebCore;

SVGTransformList::SVGTransformList(const QualifiedName& attributeName)
    : SVGPODList<SVGTransform>(attributeName)
{
}

SVGTransformList::~SVGTransformList()
{
}

SVGTransform SVGTransformList::createSVGTransformFromMatrix(const TransformationMatrix& matrix) const
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
        
    TransformationMatrix matrix;
    ExceptionCode ec = 0;
    for (unsigned int i = 0; i < length; i++)
        matrix = getItem(i, ec).matrix() * matrix;

    return SVGTransform(matrix);
}

String SVGTransformList::valueAsString() const
{
    // TODO: We may want to build a real transform string, instead of concatting to a matrix(...).
    SVGTransform transform = concatenate();
    if (transform.type() == SVGTransform::SVG_TRANSFORM_MATRIX) {
        TransformationMatrix matrix = transform.matrix();
        return String::format("matrix(%f %f %f %f %f %f)", matrix.a(), matrix.b(), matrix.c(), matrix.d(), matrix.e(), matrix.f());
    }

    return String();
}

#endif // ENABLE(SVG)
