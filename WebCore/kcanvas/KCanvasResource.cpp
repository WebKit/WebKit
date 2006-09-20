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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#ifdef SVG_SUPPORT
#include "KCanvasResource.h"

#include "Document.h"
#include "KRenderingPaintServer.h"
#include "SVGStyledElement.h"

namespace WebCore {

TextStream &operator<<(TextStream& ts, const KCanvasResource& r) 
{ 
    return r.externalRepresentation(ts); 
}

KCanvasResource::KCanvasResource()
{
}

KCanvasResource::~KCanvasResource()
{
}

void KCanvasResource::addClient(const RenderPath *item)
{
    if(m_clients.find(item) != m_clients.end())
        return;

    m_clients.append(item);
}

const RenderPathList &KCanvasResource::clients() const
{
    return m_clients;
}

void KCanvasResource::invalidate()
{
    RenderPathList::ConstIterator it = m_clients.begin();
    RenderPathList::ConstIterator end = m_clients.end();

    for(; it != end; ++it)
        const_cast<RenderPath*>(*it)->repaint();
}

String KCanvasResource::idInRegistry() const
{
    return m_registryId;
}

void KCanvasResource::setIdInRegistry(const String& newId)
{
    m_registryId = newId;
} 

TextStream& KCanvasResource::externalRepresentation(TextStream& ts) const
{
    return ts;
}

KCanvasResource* getResourceById(Document* document, const AtomicString& id)
{
    if (id.isEmpty())
        return 0;
    Element *element = document->getElementById(id);
    SVGElement *svgElement = svg_dynamic_cast(element);
    if (svgElement && svgElement->isStyled())
        return static_cast<SVGStyledElement*>(svgElement)->canvasResource();
    return 0;
}

KRenderingPaintServer* getPaintServerById(Document* document, const AtomicString& id)
{
    KCanvasResource* resource = getResourceById(document, id);
    if (resource && resource->isPaintServer())
        return static_cast<KRenderingPaintServer*>(resource);
    return 0;
}

}

// vim:ts=4:noet
#endif // SVG_SUPPORT

