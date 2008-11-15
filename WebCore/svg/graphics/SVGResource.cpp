/*
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG)
#include "SVGResource.h"

#include "RenderPath.h"
#include "SVGElement.h"
#include "SVGStyledElement.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

SVGResource::SVGResource()
{
}

struct ResourceSet { 
    ResourceSet() 
    {
        for (int i = 0; i < _ResourceTypeCount; i++)
            resources[i] = 0;
    }
    SVGResource* resources[_ResourceTypeCount]; 
};

typedef HashMap<SVGStyledElement*, ResourceSet*> ResourceClientMap;

static ResourceClientMap& clientMap() {
    DEFINE_STATIC_LOCAL(ResourceClientMap, map, ());
    return map;
}

SVGResource::~SVGResource()
{
    int type = -1;
    HashSet<SVGStyledElement*>::iterator itr = m_clients.begin();
    
    for (; type < 0 && itr != m_clients.end(); ++itr) {
        ResourceSet* target = clientMap().get(*itr);
        if (!target)
            continue;

        for (int i = 0; i < _ResourceTypeCount; i++) {
            if (target->resources[i] != this) 
                continue;
            type = i;
            target->resources[i] = 0;
            break;
        }
    }
    
    if (type < 0)
        return;
    
    for (; itr != m_clients.end(); ++itr) {
        ResourceSet* target = clientMap().get(*itr);
        if (!target)
            continue;
        
        if (target->resources[type] == this) 
            target->resources[type] = 0;
    }
}

void SVGResource::invalidate()
{
    HashSet<SVGStyledElement*>::const_iterator it = m_clients.begin();
    const HashSet<SVGStyledElement*>::const_iterator end = m_clients.end();

    for (; it != end; ++it) {
        SVGStyledElement* cur = *it;

        if (cur->renderer())
            cur->renderer()->setNeedsLayout(true);

        cur->invalidateResourcesInAncestorChain();
    }
}

void SVGResource::invalidateClients(HashSet<SVGStyledElement*> clients)
{
    HashSet<SVGStyledElement*>::const_iterator it = clients.begin();
    const HashSet<SVGStyledElement*>::const_iterator end = clients.end();

    for (; it != end; ++it) {
        SVGStyledElement* cur = *it;

        if (cur->renderer())
            cur->renderer()->setNeedsLayout(true);

        cur->invalidateResourcesInAncestorChain();
    }
}

void SVGResource::removeClient(SVGStyledElement* item) 
{
    ResourceClientMap::iterator resourcePtr = clientMap().find(item);
    if (resourcePtr == clientMap().end())
        return;
    
    ResourceSet* set = resourcePtr->second;
    ASSERT(set);
    
    clientMap().remove(resourcePtr);
    
    for (int i = 0; i < _ResourceTypeCount; i++)
        if (set->resources[i])
            set->resources[i]->m_clients.remove(item);
    
    delete set;
}

void SVGResource::addClient(SVGStyledElement* item)
{
    if (m_clients.contains(item))
        return;

    m_clients.add(item);

    ResourceSet* target = clientMap().get(item);
    if (!target) 
        target = new ResourceSet;

    SVGResourceType type = resourceType();
    if (SVGResource* oldResource = target->resources[type])
        oldResource->m_clients.remove(item);

    target->resources[type] = this;
    clientMap().set(item, target);
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
    SVGElement* svgElement = 0;
    if (element && element->isSVGElement())
        svgElement = static_cast<SVGElement*>(element);

    if (svgElement && svgElement->isStyled())
        return static_cast<SVGStyledElement*>(svgElement)->canvasResource();

    return 0;
}

TextStream& operator<<(TextStream& ts, const SVGResource& r)
{
    return r.externalRepresentation(ts);
}

}

#endif
