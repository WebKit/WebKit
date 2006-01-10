/*
 * Copyright (C) 2003, 2004, 2005 Apple Computer, Inc.  All rights reserved.
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
#import "KWQAccObjectCache.h"

#import "KWQAccObject.h"
#import <kxmlcore/Assertions.h>
#import "KWQFoundationExtras.h"
#import "KWQString.h"
#import "render_object.h"
#import "WebCoreViewFactory.h"

using khtml::EAffinity;
using khtml::RenderObject;
using khtml::VisiblePosition;

// The simple Cocoa calls in this file don't throw exceptions.

bool KWQAccObjectCache::gAccessibilityEnabled = false;

typedef struct KWQTextMarkerData  {
    KWQAccObjectID  accObjectID;
    DOM::NodeImpl*  nodeImpl;
    int             offset;
    EAffinity       affinity;
};

KWQAccObjectCache::KWQAccObjectCache() : m_accCache(0), m_accCacheByID(0), m_accObjectIDSource(0)
{
}

KWQAccObjectCache::~KWQAccObjectCache()
{
    delete m_accCache;
    if (m_accCacheByID) {
        // m_accCacheByID should have been emptied by releasing m_accCache
        ASSERT(m_accCacheByID->isEmpty());
        delete m_accCacheByID;
    }
}

KWQAccObject* KWQAccObjectCache::accObject(RenderObject* renderer)
{
    if (!m_accCache)
        m_accCache = new RenderObjectToAccObjectMap;
    
    KWQAccObject* obj = m_accCache->get(renderer);
    if (!obj) {
        obj = [[KWQAccObject alloc] initWithRenderer: renderer]; // Initial ref happens here.
        setAccObject(renderer, obj);
    }

    return obj;
}

void KWQAccObjectCache::setAccObject(RenderObject* impl, KWQAccObject* accObject)
{
    if (!m_accCache)
        m_accCache = new RenderObjectToAccObjectMap;
    
    m_accCache->set(impl, accObject);
    ASSERT(!m_accCacheByID || m_accCache->size() >= m_accCacheByID->size());
}

void KWQAccObjectCache::removeAccObject(RenderObject* impl)
{
    if (!m_accCache)
        return;

    KWQAccObject* obj = m_accCache->get(impl);
    if (obj) {
        [obj detach];
        [obj release];
        m_accCache->remove(impl);
    }

    ASSERT(!m_accCacheByID || m_accCache->size() >= m_accCacheByID->size());
}

KWQAccObjectID KWQAccObjectCache::getAccObjectID(KWQAccObject* accObject)
{
    KWQAccObjectID  accObjectID;

    // create the ID table as needed
    if (!m_accCacheByID)
        m_accCacheByID = new AccIdToAccObjectMap;
    
    // check for already-assigned ID
    accObjectID = [accObject accObjectID];
    if (accObjectID != 0) {
        ASSERT(m_accCacheByID->contains(accObjectID));
        return accObjectID;
    }
    
    // generate a new ID
    accObjectID = m_accObjectIDSource + 1;
    while (accObjectID == 0 || m_accCacheByID->contains(accObjectID)) {
        ASSERT(accObjectID != m_accObjectIDSource);   // check for exhaustion
        accObjectID += 1;
    }
    m_accObjectIDSource = accObjectID;

    // assign the new ID to the object
    [accObject setAccObjectID:accObjectID];
    
    // add the object to the ID table
    m_accCacheByID->set(accObjectID, accObject);
    ASSERT(m_accCacheByID->contains(accObjectID));
    
    return accObjectID;
}

void KWQAccObjectCache::removeAccObjectID(KWQAccObject* accObject)
{
    // retrieve and clear the ID from the object, nothing to do if it was never assigned
    KWQAccObjectID  accObjectID = [accObject accObjectID];
    if (accObjectID == 0)
        return;
    [accObject setAccObjectID:0];
    
    // remove the element from the lookup table
    ASSERT(m_accCacheByID);
    ASSERT(m_accCacheByID->contains(accObjectID));
    m_accCacheByID->remove(accObjectID);
}

WebCoreTextMarker *KWQAccObjectCache::textMarkerForVisiblePosition(const VisiblePosition &visiblePos)
{
    DOM::Position deepPos = visiblePos.deepEquivalent();
    DOM::NodeImpl* domNode = deepPos.node();
    ASSERT(domNode != NULL);
    if (domNode == NULL)
        return nil;
    
    // locate the renderer, which must exist for a visible dom node
    khtml::RenderObject* renderer = domNode->renderer();
    ASSERT(renderer != NULL);
    
    // find or create an accessibility object for this renderer
    KWQAccObject* accObject = this->accObject(renderer);
    
    // create a text marker, adding an ID for the KWQAccObject if needed
    KWQTextMarkerData textMarkerData;
    textMarkerData.accObjectID = getAccObjectID(accObject);
    textMarkerData.nodeImpl = domNode;
    textMarkerData.offset = deepPos.offset();
    textMarkerData.affinity = visiblePos.affinity();
    return [[WebCoreViewFactory sharedFactory] textMarkerWithBytes:&textMarkerData length:sizeof(textMarkerData)]; 
}

VisiblePosition KWQAccObjectCache::visiblePositionForTextMarker(WebCoreTextMarker *textMarker)
{
    KWQTextMarkerData textMarkerData;
    
    if (![[WebCoreViewFactory sharedFactory] getBytes:&textMarkerData fromTextMarker:textMarker length:sizeof(textMarkerData)])
        return VisiblePosition();
    
    // return empty position if the text marker is no longer valid
    if (!m_accCacheByID || !m_accCacheByID->contains(textMarkerData.accObjectID))
        return VisiblePosition();

    // return the position from the data we stored earlier
    return VisiblePosition(textMarkerData.nodeImpl, textMarkerData.offset, textMarkerData.affinity);
}

void KWQAccObjectCache::detach(RenderObject* renderer)
{
    removeAccObject(renderer);
}

void KWQAccObjectCache::childrenChanged(RenderObject* renderer)
{
    if (!m_accCache)
        return;
    
    KWQAccObject* obj = m_accCache->get(renderer);
    if (!obj)
        return;
    
    [obj childrenChanged];
}

void KWQAccObjectCache::postNotificationToTopWebArea(RenderObject* renderer, const QString& msg)
{
    if (renderer) {
        RenderObject * obj = renderer->document()->topDocument()->renderer();
        NSAccessibilityPostNotification(accObject(obj), msg.getNSString());
    }
}

void KWQAccObjectCache::postNotification(RenderObject* renderer, const QString& msg)
{
    if (renderer)
        NSAccessibilityPostNotification(accObject(renderer), msg.getNSString());
}

void KWQAccObjectCache::handleFocusedUIElementChanged()
{
    [[WebCoreViewFactory sharedFactory] accessibilityHandleFocusChanged];
}
