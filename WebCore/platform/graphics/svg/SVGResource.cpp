/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SVGResource.h"

#ifdef SVG_SUPPORT

#include "KRenderingPaintServer.h"
#include "RenderPath.h"
#include "SVGElement.h"
#include "SVGStyledElement.h"

namespace WebCore {

SVGResource::SVGResource()
{
}

SVGResource::~SVGResource()
{
}

void SVGResource::invalidate()
{
    unsigned size = m_clients.size();
    for (unsigned i = 0; i < size; i++)
        const_cast<RenderPath*>(m_clients[i])->repaint();
}

void SVGResource::addClient(const RenderPath* item)
{
    unsigned size = m_clients.size();

    for (unsigned i = 0; i < size; i++) {
        if (m_clients[i] == item)
            return;
    }

    m_clients.append(item);
}

const RenderPathList& SVGResource::clients() const
{
    return m_clients;
}

String SVGResource::idInRegistry() const
{
    return m_registryId;
}

void SVGResource::setIdInRegistry(const String& id)
{
    m_registryId = id;
}

TextStream& SVGResource::externalRepresentation(TextStream& ts) const
{
    return ts;
}

SVGResource* getResourceById(Document* document, const AtomicString& id)
{
    if (id.isEmpty())
        return 0;

    Element* element = document->getElementById(id);
    SVGElement* svgElement = svg_dynamic_cast(element);

    if (svgElement && svgElement->isStyled())
        return static_cast<SVGStyledElement*>(svgElement)->canvasResource();

    return 0;
}

KRenderingPaintServer* getPaintServerById(Document* document, const AtomicString& id)
{
    SVGResource* resource = getResourceById(document, id);
    if (resource && resource->isPaintServer())
        return static_cast<KRenderingPaintServer*>(resource);

    return 0;
}


TextStream& operator<<(TextStream& ts, const SVGResource& r)
{
    return r.externalRepresentation(ts);
}

} // namespace WebCore

#endif
