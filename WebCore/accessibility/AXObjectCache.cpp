/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXObjectCache.h"

#include "AccessibilityARIAGrid.h"
#include "AccessibilityARIAGridCell.h"
#include "AccessibilityARIAGridRow.h"
#include "AccessibilityImageMapLink.h"
#include "AccessibilityList.h"
#include "AccessibilityListBox.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityMediaControls.h"
#include "AccessibilityMenuList.h"
#include "AccessibilityMenuListOption.h"
#include "AccessibilityMenuListPopup.h"
#include "AccessibilityProgressIndicator.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilityScrollbar.h"
#include "AccessibilitySlider.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableHeaderContainer.h"
#include "AccessibilityTableRow.h"
#include "FocusController.h"
#include "Frame.h"
#include "HTMLAreaElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#if ENABLE(VIDEO)
#include "MediaControlElements.h"
#endif
#include "InputElement.h"
#include "Page.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderView.h"

#include <wtf/PassRefPtr.h>

namespace WebCore {

using namespace HTMLNames;
    
bool AXObjectCache::gAccessibilityEnabled = false;
bool AXObjectCache::gAccessibilityEnhancedUserInterfaceEnabled = false;

AXObjectCache::AXObjectCache()
    : m_notificationPostTimer(this, &AXObjectCache::notificationPostTimerFired)
{
}

AXObjectCache::~AXObjectCache()
{
    HashMap<AXID, RefPtr<AccessibilityObject> >::iterator end = m_objects.end();
    for (HashMap<AXID, RefPtr<AccessibilityObject> >::iterator it = m_objects.begin(); it != end; ++it) {
        AccessibilityObject* obj = (*it).second.get();
        detachWrapper(obj);
        obj->detach();
        removeAXID(obj);
    }
}

AccessibilityObject* AXObjectCache::focusedImageMapUIElement(HTMLAreaElement* areaElement)
{
    // Find the corresponding accessibility object for the HTMLAreaElement. This should be
    // in the list of children for its corresponding image.
    if (!areaElement)
        return 0;
    
    HTMLImageElement* imageElement = areaElement->imageElement();
    if (!imageElement)
        return 0;
    
    AccessibilityObject* axRenderImage = areaElement->document()->axObjectCache()->getOrCreate(imageElement->renderer());
    if (!axRenderImage)
        return 0;
    
    AccessibilityObject::AccessibilityChildrenVector imageChildren = axRenderImage->children();
    unsigned count = imageChildren.size();
    for (unsigned k = 0; k < count; ++k) {
        AccessibilityObject* child = imageChildren[k].get();
        if (!child->isImageMapLink())
            continue;
        
        if (static_cast<AccessibilityImageMapLink*>(child)->areaElement() == areaElement)
            return child;
    }    
    
    return 0;
}
    
AccessibilityObject* AXObjectCache::focusedUIElementForPage(const Page* page)
{
    // get the focused node in the page
    Document* focusedDocument = page->focusController()->focusedOrMainFrame()->document();
    Node* focusedNode = focusedDocument->focusedNode();
    if (!focusedNode)
        focusedNode = focusedDocument;

    if (focusedNode->hasTagName(areaTag))
        return focusedImageMapUIElement(static_cast<HTMLAreaElement*>(focusedNode));
    
    RenderObject* focusedNodeRenderer = focusedNode->renderer();
    if (!focusedNodeRenderer)
        return 0;

    AccessibilityObject* obj = focusedNodeRenderer->document()->axObjectCache()->getOrCreate(focusedNodeRenderer);

    if (obj->shouldFocusActiveDescendant()) {
        if (AccessibilityObject* descendant = obj->activeDescendant())
            obj = descendant;
    }

    // the HTML element, for example, is focusable but has an AX object that is ignored
    if (obj->accessibilityIsIgnored())
        obj = obj->parentObjectUnignored();

    return obj;
}

AccessibilityObject* AXObjectCache::get(RenderObject* renderer)
{
    if (!renderer)
        return 0;
    
    AccessibilityObject* obj = 0;
    AXID axID = m_renderObjectMapping.get(renderer);
    ASSERT(!HashTraits<AXID>::isDeletedValue(axID));

    if (axID)
        obj = m_objects.get(axID).get();
    
    return obj;
}
    
bool AXObjectCache::nodeHasRole(Node* node, const AtomicString& role)
{
    if (!node || !node->isElementNode())
        return false;
    
    return equalIgnoringCase(static_cast<Element*>(node)->getAttribute(roleAttr), role);
}

AccessibilityObject* AXObjectCache::getOrCreate(RenderObject* renderer)
{
    if (!renderer)
        return 0;
    
    AccessibilityObject* obj = get(renderer);

    if (!obj) {
        Node* node = renderer->node();
        RefPtr<AccessibilityObject> newObj = 0;
        if (renderer->isListBox())
            newObj = AccessibilityListBox::create(renderer);
        else if (renderer->isMenuList())
            newObj = AccessibilityMenuList::create(renderer);

        // If the node is aria role="list" or the aria role is empty and its a ul/ol/dl type (it shouldn't be a list if aria says otherwise). 
        else if (node && ((nodeHasRole(node, "list") || nodeHasRole(node, "directory"))
                          || (nodeHasRole(node, nullAtom) && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(dlTag)))))
            newObj = AccessibilityList::create(renderer);
        
        // aria tables
        else if (nodeHasRole(node, "grid") || nodeHasRole(node, "treegrid"))
            newObj = AccessibilityARIAGrid::create(renderer);
        else if (nodeHasRole(node, "row"))
            newObj = AccessibilityARIAGridRow::create(renderer);
        else if (nodeHasRole(node, "gridcell") || nodeHasRole(node, "columnheader") || nodeHasRole(node, "rowheader"))
            newObj = AccessibilityARIAGridCell::create(renderer);

        // standard tables
        else if (renderer->isTable())
            newObj = AccessibilityTable::create(renderer);
        else if (renderer->isTableRow())
            newObj = AccessibilityTableRow::create(renderer);
        else if (renderer->isTableCell())
            newObj = AccessibilityTableCell::create(renderer);

#if ENABLE(VIDEO)
        // media controls
        else if (renderer->node() && renderer->node()->isMediaControlElement())
            newObj = AccessibilityMediaControl::create(renderer);
#endif

#if ENABLE(PROGRESS_TAG)
        // progress bar
        else if (renderer->isProgress())
            newObj = AccessibilityProgressIndicator::create(toRenderProgress(renderer));
#endif

        // input type=range
        else if (renderer->isSlider())
            newObj = AccessibilitySlider::create(renderer);

        else
            newObj = AccessibilityRenderObject::create(renderer);
        
        obj = newObj.get();
        
        getAXID(obj);
        
        m_renderObjectMapping.set(renderer, obj->axObjectID());
        m_objects.set(obj->axObjectID(), obj);    
        attachWrapper(obj);
    }
    
    return obj;
}

AccessibilityObject* AXObjectCache::getOrCreate(AccessibilityRole role)
{
    RefPtr<AccessibilityObject> obj = 0;
    
    // will be filled in...
    switch (role) {
    case ListBoxOptionRole:
        obj = AccessibilityListBoxOption::create();
        break;
    case ImageMapLinkRole:
        obj = AccessibilityImageMapLink::create();
        break;
    case ColumnRole:
        obj = AccessibilityTableColumn::create();
        break;            
    case TableHeaderContainerRole:
        obj = AccessibilityTableHeaderContainer::create();
        break;   
    case SliderThumbRole:
        obj = AccessibilitySliderThumb::create();
        break;
    case MenuListPopupRole:
        obj = AccessibilityMenuListPopup::create();
        break;
    case MenuListOptionRole:
        obj = AccessibilityMenuListOption::create();
        break;
    case ScrollBarRole:
        obj = AccessibilityScrollbar::create();
        break;
    default:
        obj = 0;
    }
    
    if (obj)
        getAXID(obj.get());
    else
        return 0;

    m_objects.set(obj->axObjectID(), obj);    
    attachWrapper(obj.get());
    return obj.get();
}

void AXObjectCache::remove(AXID axID)
{
    if (!axID)
        return;
    
    // first fetch object to operate some cleanup functions on it 
    AccessibilityObject* obj = m_objects.get(axID).get();
    if (!obj)
        return;
    
    detachWrapper(obj);
    obj->detach();
    removeAXID(obj);
    
    // finally remove the object
    if (!m_objects.take(axID))
        return;
    
    ASSERT(m_objects.size() >= m_idsInUse.size());    
}
    
void AXObjectCache::remove(RenderObject* renderer)
{
    if (!renderer)
        return;
    
    AXID axID = m_renderObjectMapping.get(renderer);
    remove(axID);
    m_renderObjectMapping.remove(renderer);
}

#if !PLATFORM(WIN)
AXID AXObjectCache::platformGenerateAXID() const
{
    static AXID lastUsedID = 0;

    // Generate a new ID.
    AXID objID = lastUsedID;
    do {
        ++objID;
    } while (!objID || HashTraits<AXID>::isDeletedValue(objID) || m_idsInUse.contains(objID));

    lastUsedID = objID;

    return objID;
}
#endif

AXID AXObjectCache::getAXID(AccessibilityObject* obj)
{
    // check for already-assigned ID
    AXID objID = obj->axObjectID();
    if (objID) {
        ASSERT(m_idsInUse.contains(objID));
        return objID;
    }

    objID = platformGenerateAXID();

    m_idsInUse.add(objID);
    obj->setAXObjectID(objID);
    
    return objID;
}

void AXObjectCache::removeAXID(AccessibilityObject* object)
{
    if (!object)
        return;
    
    AXID objID = object->axObjectID();
    if (!objID)
        return;
    ASSERT(!HashTraits<AXID>::isDeletedValue(objID));
    ASSERT(m_idsInUse.contains(objID));
    object->setAXObjectID(0);
    m_idsInUse.remove(objID);
}

#if HAVE(ACCESSIBILITY)
void AXObjectCache::contentChanged(RenderObject* renderer)
{
    AccessibilityObject* object = getOrCreate(renderer);
    if (object)
        object->contentChanged(); 
}
#endif

void AXObjectCache::childrenChanged(RenderObject* renderer)
{
    if (!renderer)
        return;
 
    AXID axID = m_renderObjectMapping.get(renderer);
    if (!axID)
        return;
    
    AccessibilityObject* obj = m_objects.get(axID).get();
    if (obj)
        obj->childrenChanged();
}
    
void AXObjectCache::notificationPostTimerFired(Timer<AXObjectCache>*)
{
    m_notificationPostTimer.stop();

    unsigned i = 0, count = m_notificationsToPost.size();
    for (i = 0; i < count; ++i) {
        AccessibilityObject* obj = m_notificationsToPost[i].first.get();
#ifndef NDEBUG
        // Make sure none of the render views are in the process of being layed out.
        // Notifications should only be sent after the renderer has finished
        if (obj->isAccessibilityRenderObject()) {
            AccessibilityRenderObject* renderObj = static_cast<AccessibilityRenderObject*>(obj);
            RenderObject* renderer = renderObj->renderer();
            if (renderer && renderer->view())
                ASSERT(!renderer->view()->layoutState());
        }
#endif
        
        postPlatformNotification(obj, m_notificationsToPost[i].second);
    }
    
    m_notificationsToPost.clear();
}
    
#if HAVE(ACCESSIBILITY)
void AXObjectCache::postNotification(RenderObject* renderer, AXNotification notification, bool postToElement, PostType postType)
{
    // Notifications for text input objects are sent to that object.
    // All others are sent to the top WebArea.
    if (!renderer)
        return;
    
    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> object = get(renderer);
    while (!object && renderer) {
        renderer = renderer->parent();
        object = get(renderer); 
    }
    
    if (!renderer)
        return;
    
    postNotification(object.get(), renderer->document(), notification, postToElement, postType);
}

void AXObjectCache::postNotification(AccessibilityObject* object, Document* document, AXNotification notification, bool postToElement, PostType postType)
{
    if (object && !postToElement)
        object = object->observableObject();

    if (!object && document)
        object = get(document->renderer());

    if (!object)
        return;

    if (postType == PostAsynchronously) {
        m_notificationsToPost.append(make_pair(object, notification));
        if (!m_notificationPostTimer.isActive())
            m_notificationPostTimer.startOneShot(0);
    } else
        postPlatformNotification(object, notification);
}

void AXObjectCache::selectedChildrenChanged(RenderObject* renderer)
{
    // postToElement is false so that you can pass in any child of an element and it will go up the parent tree
    // to find the container which should send out the notification.
    postNotification(renderer, AXSelectedChildrenChanged, false);
}
#endif

#if HAVE(ACCESSIBILITY)
void AXObjectCache::handleActiveDescendantChanged(RenderObject* renderer)
{
    if (!renderer)
        return;
    AccessibilityObject* obj = getOrCreate(renderer);
    if (obj)
        obj->handleActiveDescendantChanged();
}

void AXObjectCache::handleAriaRoleChanged(RenderObject* renderer)
{
    if (!renderer)
        return;
    AccessibilityObject* obj = getOrCreate(renderer);
    if (obj && obj->isAccessibilityRenderObject())
        static_cast<AccessibilityRenderObject*>(obj)->updateAccessibilityRole();
}
#endif
    
VisiblePosition AXObjectCache::visiblePositionForTextMarkerData(TextMarkerData& textMarkerData)
{
    VisiblePosition visiblePos = VisiblePosition(textMarkerData.node, textMarkerData.offset, textMarkerData.affinity);
    Position deepPos = visiblePos.deepEquivalent();
    if (deepPos.isNull())
        return VisiblePosition();
    
    RenderObject* renderer = deepPos.node()->renderer();
    if (!renderer)
        return VisiblePosition();
    
    AXObjectCache* cache = renderer->document()->axObjectCache();
    if (!cache->isIDinUse(textMarkerData.axID))
        return VisiblePosition();
    
    if (deepPos.node() != textMarkerData.node || deepPos.deprecatedEditingOffset() != textMarkerData.offset)
        return VisiblePosition();
    
    return visiblePos;
}

void AXObjectCache::textMarkerDataForVisiblePosition(TextMarkerData& textMarkerData, const VisiblePosition& visiblePos)
{
    // This memory must be bzero'd so instances of TextMarkerData can be tested for byte-equivalence.
    // This also allows callers to check for failure by looking at textMarkerData upon return.
    memset(&textMarkerData, 0, sizeof(TextMarkerData));
    
    if (visiblePos.isNull())
        return;
    
    Position deepPos = visiblePos.deepEquivalent();
    Node* domNode = deepPos.node();
    ASSERT(domNode);
    if (!domNode)
        return;
    
    if (domNode->isHTMLElement()) {
        InputElement* inputElement = toInputElement(static_cast<Element*>(domNode));
        if (inputElement && inputElement->isPasswordField())
            return;
    }
    
    // locate the renderer, which must exist for a visible dom node
    RenderObject* renderer = domNode->renderer();
    ASSERT(renderer);
    
    // find or create an accessibility object for this renderer
    AXObjectCache* cache = renderer->document()->axObjectCache();
    RefPtr<AccessibilityObject> obj = cache->getOrCreate(renderer);
    
    textMarkerData.axID = obj.get()->axObjectID();
    textMarkerData.node = domNode;
    textMarkerData.offset = deepPos.deprecatedEditingOffset();
    textMarkerData.affinity = visiblePos.affinity();    
}
    
} // namespace WebCore
