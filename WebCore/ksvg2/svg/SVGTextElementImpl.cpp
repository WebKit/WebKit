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
#include <kdom/kdom.h>
#include "SVGTextElementImpl.h"
#include "SVGTSpanElementImpl.h"
#include "SVGAnimatedLengthListImpl.h"
#include "SVGRenderStyle.h"
#include "KCanvasRenderingStyle.h"
#include <kdom/css/RenderStyle.h>

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/RenderSVGText.h>

#include <qvector.h>

using namespace KSVG;

SVGTextElementImpl::SVGTextElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc)
: SVGTextPositioningElementImpl(tagName, doc), SVGTransformableImpl()
{
}

SVGTextElementImpl::~SVGTextElementImpl()
{
}

void SVGTextElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    //if(SVGTransformableImpl::parseMappedAttribute(attr)) return;
    SVGTextPositioningElementImpl::parseMappedAttribute(attr);
}

SVGElementImpl *SVGTextElementImpl::nearestViewportElement() const
{
    return SVGTransformableImpl::nearestViewportElement(this);
}

SVGElementImpl *SVGTextElementImpl::farthestViewportElement() const
{
    return SVGTransformableImpl::farthestViewportElement(this);
}

SVGRectImpl *SVGTextElementImpl::getBBox() const
{
    return SVGTransformableImpl::getBBox(this);
}

SVGMatrixImpl *SVGTextElementImpl::getScreenCTM() const
{
    return SVGLocatableImpl::getScreenCTM(this);
}

SVGMatrixImpl *SVGTextElementImpl::getCTM() const
{
    return SVGLocatableImpl::getScreenCTM(this);
}

khtml::RenderObject *SVGTextElementImpl::createRenderer(RenderArena *arena, khtml::RenderStyle *style)
{
    return new (arena) RenderSVGText(this);
}

bool SVGTextElementImpl::childShouldCreateRenderer(DOM::NodeImpl *child) const
{
    if (child->isTextNode())
        return true;
    return false;
}

// vim:ts=4:noet
