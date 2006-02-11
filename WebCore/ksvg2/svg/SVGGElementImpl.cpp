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
#include "SVGGElementImpl.h"

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

using namespace WebCore;

SVGGElementImpl::SVGGElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : SVGStyledTransformableElementImpl(tagName, doc), SVGTestsImpl(), SVGLangSpaceImpl(), SVGExternalResourcesRequiredImpl()
{
}

SVGGElementImpl::~SVGGElementImpl()
{
}

void SVGGElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
{
    if(SVGTestsImpl::parseMappedAttribute(attr)) return;
    if(SVGLangSpaceImpl::parseMappedAttribute(attr)) return;
    if(SVGExternalResourcesRequiredImpl::parseMappedAttribute(attr)) return;
    SVGStyledTransformableElementImpl::parseMappedAttribute(attr);
}

RenderObject *SVGGElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return QPainter::renderingDevice()->createContainer(arena, style, this);
}

// Helper class for <use> support
SVGDummyElementImpl::SVGDummyElementImpl(const QualifiedName& tagName, DocumentImpl *doc) : SVGGElementImpl(tagName, doc),  m_localName("dummy")
{
}

SVGDummyElementImpl::~SVGDummyElementImpl()
{
}

const AtomicString& SVGDummyElementImpl::localName() const
{
    return m_localName;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

