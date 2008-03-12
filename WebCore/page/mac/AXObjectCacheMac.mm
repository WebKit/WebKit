/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "AXObjectCache.h"

#import "Document.h"
#import "FoundationExtras.h"
#import "RenderObject.h"
#import "WebCoreAXObject.h"
#import "WebCoreViewFactory.h"

// The simple Cocoa calls in this file don't throw exceptions.

namespace WebCore {

struct TextMarkerData  {
    AXID axID;
    Node* node;
    int offset;
    EAffinity affinity;
};

bool AXObjectCache::gAccessibilityEnabled = false;

AXObjectCache::~AXObjectCache()
{
    HashMap<RenderObject*, WebCoreAXObject*>::iterator end = m_objects.end();
    for (HashMap<RenderObject*, WebCoreAXObject*>::iterator it = m_objects.begin(); it != end; ++it) {
        WebCoreAXObject* obj = (*it).second;
        [obj detach];
        HardRelease(obj);
    }
}

WebCoreAXObject* AXObjectCache::get(RenderObject* renderer)
{
    WebCoreAXObject* obj = m_objects.get(renderer);
    if (obj)
        return obj;

    obj = [[WebCoreAXObject alloc] initWithRenderer:renderer];
    HardRetainWithNSRelease(obj);
    m_objects.set(renderer, obj);
    return obj;
}

void AXObjectCache::remove(RenderObject* renderer)
{
    WebCoreAXObject* obj = m_objects.take(renderer);
    if (!obj) {
        ASSERT(!renderer->hasAXObject());
        return;
    }

    [obj detach];
    HardRelease(obj);

    ASSERT(m_objects.size() >= m_idsInUse.size());
}

AXID AXObjectCache::getAXID(WebCoreAXObject* obj)
{
    // check for already-assigned ID
    AXID objID = [obj axObjectID];
    if (objID) {
        ASSERT(m_idsInUse.contains(objID));
        return objID;
    }

    // generate a new ID
    static AXID lastUsedID = 0;
    objID = lastUsedID;
    do
        ++objID;
    while (objID == 0 || objID == AXIDHashTraits::deletedValue() || m_idsInUse.contains(objID));
    m_idsInUse.add(objID);
    lastUsedID = objID;
    [obj setAXObjectID:objID];

    return objID;
}

void AXObjectCache::removeAXID(WebCoreAXObject* obj)
{
    AXID objID = [obj axObjectID];
    if (objID == 0)
        return;
    ASSERT(objID != AXIDHashTraits::deletedValue());
    ASSERT(m_idsInUse.contains(objID));
    [obj setAXObjectID:0];
    m_idsInUse.remove(objID);
}

WebCoreTextMarker* AXObjectCache::textMarkerForVisiblePosition(const VisiblePosition& visiblePos)
{
    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.node();
    ASSERT(domNode);
    if (!domNode)
        return nil;
    
    // locate the renderer, which must exist for a visible dom node
    RenderObject* renderer = domNode->renderer();
    ASSERT(renderer);
    
    // find or create an accessibility object for this renderer
    WebCoreAXObject* obj = get(renderer);
    
    // create a text marker, adding an ID for the WebCoreAXObject if needed
    TextMarkerData textMarkerData;
    textMarkerData.axID = getAXID(obj);
    textMarkerData.node = domNode;
    textMarkerData.offset = deepPos.offset();
    textMarkerData.affinity = visiblePos.affinity();
    return [[WebCoreViewFactory sharedFactory] textMarkerWithBytes:&textMarkerData length:sizeof(textMarkerData)]; 
}

VisiblePosition AXObjectCache::visiblePositionForTextMarker(WebCoreTextMarker* textMarker)
{
    TextMarkerData textMarkerData;
    
    if (![[WebCoreViewFactory sharedFactory] getBytes:&textMarkerData fromTextMarker:textMarker length:sizeof(textMarkerData)])
        return VisiblePosition();

    // return empty position if the text marker is no longer valid
    if (!m_idsInUse.contains(textMarkerData.axID))
        return VisiblePosition();

    // generate a VisiblePosition from the data we stored earlier
    VisiblePosition visiblePos = VisiblePosition(textMarkerData.node, textMarkerData.offset, textMarkerData.affinity);

    // make sure the node and offset still match (catches stale markers). affinity is not critical for this.
    Position deepPos = visiblePos.deepEquivalent();
    if (deepPos.node() != textMarkerData.node || deepPos.offset() != textMarkerData.offset)
        return VisiblePosition();
    
    return visiblePos;
}

void AXObjectCache::childrenChanged(RenderObject* renderer)
{
    WebCoreAXObject* obj = m_objects.get(renderer);
    if (obj)
        [obj childrenChanged];
}

void AXObjectCache::postNotification(RenderObject* renderer, const String& message)
{
    if (!renderer)
        return;
    
    // notifications for text input objects are sent to that object
    // all others are sent to the top WebArea
    WebCoreAXObject* obj = [get(renderer) observableObject];
    if (obj)
        NSAccessibilityPostNotification(obj, message);
    else
        NSAccessibilityPostNotification(get(renderer->document()->renderer()), message);
}

void AXObjectCache::postNotificationToElement(RenderObject* renderer, const String& message)
{
    // send the notification to the specified element itself, not one of its ancestors
    if (renderer)
        NSAccessibilityPostNotification(get(renderer), message);
}

void AXObjectCache::handleFocusedUIElementChanged()
{
    [[WebCoreViewFactory sharedFactory] accessibilityHandleFocusChanged];
}

}
