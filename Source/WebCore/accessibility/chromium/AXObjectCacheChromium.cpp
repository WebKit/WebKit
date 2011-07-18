/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 Google Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "AXObjectCache.h"

#include "AccessibilityObject.h"
#include "AccessibilityScrollbar.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "RenderObject.h"
#include "Scrollbar.h"

namespace WebCore {

void AXObjectCache::detachWrapper(AccessibilityObject* obj)
{
    // In Chromium, AccessibilityObjects are wrapped lazily.
    if (AccessibilityObjectWrapper* wrapper = obj->wrapper())
        wrapper->detach();
}

void AXObjectCache::attachWrapper(AccessibilityObject*)
{
    // In Chromium, AccessibilityObjects are wrapped lazily.
}

void AXObjectCache::postPlatformNotification(AccessibilityObject* obj, AXNotification notification)
{
    if (obj && obj->isAccessibilityScrollbar() && notification == AXValueChanged) {
        // Send document value changed on scrollbar value changed notification.
        Scrollbar* scrollBar = static_cast<AccessibilityScrollbar*>(obj)->scrollbar();
        if (!scrollBar || !scrollBar->parent() || !scrollBar->parent()->isFrameView())
            return;
        Document* document = static_cast<FrameView*>(scrollBar->parent())->frame()->document();
        if (document != document->topDocument())
            return;
        obj = get(document->renderer());
    }
    
    if (!obj || !obj->document() || !obj->documentFrameView() || !obj->documentFrameView()->frame() || !obj->documentFrameView()->frame()->page())
        return;

    ChromeClient* client = obj->documentFrameView()->frame()->page()->chrome()->client();
    if (!client)
        return;

    switch (notification) {
    case AXActiveDescendantChanged:
        if (!obj->document()->focusedNode() || (obj->node() != obj->document()->focusedNode()))
            break;

        // Calling handleFocusedUIElementChanged will focus the new active
        // descendant and send the AXFocusedUIElementChanged notification.
        handleFocusedUIElementChanged(0, obj->document()->focusedNode()->renderer());
        break;
    case AXAutocorrectionOccured:
    case AXCheckedStateChanged:
    case AXChildrenChanged:
    case AXFocusedUIElementChanged:
    case AXLayoutComplete:
    case AXLiveRegionChanged:
    case AXLoadComplete:
    case AXMenuListValueChanged:
    case AXRowCollapsed:
    case AXRowCountChanged:
    case AXRowExpanded:
    case AXScrolledToAnchor:
    case AXSelectedChildrenChanged:
    case AXSelectedTextChanged:
    case AXValueChanged:
    case AXInvalidStatusChanged:
        break;
    }

    client->postAccessibilityNotification(obj, notification);        
}

void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject*, AXTextChange, unsigned, unsigned)
{
}

void AXObjectCache::handleFocusedUIElementChanged(RenderObject*, RenderObject* newFocusedRenderer)
{
    if (!newFocusedRenderer)
        return;

    Page* page = newFocusedRenderer->document()->page();
    if (!page)
        return;

    AccessibilityObject* focusedObject = focusedUIElementForPage(page);
    if (!focusedObject)
        return;

    postPlatformNotification(focusedObject, AXFocusedUIElementChanged);
}

void AXObjectCache::handleScrolledToAnchor(const Node* anchorNode)
{
    // The anchor node may not be accessible. Post the notification for the
    // first accessible object.
    postPlatformNotification(AccessibilityObject::firstAccessibleObjectFromNode(anchorNode), AXScrolledToAnchor);
}

} // namespace WebCore
