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

#include <kdebug.h>

#include "SVGMatrixImpl.h"
#include "SVGTransformImpl.h"
#include "SVGSVGElementImpl.h"
#include "ksvg.h"

using namespace KSVG;

SVGTransformImpl::SVGTransformImpl() : KDOM::Shared()
{
    m_matrix = SVGSVGElementImpl::createSVGMatrix();
    m_matrix->ref();

    m_type = SVG_TRANSFORM_UNKNOWN;
    m_angle = 0;
}

SVGTransformImpl::~SVGTransformImpl()
{
    if(m_matrix)
        m_matrix->deref();
}

unsigned short SVGTransformImpl::type() const
{
    return m_type;
}

SVGMatrixImpl *SVGTransformImpl::matrix() const
{
    return m_matrix;
}

double SVGTransformImpl::angle() const
{
    return m_angle;
}

void SVGTransformImpl::setMatrix(SVGMatrixImpl *matrix)
{
    m_type = SVG_TRANSFORM_MATRIX;
    m_angle = 0;
    
    KDOM_SAFE_SET(m_matrix, matrix);
}

void SVGTransformImpl::setTranslate(double tx, double ty)
{
    m_type = SVG_TRANSFORM_TRANSLATE;
    m_angle = 0;
    
    m_matrix->reset();
    m_matrix->translate(tx, ty);
}

void SVGTransformImpl::setScale(double sx, double sy)
{
    m_type = SVG_TRANSFORM_SCALE;
    m_angle = 0;
    
    m_matrix->reset();
    m_matrix->scaleNonUniform(sx, sy);
}

void SVGTransformImpl::setRotate(double angle, double cx, double cy)
{
    m_type = SVG_TRANSFORM_ROTATE;
    m_angle = angle;
    
    // TODO: toString() implementation, which can show cx, cy (need to be stored?)
    m_matrix->reset();
    m_matrix->translate(cx, cy);
    m_matrix->rotate(angle);
    m_matrix->translate(-cx, -cy);
}

void SVGTransformImpl::setSkewX(double angle)
{
    m_type = SVG_TRANSFORM_SKEWX;
    m_angle = angle;
    
    m_matrix->reset();
    m_matrix->skewX(angle);
}

void SVGTransformImpl::setSkewY(double angle)
{
    m_type = SVG_TRANSFORM_SKEWY;
    m_angle = angle;
    
    m_matrix->reset();
    m_matrix->skewY(angle);
}

// vim:ts=4:noet
