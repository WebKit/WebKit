/*
 * Copyright (C) 2008 Nuanti Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "AXObjectCache.h"

#include "AccessibilityObject.h"
#include "AccessibilityObjectWrapperAtk.h"
#include "Document.h"
#include "Element.h"
#include "GOwnPtr.h"
#include "HTMLSelectElement.h"
#include "Range.h"
#include "TextIterator.h"

namespace WebCore {

void AXObjectCache::detachWrapper(AccessibilityObject* obj)
{
    webkit_accessible_detach(WEBKIT_ACCESSIBLE(obj->wrapper()));
}

void AXObjectCache::attachWrapper(AccessibilityObject* obj)
{
    AtkObject* atkObj = ATK_OBJECT(webkit_accessible_new(obj));
    obj->setWrapper(atkObj);
    g_object_unref(atkObj);
}

static AccessibilityObject* getListObject(AccessibilityObject* object)
{
    // Only list boxes and menu lists supported so far.
    if (!object->isListBox() && !object->isMenuList())
        return 0;

    // For list boxes the list object is just itself.
    if (object->isListBox())
        return object;

    // For menu lists we need to return the first accessible child,
    // with role MenuListPopupRole, since that's the one holding the list
    // of items with role MenuListOptionRole.
    AccessibilityObject::AccessibilityChildrenVector children = object->children();
    if (!children.size())
        return 0;

    AccessibilityObject* listObject = children.at(0).get();
    if (!listObject->isMenuListPopup())
        return 0;

    return listObject;
}

static void notifyChildrenSelectionChange(AccessibilityObject* object)
{
    // This static variables are needed to keep track of the old
    // focused object and its associated list object, as per previous
    // calls to this function, in order to properly decide whether to
    // emit some signals or not.
    DEFINE_STATIC_LOCAL(RefPtr<AccessibilityObject>, oldListObject, ());
    DEFINE_STATIC_LOCAL(RefPtr<AccessibilityObject>, oldFocusedObject, ());

    // Only list boxes and menu lists supported so far.
    if (!object || !(object->isListBox() || object->isMenuList()))
        return;

    // Emit signal from the listbox's point of view first.
    g_signal_emit_by_name(object->wrapper(), "selection-changed");

    // Find the item where the selection change was triggered from.
    HTMLSelectElement* select = toHTMLSelectElement(object->node());
    if (!select)
        return;
    int changedItemIndex = select->activeSelectionStartListIndex();

    AccessibilityObject* listObject = getListObject(object);
    if (!listObject) {
        oldListObject = 0;
        return;
    }

    AccessibilityObject::AccessibilityChildrenVector items = listObject->children();
    if (changedItemIndex < 0 || changedItemIndex >= static_cast<int>(items.size()))
        return;
    AccessibilityObject* item = items.at(changedItemIndex).get();

    // Ensure the current list object is the same than the old one so
    // further comparisons make sense. Otherwise, just reset
    // oldFocusedObject so it won't be taken into account.
    if (oldListObject != listObject)
        oldFocusedObject = 0;

    AtkObject* axItem = item ? item->wrapper() : 0;
    AtkObject* axOldFocusedObject = oldFocusedObject ? oldFocusedObject->wrapper() : 0;

    // Old focused object just lost focus, so emit the events.
    if (axOldFocusedObject && axItem != axOldFocusedObject) {
        g_signal_emit_by_name(axOldFocusedObject, "focus-event", false);
        g_signal_emit_by_name(axOldFocusedObject, "state-change", "focused", false);
    }

    // Emit needed events for the currently (un)selected item.
    if (axItem) {
        bool isSelected = item->isSelected();
        g_signal_emit_by_name(axItem, "state-change", "selected", isSelected);
        g_signal_emit_by_name(axItem, "focus-event", isSelected);
        g_signal_emit_by_name(axItem, "state-change", "focused", isSelected);
    }

    // Update pointers to the previously involved objects.
    oldListObject = listObject;
    oldFocusedObject = item;
}

void AXObjectCache::postPlatformNotification(AccessibilityObject* coreObject, AXNotification notification)
{
    AtkObject* axObject = coreObject->wrapper();
    if (!axObject)
        return;

    if (notification == AXCheckedStateChanged) {
        if (!coreObject->isCheckboxOrRadio())
            return;
        g_signal_emit_by_name(axObject, "state-change", "checked", coreObject->isChecked());
    } else if (notification == AXSelectedChildrenChanged || notification == AXMenuListValueChanged) {
        if (notification == AXMenuListValueChanged && coreObject->isMenuList()) {
            g_signal_emit_by_name(axObject, "focus-event", true);
            g_signal_emit_by_name(axObject, "state-change", "focused", true);
        }
        notifyChildrenSelectionChange(coreObject);
    } else if (notification == AXValueChanged) {
        if (!ATK_IS_VALUE(axObject))
            return;

        AtkPropertyValues propertyValues;
        propertyValues.property_name = "accessible-value";

        memset(&propertyValues.new_value,  0, sizeof(GValue));
        atk_value_get_current_value(ATK_VALUE(axObject), &propertyValues.new_value);

        g_signal_emit_by_name(ATK_OBJECT(axObject), "property-change::accessible-value", &propertyValues, NULL);
    }
}

static void emitTextChanged(AccessibilityObject* object, AXObjectCache::AXTextChange textChange, unsigned offset, const String& text)
{
    AtkObject* wrapper = object->parentObjectUnignored()->wrapper();
    if (!wrapper || !ATK_IS_TEXT(wrapper))
        return;

    // Select the right signal to be emitted
    CString detail;
    switch (textChange) {
    case AXObjectCache::AXTextInserted:
        detail = "text-insert";
        break;
    case AXObjectCache::AXTextDeleted:
        detail = "text-remove";
        break;
    }

    if (!detail.isNull())
        g_signal_emit_by_name(wrapper, detail.data(), offset, text.length(), text.utf8().data());
}

void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject* object, AXTextChange textChange, unsigned offset, const String& text)
{
    if (!object || !object->isAccessibilityRenderObject() || text.isEmpty())
        return;

    Node* node = object->node();
    RefPtr<Range> range = Range::create(node->document(), node->parentNode(), 0, node, 0);
    emitTextChanged(object, textChange, offset + TextIterator::rangeLength(range.get()), text);
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* object, AXLoadingEvent loadingEvent)
{
    if (!object)
        return;

    AtkObject* axObject = object->wrapper();
    if (!axObject || !ATK_IS_DOCUMENT(axObject))
        return;

    switch (loadingEvent) {
    case AXObjectCache::AXLoadingStarted:
        g_signal_emit_by_name(axObject, "state-change", "busy", true);
        break;
    case AXObjectCache::AXLoadingReloaded:
        g_signal_emit_by_name(axObject, "state-change", "busy", true);
        g_signal_emit_by_name(axObject, "reload");
        break;
    case AXObjectCache::AXLoadingFailed:
        g_signal_emit_by_name(axObject, "load-stopped");
        g_signal_emit_by_name(axObject, "state-change", "busy", false);
        break;
    case AXObjectCache::AXLoadingFinished:
        g_signal_emit_by_name(axObject, "load-complete");
        g_signal_emit_by_name(axObject, "state-change", "busy", false);
        break;
    }
}

void AXObjectCache::handleFocusedUIElementChanged(RenderObject* oldFocusedRender, RenderObject* newFocusedRender)
{
    RefPtr<AccessibilityObject> oldObject = getOrCreate(oldFocusedRender);
    if (oldObject) {
        g_signal_emit_by_name(oldObject->wrapper(), "focus-event", false);
        g_signal_emit_by_name(oldObject->wrapper(), "state-change", "focused", false);
    }
    RefPtr<AccessibilityObject> newObject = getOrCreate(newFocusedRender);
    if (newObject) {
        g_signal_emit_by_name(newObject->wrapper(), "focus-event", true);
        g_signal_emit_by_name(newObject->wrapper(), "state-change", "focused", true);
    }
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
}

} // namespace WebCore
