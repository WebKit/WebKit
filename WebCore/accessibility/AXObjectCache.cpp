/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "AccessibilityARIAGridRow.h"
#include "AccessibilityARIAGridCell.h"
#include "AccessibilityList.h"
#include "AccessibilityListBox.h"
#include "AccessibilityListBoxOption.h"
#include "AccessibilityImageMapLink.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilityTable.h"
#include "AccessibilityTableCell.h"
#include "AccessibilityTableColumn.h"
#include "AccessibilityTableHeaderContainer.h"
#include "AccessibilityTableRow.h"
#include "HTMLNames.h"
#include "RenderObject.h"
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
    
bool AXObjectCache::nodeIsAriaType(Node* node, String role)
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
        else if (node && (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(dlTag)))
            newObj = AccessibilityList::create(renderer);
        
        // aria tables
        else if (nodeIsAriaType(node, "grid"))
            newObj = AccessibilityARIAGrid::create(renderer);
        else if (nodeIsAriaType(node, "row"))
            newObj = AccessibilityARIAGridRow::create(renderer);
        else if (nodeIsAriaType(node, "gridcell") || nodeIsAriaType(node, "columnheader") || nodeIsAriaType(node, "rowheader"))
            newObj = AccessibilityARIAGridCell::create(renderer);

        // standard tables
        else if (renderer->isTable())
            newObj = AccessibilityTable::create(renderer);
        else if (renderer->isTableRow())
            newObj = AccessibilityTableRow::create(renderer);
        else if (renderer->isTableCell())
            newObj = AccessibilityTableCell::create(renderer);

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
    if (!m_objects.take(axID)) {
        return;
    }
    
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

AXID AXObjectCache::getAXID(AccessibilityObject* obj)
{
    // check for already-assigned ID
    AXID objID = obj->axObjectID();
    if (objID) {
        ASSERT(m_idsInUse.contains(objID));
        return objID;
    }
    
    // generate a new ID
    static AXID lastUsedID = 0;
    objID = lastUsedID;
    do
        ++objID;
    while (objID == 0 || HashTraits<AXID>::isDeletedValue(objID) || m_idsInUse.contains(objID));
    m_idsInUse.add(objID);
    lastUsedID = objID;
    obj->setAXObjectID(objID);
    
    return objID;
}

void AXObjectCache::removeAXID(AccessibilityObject* obj)
{
    if (!obj)
        return;
    
    AXID objID = obj->axObjectID();
    if (objID == 0)
        return;
    ASSERT(!HashTraits<AXID>::isDeletedValue(objID));
    ASSERT(m_idsInUse.contains(objID));
    obj->setAXObjectID(0);
    m_idsInUse.remove(objID);
}

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
        AccessibilityObject* obj = m_notificationsToPost[i].first;
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
void AXObjectCache::postNotification(RenderObject* renderer, const String& message, bool postToElement)
{
    // Notifications for text input objects are sent to that object.
    // All others are sent to the top WebArea.
    if (!renderer)
        return;
    
    // Get an accessibility object that already exists. One should not be created here
    // because a render update may be in progress and creating an AX object can re-trigger a layout
    RefPtr<AccessibilityObject> obj = get(renderer);
    while (!obj && renderer) {
        renderer = renderer->parent();
        obj = get(renderer); 
    }
    
    if (!renderer)
        return;

    if (obj && !postToElement)
        obj = obj->observableObject();
    
    Document* document = renderer->document();
    if (!obj && document)
        obj = get(document->renderer());
    
    if (!obj)
        return;

    m_notificationsToPost.append(make_pair(obj.get(), message));
    if (!m_notificationPostTimer.isActive())
        m_notificationPostTimer.startOneShot(0);
}

void AXObjectCache::selectedChildrenChanged(RenderObject* renderer)
{
    postNotification(renderer, "AXSelectedChildrenChanged", true);
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
        static_cast<AccessibilityRenderObject*>(obj)->setAriaRole();
}
#endif

} // namespace WebCore
