/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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
#include "SVGStyledLocatableElement.h"

#include "RenderPath.h"
#include "SVGElement.h"
#include "TransformationMatrix.h"
#include "SVGSVGElement.h"

namespace WebCore {

SVGStyledLocatableElement::SVGStyledLocatableElement(const QualifiedName& tagName, Document* doc)
    : SVGLocatable()
    , SVGStyledElement(tagName, doc)
{
}

SVGStyledLocatableElement::~SVGStyledLocatableElement()
{
}

SVGElement* SVGStyledLocatableElement::nearestViewportElement() const
{
    return SVGLocatable::nearestViewportElement(this);
}

SVGElement* SVGStyledLocatableElement::farthestViewportElement() const
{
    return SVGLocatable::farthestViewportElement(this);
}

FloatRect SVGStyledLocatableElement::getBBox() const
{
    return SVGLocatable::getBBox(this);
}

TransformationMatrix SVGStyledLocatableElement::getCTM() const
{
    return SVGLocatable::getCTM(this);
}

TransformationMatrix SVGStyledLocatableElement::getScreenCTM() const
{
    return SVGLocatable::getScreenCTM(this);
}

}

#endif // ENABLE(SVG)
