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
#import "AccessibilityObjectCache.h"

#import "WebCoreAXObject.h"
#import <kxmlcore/Assertions.h>
#import "FoundationExtras.h"
#import "DeprecatedString.h"
#import "RenderObject.h"
#import "WebCoreViewFactory.h"
#import "Document.h"

using WebCore::EAffinity;
using WebCore::RenderObject;
using WebCore::VisiblePosition;

// The simple Cocoa calls in this file don't throw exceptions.

bool AccessibilityObjectCache::gAccessibilityEnabled = false;

typedef struct KWQTextMarkerData  {
    WebCoreAXID  axObjectID;
    WebCore::Node*  nodeImpl;
    int             offset;
    EAffinity       affinity;
};

AccessibilityObjectCache::AccessibilityObjectCache()
{
    accCache = NULL;
    accCacheByID = NULL;
    accObjectIDSource = 0;
}

AccessibilityObjectCache::~AccessibilityObjectCache()
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

WebCoreAXObject* AccessibilityObjectCache::accObject(RenderObject* renderer)
{
    if (!accCache)
        // No need to retain/free either impl key, or id value.
        accCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    WebCoreAXObject* obj = (WebCoreAXObject*)CFDictionaryGetValue(accCache, renderer);
    if (!obj) {
        obj = [[WebCoreAXObject alloc] initWithRenderer: renderer]; // Initial ref happens here.
        setAccObject(renderer, obj);
    }

    return obj;
}

void AccessibilityObjectCache::setAccObject(RenderObject* impl, WebCoreAXObject* accObject)
{
    if (!accCache)
        // No need to retain/free either impl key, or id value.
        accCache = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    CFDictionarySetValue(accCache, (const void *)impl, accObject);
    ASSERT(!accCacheByID || CFDictionaryGetCount(accCache) >= CFDictionaryGetCount(accCacheByID));
}

void AccessibilityObjectCache::removeAccObject(RenderObject* impl)
{
    if (!accCache)
        return;

    WebCoreAXObject* obj = (WebCoreAXObject*)CFDictionaryGetValue(accCache, impl);
    if (obj) {
        [obj detach];
        [obj release];
        CFDictionaryRemoveValue(accCache, impl);
    }

    ASSERT(!accCacheByID || CFDictionaryGetCount(accCache) >= CFDictionaryGetCount(accCacheByID));
}

WebCoreAXID AccessibilityObjectCache::getAccObjectID(WebCoreAXObject* accObject)
{
    WebCoreAXID  axObjectID;

    // create the ID table as needed
    if (accCacheByID == NULL)
        accCacheByID = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    // check for already-assigned ID
    axObjectID = [accObject axObjectID];
    if (axObjectID != 0) {
        ASSERT(CFDictionaryContainsKey(accCacheByID, (const void *)axObjectID));
        return axObjectID;
    }
    
    // generate a new ID
    axObjectID = accObjectIDSource + 1;
    while (axObjectID == 0 || CFDictionaryContainsKey(accCacheByID, (const void *)axObjectID)) {
        ASSERT(axObjectID != accObjectIDSource);   // check for exhaustion
        axObjectID += 1;
    }
    accObjectIDSource = axObjectID;

    // assign the new ID to the object
    [accObject setAXObjectID:axObjectID];
    
    // add the object to the ID table
    CFDictionarySetValue(accCacheByID, (const void *)axObjectID, accObject);
    ASSERT(CFDictionaryContainsKey(accCacheByID, (const void *)axObjectID));
    
    return axObjectID;
}

void AccessibilityObjectCache::removeAXObjectID(WebCoreAXObject* accObject)
{
    // retrieve and clear the ID from the object, nothing to do if it was never assigned
    WebCoreAXID  axObjectID = [accObject axObjectID];
    if (axObjectID == 0)
        return;
    [accObject setAXObjectID:0];
    
    // remove the element from the lookup table
    ASSERT(accCacheByID != NULL);
    ASSERT(CFDictionaryContainsKey(accCacheByID, (const void *)axObjectID));
    CFDictionaryRemoveValue(accCacheByID, (const void *)axObjectID);
}

WebCoreTextMarker *AccessibilityObjectCache::textMarkerForVisiblePosition (const VisiblePosition &visiblePos)
{
    WebCore::Position deepPos = visiblePos.deepEquivalent();
    WebCore::Node* domNode = deepPos.node();
    ASSERT(domNode != NULL);
    if (domNode == NULL)
        return nil;
    
    // locate the renderer, which must exist for a visible dom node
    WebCore::RenderObject* renderer = domNode->renderer();
    ASSERT(renderer != NULL);
    
    // find or create an accessibility object for this renderer
    WebCoreAXObject* accObject = this->accObject(renderer);
    
    // create a text marker, adding an ID for the WebCoreAXObject if needed
    KWQTextMarkerData textMarkerData;
    textMarkerData.axObjectID = getAccObjectID(accObject);
    textMarkerData.nodeImpl = domNode;
    textMarkerData.offset = deepPos.offset();
    textMarkerData.affinity = visiblePos.affinity();
    return [[WebCoreViewFactory sharedFactory] textMarkerWithBytes:&textMarkerData length:sizeof(textMarkerData)]; 
}

VisiblePosition AccessibilityObjectCache::visiblePositionForTextMarker(WebCoreTextMarker *textMarker)
{
    KWQTextMarkerData textMarkerData;
    
    if (![[WebCoreViewFactory sharedFactory] getBytes:&textMarkerData fromTextMarker:textMarker length:sizeof(textMarkerData)])
        return VisiblePosition();
    
    // return empty position if the text marker is no longer valid
    if (!accCacheByID || !CFDictionaryContainsKey(accCacheByID, (const void *)textMarkerData.axObjectID))
        return VisiblePosition();

    // return the position from the data we stored earlier
    return VisiblePosition(textMarkerData.nodeImpl, textMarkerData.offset, textMarkerData.affinity);
}

void AccessibilityObjectCache::detach(RenderObject* renderer)
{
    removeAccObject(renderer);
}

void AccessibilityObjectCache::childrenChanged(RenderObject* renderer)
{
    if (!accCache)
        return;
    
    WebCoreAXObject* obj = (WebCoreAXObject*)CFDictionaryGetValue(accCache, renderer);
    if (!obj)
        return;
    
    [obj childrenChanged];
}

void AccessibilityObjectCache::postNotificationToTopWebArea(RenderObject* renderer, const DeprecatedString& msg)
{
    if (renderer) {
        RenderObject * obj = renderer->document()->topDocument()->renderer();
        NSAccessibilityPostNotification(accObject(obj), msg.getNSString());
    }
}

void AccessibilityObjectCache::postNotification(RenderObject* renderer, const DeprecatedString& msg)
{
    if (renderer)
        NSAccessibilityPostNotification(accObject(renderer), msg.getNSString());
}

void AccessibilityObjectCache::handleFocusedUIElementChanged()
{
    [[WebCoreViewFactory sharedFactory] accessibilityHandleFocusChanged];
}
