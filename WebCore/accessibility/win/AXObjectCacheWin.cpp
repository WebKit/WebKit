/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#include "Document.h"
#include "Page.h"
#include "RenderObject.h"

using namespace std;

namespace WebCore {

void AXObjectCache::detachWrapper(AccessibilityObject* obj)
{
    // On Windows, AccessibilityObjects are created when get_accChildCount is
    // called, but they are not wrapped until get_accChild is called, so this
    // object may not have a wrapper.
    if (AccessibilityObjectWrapper* wrapper = obj->wrapper())
        wrapper->detach();
}

void AXObjectCache::attachWrapper(AccessibilityObject*)
{
    // On Windows, AccessibilityObjects are wrapped when the accessibility
    // software requests them via get_accChild.
}

void AXObjectCache::postPlatformNotification(AccessibilityObject*, const String&)
{
}

AXID AXObjectCache::platformGenerateAXID() const
{
    static AXID lastUsedID = 0;

    // Generate a new ID. Windows accessibility relies on a positive AXID,
    // ranging from 1 to LONG_MAX.
    AXID objID = lastUsedID;
    do {
        ++objID;
        objID %= std::numeric_limits<LONG>::max();
    } while (objID == 0 || HashTraits<AXID>::isDeletedValue(objID) || m_idsInUse.contains(objID));

    ASSERT(objID >= 1 && objID <= std::numeric_limits<LONG>::max());

    lastUsedID = objID;

    return objID;
}

void AXObjectCache::handleFocusedUIElementChanged(RenderObject*, RenderObject* newFocusedRenderer)
{
    if (!newFocusedRenderer)
        return;

    Page* page = newFocusedRenderer->document()->page();
    if (!page || !page->chrome()->platformWindow())
        return;

    AccessibilityObject* focusedObject = focusedUIElementForPage(page);
    if (!focusedObject)
        return;

    ASSERT(!focusedObject->accessibilityIsIgnored());
    ASSERT(focusedObject->axObjectID() >= 1 && focusedObject->axObjectID() <= numeric_limits<LONG>::max());

    // Windows will end up calling get_accChild() on the root accessible
    // object for the WebView, passing the child ID that we specify below. We
    // negate the AXID so we know that the caller is passing the ID of an
    // element, not the index of a child element.
    NotifyWinEvent(EVENT_OBJECT_FOCUS, page->chrome()->platformWindow(), OBJID_CLIENT, -static_cast<LONG>(focusedObject->axObjectID()));
}

} // namespace WebCore
