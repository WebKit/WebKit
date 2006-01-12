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
#import "dom_docimpl.h"

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

KWQAccObjectCache::KWQAccObjectCache()
{
    accCache = NULL;
    accCacheByID = NULL;
    accObjectIDSource = 0;
}

KWQAccObjectCache::~KWQAccObjectCache()
{
    // Destroy the dictionary
    if (accCache)
        CFRelease(accCache);
        
    // Destroy the ID lookup dictionary
    if (accCacheByID) {
        // accCacheByID should have been emptied by releasing accCache
        ASSERT(CFDictionaryGetCount(accCacheByID) == 0);
        CFRelease(accCacheByID);
    }
}

KWQAccObject* KWQAccObjectCache::accObject(RenderObject* renderer)
{
    if (!accCache)
        // No need to retain/free either impl key, or id value.
        accCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    KWQAccObject* obj = (KWQAccObject*)CFDictionaryGetValue(accCache, renderer);
    if (!obj) {
        obj = [[KWQAccObject alloc] initWithRenderer: renderer]; // Initial ref happens here.
        setAccObject(renderer, obj);
    }

    return obj;
}

void KWQAccObjectCache::setAccObject(RenderObject* impl, KWQAccObject* accObject)
{
    if (!accCache)
        // No need to retain/free either impl key, or id value.
        accCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionarySetValue(accCache, (const void *)impl, accObject);
    ASSERT(!accCacheByID || CFDictionaryGetCount(accCache) >= CFDictionaryGetCount(accCacheByID));
}

void KWQAccObjectCache::removeAccObject(RenderObject* impl)
{
    if (!accCache)
        return;

    KWQAccObject* obj = (KWQAccObject*)CFDictionaryGetValue(accCache, impl);
    if (obj) {
        [obj detach];
        [obj release];
        CFDictionaryRemoveValue(accCache, impl);
    }

    ASSERT(!accCacheByID || CFDictionaryGetCount(accCache) >= CFDictionaryGetCount(accCacheByID));
}

KWQAccObjectID KWQAccObjectCache::getAccObjectID(KWQAccObject* accObject)
{
    KWQAccObjectID  accObjectID;

    // create the ID table as needed
    if (accCacheByID == NULL)
        accCacheByID = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    // check for already-assigned ID
    accObjectID = [accObject accObjectID];
    if (accObjectID != 0) {
        ASSERT(CFDictionaryContainsKey(accCacheByID, (const void *)accObjectID));
        return accObjectID;
    }
    
    // generate a new ID
    accObjectID = accObjectIDSource + 1;
    while (accObjectID == 0 || CFDictionaryContainsKey(accCacheByID, (const void *)accObjectID)) {
        ASSERT(accObjectID != accObjectIDSource);   // check for exhaustion
        accObjectID += 1;
    }
    accObjectIDSource = accObjectID;

    // assign the new ID to the object
    [accObject setAccObjectID:accObjectID];
    
    // add the object to the ID table
    CFDictionarySetValue(accCacheByID, (const void *)accObjectID, accObject);
    ASSERT(CFDictionaryContainsKey(accCacheByID, (const void *)accObjectID));
    
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
    ASSERT(accCacheByID != NULL);
    ASSERT(CFDictionaryContainsKey(accCacheByID, (const void *)accObjectID));
    CFDictionaryRemoveValue(accCacheByID, (const void *)accObjectID);
}

WebCoreTextMarker *KWQAccObjectCache::textMarkerForVisiblePosition (const VisiblePosition &visiblePos)
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
    if (!accCacheByID || !CFDictionaryContainsKey(accCacheByID, (const void *)textMarkerData.accObjectID))
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
    if (!accCache)
        return;
    
    KWQAccObject* obj = (KWQAccObject*)CFDictionaryGetValue(accCache, renderer);
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
