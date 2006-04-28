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
#include "SVGGElement.h"

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace WebCore;

SVGGElement::SVGGElement(const QualifiedName& tagName, Document *doc) : SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGGElement::~SVGGElement()
{
}

void SVGGElement::parseMappedAttribute(MappedAttribute *attr)
{
    if(SVGTests::parseMappedAttribute(attr)) return;
    if(SVGLangSpace::parseMappedAttribute(attr)) return;
    if(SVGExternalResourcesRequired::parseMappedAttribute(attr)) return;
    SVGStyledTransformableElement::parseMappedAttribute(attr);
}

RenderObject* SVGGElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) KCanvasContainer(this);
}

// Helper class for <use> support
SVGDummyElement::SVGDummyElement(const QualifiedName& tagName, Document *doc) : SVGGElement(tagName, doc),  m_localName("dummy")
{
}

SVGDummyElement::~SVGDummyElement()
{
}

const AtomicString& SVGDummyElement::localName() const
{
    return m_localName;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

