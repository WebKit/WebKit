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
#include "AccessibilityRenderObject.h"
#include "GOwnPtr.h"
#include "Range.h"
#include "SelectElement.h"
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

static void notifyChildrenSelectionChange(AccessibilityObject* object)
{
    // This static variable is needed to keep track of the old focused
    // object as per previous calls to this function, in order to
    // properly decide whether to emit some signals or not.
    static RefPtr<AccessibilityObject> oldFocusedObject = 0;

    // Only list boxes supported so far.
    if (!object || !object->isListBox())
        return;

    // Emit signal from the listbox's point of view first.
    g_signal_emit_by_name(object->wrapper(), "selection-changed");

    // Find the item where the selection change was triggered from.
    AccessibilityObject::AccessibilityChildrenVector items = object->children();
    SelectElement* select = toSelectElement(static_cast<Element*>(object->node()));
    if (!select)
        return;
    int changedItemIndex = select->activeSelectionStartListIndex();
    if (changedItemIndex < 0 || changedItemIndex >= static_cast<int>(items.size()))
        return;
    AccessibilityObject* item = items.at(changedItemIndex).get();

    // Ensure the oldFocusedObject belongs to the same document that
    // the current item so further comparisons make sense. Otherwise,
    // just reset oldFocusedObject so it won't be taken into account.
    if (item && oldFocusedObject && item->document() != oldFocusedObject->document())
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

    // Update pointer to the previously focused object.
    oldFocusedObject = item;
}

void AXObjectCache::postPlatformNotification(AccessibilityObject* coreObject, AXNotification notification)
{
    if (notification == AXCheckedStateChanged) {
        if (!coreObject->isCheckboxOrRadio())
            return;
        g_signal_emit_by_name(coreObject->wrapper(), "state-change", "checked", coreObject->isChecked());
    } else if (notification == AXSelectedChildrenChanged)
        notifyChildrenSelectionChange(coreObject);
}

static void emitTextChanged(AccessibilityRenderObject* object, AXObjectCache::AXTextChange textChange, unsigned offset, unsigned count)
{
    // Get the axObject for the parent object
    AtkObject* wrapper = object->parentObjectUnignored()->wrapper();
    if (!wrapper || !ATK_IS_TEXT(wrapper))
        return;

    // Select the right signal to be emitted
    CString detail;
    switch (textChange) {
    case AXObjectCache::AXTextInserted:
        detail = "text-changed::insert";
        break;
    case AXObjectCache::AXTextDeleted:
        detail = "text-changed::delete";
        break;
    }

    if (!detail.isNull())
        g_signal_emit_by_name(wrapper, detail.data(), offset, count);
}

void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject* object, AXTextChange textChange, unsigned offset, unsigned count)
{
    // Sanity check
    if (count < 1 || !object || !object->isAccessibilityRenderObject())
        return;

    Node* node = object->node();
    RefPtr<Range> range = Range::create(node->document(),  Position(node->parentNode(), 0), Position(node, 0));
    emitTextChanged(toAccessibilityRenderObject(object), textChange, offset + TextIterator::rangeLength(range.get()), count);
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
