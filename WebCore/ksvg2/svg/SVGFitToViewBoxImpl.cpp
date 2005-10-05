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
#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>
#include <kdom/core/DOMStringImpl.h>

#include "svgattrs.h"
#include "SVGRectImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGAnimatedRectImpl.h"
#include "SVGFitToViewBoxImpl.h"
#include "SVGPreserveAspectRatioImpl.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

using namespace KSVG;

SVGFitToViewBoxImpl::SVGFitToViewBoxImpl()
{
    m_viewBox = 0;
    m_preserveAspectRatio = 0;
}

SVGFitToViewBoxImpl::~SVGFitToViewBoxImpl()
{
    if(m_viewBox)
        m_viewBox->deref();
    if(m_preserveAspectRatio)
        m_preserveAspectRatio->deref();
}

SVGAnimatedRectImpl *SVGFitToViewBoxImpl::viewBox() const
{
    if(!m_viewBox)
    {
        const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
        m_viewBox = new SVGAnimatedRectImpl(context);
        m_viewBox->ref();
    }

    return m_viewBox;
}

SVGAnimatedPreserveAspectRatioImpl *SVGFitToViewBoxImpl::preserveAspectRatio() const
{
    if(!m_preserveAspectRatio)
    {
        const SVGStyledElementImpl *context = dynamic_cast<const SVGStyledElementImpl *>(this);
        m_preserveAspectRatio = new SVGAnimatedPreserveAspectRatioImpl(context);
        m_preserveAspectRatio->ref();
    }

    return m_preserveAspectRatio;
}

void SVGFitToViewBoxImpl::parseViewBox(KDOM::DOMStringImpl *str)
{
    // allow for viewbox def with ',' or whitespace
    QString viewbox(str->unicode(), str->length());
    QStringList points = QStringList::split(' ', viewbox.replace(',', ' ').simplifyWhiteSpace());

    if (points.count() == 4) {
        viewBox()->baseVal()->setX(points[0].toDouble());
        viewBox()->baseVal()->setY(points[1].toDouble());
        viewBox()->baseVal()->setWidth(points[2].toDouble());
        viewBox()->baseVal()->setHeight(points[3].toDouble());
    } else {
        fprintf(stderr, "WARNING: Malformed viewbox string: %s (l: %i)", viewbox.ascii(), viewbox.length());
    }
}

SVGMatrixImpl *SVGFitToViewBoxImpl::viewBoxToViewTransform(float viewWidth, float viewHeight) const
{
    SVGRectImpl *viewBoxRect = viewBox()->baseVal();
    if(viewBoxRect->width() == 0 || viewBoxRect->height() == 0)
        return SVGSVGElementImpl::createSVGMatrix();

    return preserveAspectRatio()->baseVal()->getCTM(viewBoxRect->x(),
            viewBoxRect->y(), viewBoxRect->width(), viewBoxRect->height(),
            0, 0, viewWidth, viewHeight);
}

bool SVGFitToViewBoxImpl::parseAttribute(KDOM::AttributeImpl *attr)
{
    int id = (attr->id() & NodeImpl_IdLocalMask);
    switch(id)
    {
        case ATTR_VIEWBOX:
        {
            parseViewBox(attr->value());
            return true;
        }
        case ATTR_PRESERVEASPECTRATIO:
        {
            preserveAspectRatio()->baseVal()->parsePreserveAspectRatio(attr->value());
            return true;
        }
    }

    return false;
}

// vim:ts=4:noet
