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
#include "SVGStyledLocatableElement.h"

#include "SVGElement.h"
#include "SVGMatrix.h"
#include "SVGRect.h"
#include "SVGSVGElement.h"
#include <kcanvas/RenderPath.h>

using namespace WebCore;

SVGStyledLocatableElement::SVGStyledLocatableElement(const QualifiedName& tagName, Document *doc)
: SVGStyledElement(tagName, doc), SVGLocatable()
{
}

SVGStyledLocatableElement::~SVGStyledLocatableElement()
{
}

SVGElement *SVGStyledLocatableElement::nearestViewportElement() const
{
    return SVGLocatable::nearestViewportElement(this);
}

SVGElement *SVGStyledLocatableElement::farthestViewportElement() const
{
    return SVGLocatable::farthestViewportElement(this);
}

SVGRect *SVGStyledLocatableElement::getBBox() const
{
    return SVGLocatable::getBBox(this);
}

SVGMatrix *SVGStyledLocatableElement::getCTM() const
{
    return SVGLocatable::getCTM(this);
}

SVGMatrix *SVGStyledLocatableElement::getScreenCTM() const
{
    return SVGLocatable::getScreenCTM(this);
}

SVGMatrix *SVGStyledLocatableElement::getTransformToElement(SVGElement *) const
{
    // TODO!
    return 0;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

