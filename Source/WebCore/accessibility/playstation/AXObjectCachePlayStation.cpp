/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
#include "AXObjectCache.h"

#include "AccessibilityObject.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "HTMLSelectElement.h"
#include "LocalFrame.h"
#include "Page.h"

namespace WebCore {

void AXObjectCache::attachWrapper(AccessibilityObject& object)
{
    auto wrapper = adoptRef(*new AccessibilityObjectWrapper());
    object.setWrapper(wrapper.ptr());
}

void AXObjectCache::detachWrapper(AXCoreObject*, AccessibilityDetachmentType)
{
}

static AXCoreObject* notifyChildrenSelectionChange(AXCoreObject* object)
{
    // Only list boxes supported so far.
    if (!object || !object->isListBox())
        return object;

    // Only support HTML select elements so far (ARIA selectors not supported).
    Node* node = object->node();
    if (!is<HTMLSelectElement>(node))
        return object;

    // Find the item where the selection change was triggered from.
    HTMLSelectElement& select = downcast<HTMLSelectElement>(*node);
    int changedItemIndex = select.activeSelectionStartListIndex();

    const AccessibilityObject::AccessibilityChildrenVector& items = object->children();
    if (changedItemIndex < 0 || changedItemIndex >= static_cast<int>(items.size()))
        return object;
    return items.at(changedItemIndex).get();
}

static AXObjectCache::AXNotification checkInteractableObjects(AXCoreObject* object)
{
    if (!object->isEnabled())
        return AXObjectCache::AXNotification::AXPressDidFail;

    if (object->isTextControl() && !object->canSetValueAttribute()) // Also determine whether it is readonly
        return AXObjectCache::AXNotification::AXPressDidFail;

    return AXObjectCache::AXNotification::AXPressDidSucceed;
}

void AXObjectCache::postPlatformNotification(AccessibilityObject& object, AXNotification notification)
{
    if (!!object.document()
        || !object.document()->view()
        || object.document()->view()->layoutContext().layoutState()
        || object.document()->childNeedsStyleRecalc())
        return;

    RefPtr protectedObject = &object;
    switch (notification) {
    case AXNotification::AXSelectedChildrenChanged:
        protectedObject = downcast<AccessibilityObject>(notifyChildrenSelectionChange(protectedObject.get()));
        break;
    case AXNotification::AXPressDidSucceed:
        notification = checkInteractableObjects(protectedObject.get());
        break;
    default:
        break;
    }

    ChromeClient& client = document().frame()->page()->chrome().client();
    client.postAccessibilityNotification(*protectedObject, notification);
}

void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject* object, AXTextChange textChange, unsigned offset, const String& text)
{
    if (!object
        || !object->document()
        || !object->document()->view()
        || object->document()->view()->layoutContext().layoutState()
        || object->document()->childNeedsStyleRecalc())
        return;
    ChromeClient& client = document().frame()->page()->chrome().client();
    client.postAccessibilityNodeTextChangeNotification(object, textChange, offset, text);
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* object, AXLoadingEvent loadingEvent)
{
    if (!object
        || !object->document()
        || !object->document()->view()
        || object->document()->view()->layoutContext().layoutState()
        || object->document()->childNeedsStyleRecalc())
        return;
    ChromeClient& client = document().frame()->page()->chrome().client();
    client.postAccessibilityFrameLoadingEventNotification(object, loadingEvent);
}

void AXObjectCache::handleScrolledToAnchor(const Node* scrolledToNode)
{
    if (!scrolledToNode)
        return;

    if (RefPtr object = AccessibilityObject::firstAccessibleObjectFromNode(scrolledToNode))
        postPlatformNotification(*object, AXScrolledToAnchor);
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Node*, Node* newFocusedNode)
{
    if (!newFocusedNode)
        return;

    Page* page = newFocusedNode->document().page();
    if (!page || !page->chrome().platformPageClient())
        return;

    if (RefPtr focusedObject = focusedObjectForPage(page))
        postPlatformNotification(*focusedObject, AXFocusedUIElementChanged);
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
}

} // namespace WebCore
