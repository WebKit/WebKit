/*
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"

#if ENABLE(SVG)
#include "SVGShadowTreeElements.h"

#include "Document.h"
#include "FloatSize.h"
#include "RenderObject.h"
#include "SVGNames.h"

namespace WebCore {

// SVGShadowTreeContainerElement
SVGShadowTreeContainerElement::SVGShadowTreeContainerElement(Document* document)
    : SVGGElement(SVGNames::gTag, document)
{
}
    
SVGShadowTreeContainerElement::~SVGShadowTreeContainerElement()
{
}

FloatSize SVGShadowTreeContainerElement::containerTranslation() const
{
    return FloatSize(m_xOffset.value(this), m_yOffset.value(this));
}

// SVGShadowTreeRootElement
SVGShadowTreeRootElement::SVGShadowTreeRootElement(Document* document, Node* shadowParent)
    : SVGShadowTreeContainerElement(document)
    , m_shadowParent(shadowParent)
{
    setInDocument(true);
}

SVGShadowTreeRootElement::~SVGShadowTreeRootElement()
{
}

void SVGShadowTreeRootElement::attachElement(PassRefPtr<RenderStyle> style, RenderArena* arena)
{
    ASSERT(m_shadowParent);

    // Create the renderer with the specified style
    RenderObject* renderer = createRenderer(arena, style.get());
    if (renderer) {
        setRenderer(renderer);
        renderer->setStyle(style);
    }

    // Set these explicitly since this normally happens during an attach()
    setAttached();

    // Add the renderer to the render tree
    if (renderer)
        m_shadowParent->renderer()->addChild(renderer);
}

}

#endif
