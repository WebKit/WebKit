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
#include <kdom/core/Attr.h>

#include <kcanvas/KCanvasCreator.h>
#include <kcanvas/KCanvasContainer.h>
#include <kcanvas/device/KRenderingDevice.h>

#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGTests.h"
#include "SVGSwitchElement.h"
#include "SVGAnimatedLength.h"

using namespace WebCore;

SVGSwitchElement::SVGSwitchElement(const QualifiedName& tagName, Document *doc)
: SVGStyledTransformableElement(tagName, doc), SVGTests(), SVGLangSpace(), SVGExternalResourcesRequired()
{
}

SVGSwitchElement::~SVGSwitchElement()
{
}

bool SVGSwitchElement::childShouldCreateRenderer(Node *child) const
{
    for (Node *n = firstChild(); n != 0; n = n->nextSibling()) {
        SVGElement *element = svg_dynamic_cast(n);
        if (element && element->isValid())
            return (n == child); // Only allow this child if it's the first valid child
    }

    return false;
}

RenderObject *SVGSwitchElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return renderingDevice()->createContainer(arena, style, this);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

