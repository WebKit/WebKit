/*
    Copyright (C) 2004 Nikolas Zimmermann <wildfox@kde.org>
                  2004 Rob Buis <buis@kde.org>

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

#include "SVGMatrixImpl.h"
#include "SVGTransformImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGTransformListImpl.h"

using namespace KSVG;

SVGTransformListImpl::SVGTransformListImpl(const SVGStyledElementImpl *context)
: SVGList<SVGTransformImpl>(context)
{
}

SVGTransformListImpl::~SVGTransformListImpl()
{
}

SVGTransformImpl *SVGTransformListImpl::createSVGTransformFromMatrix(SVGMatrixImpl *matrix) const
{
	return SVGSVGElementImpl::createSVGTransformFromMatrix(matrix);
}

SVGTransformImpl *SVGTransformListImpl::consolidate()
{
	SVGTransformImpl *obj = concatenate();
	if(!obj)
		return 0;

	// Disable notifications here...
	const SVGStyledElementImpl *savedContext = m_context;

	m_context = 0;
	SVGTransformImpl *ret = initialize(obj);
	m_context = savedContext;
	
	return ret;
}

SVGTransformImpl *SVGTransformListImpl::concatenate() const
{
	unsigned int length = numberOfItems();
	if(!length)
		return 0;
		
	SVGTransformImpl *obj = SVGSVGElementImpl::createSVGTransform();
	SVGMatrixImpl *matrix = SVGSVGElementImpl::createSVGMatrix();

	for(unsigned int i = 0; i < length; i++)
		matrix->multiply(getItem(i)->matrix());

	obj->setMatrix(matrix);
	return obj;
}

// vim:ts=4:noet
