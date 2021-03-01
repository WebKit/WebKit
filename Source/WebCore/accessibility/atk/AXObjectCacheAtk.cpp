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

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityObject.h"
#include "AccessibilityRenderObject.h"
#include "Document.h"
#include "Element.h"
#include "HTMLSelectElement.h"
#include "Range.h"
#include "TextIterator.h"
#include "WebKitAccessible.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

static AtkObject* wrapperParent(WebKitAccessible* wrapper)
{
    // Look for the right object to emit the signal from, but using the implementation
    // of atk_object_get_parent from AtkObject class (which uses a cached pointer if set)
    // since the accessibility hierarchy in WebCore will no longer be navigable.
    gpointer webkitAccessibleClass = g_type_class_peek_parent(WEBKIT_ACCESSIBLE_GET_CLASS(wrapper));
    gpointer atkObjectClass = g_type_class_peek_parent(webkitAccessibleClass);
    AtkObject* atkParent = ATK_OBJECT_CLASS(atkObjectClass)->get_parent(ATK_OBJECT(wrapper));
    // We don't want to emit any signal from an object outside WebKit's world.
    return WEBKIT_IS_ACCESSIBLE(atkParent) ? atkParent : nullptr;
}

void AXObjectCache::detachWrapper(AXCoreObject* obj, AccessibilityDetachmentType detachmentType)
{
    auto* wrapper = obj->wrapper();
    ASSERT(wrapper);

    // If an object is being detached NOT because of the AXObjectCache being destroyed,
    // then it's being removed from the accessibility tree and we should emit a signal.
    if (detachmentType != AccessibilityDetachmentType::CacheDestroyed && obj->document() && wrapperParent(wrapper))
        m_deferredDetachedWrapperList.add(wrapper);

    webkitAccessibleDetach(WEBKIT_ACCESSIBLE(wrapper));
}

void AXObjectCache::attachWrapper(AXCoreObject* obj)
{
    // FIXME: at the moment, only allow to attach AccessibilityObjects.
    if (!is<AccessibilityObject>(obj))
        return;
    AccessibilityObject* accessibilityObject = downcast<AccessibilityObject>(obj);

    GRefPtr<WebKitAccessible> wrapper = adoptGRef(webkitAccessibleNew(accessibilityObject));
    accessibilityObject->setWrapper(wrapper.get());

    // If an object is being attached and we are not in the middle of a layout update, then
    // we should report ATs by emitting the children-changed::add signal from the parent.
    Document* document = accessibilityObject->document();
    if (!document || document->childNeedsStyleRecalc())
        return;

    // Don't emit the signal when the actual object being added is not going to be exposed.
    if (accessibilityObject->accessibilityIsIgnoredByDefault())
        return;

    // Don't emit the signal if the object being added is not -- or not yet -- rendered,
    // which can occur in nested iframes. In these instances we don't want to ignore the
    // child. But if an assistive technology is listening, AT-SPI2 will attempt to create
    // and cache the state set for the child upon emission of the signal. If the object
    // has not yet been rendered, this will result in a crash.
    if (!accessibilityObject->renderer())
        return;

    m_deferredAttachedWrapperObjectList.add(accessibilityObject);
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
    for (auto& coreObject : m_deferredAttachedWrapperObjectList) {
        auto* wrapper = coreObject->wrapper();
        if (!wrapper)
            continue;

        // Don't emit the signal for objects whose parents won't be exposed directly.
        auto* coreParent = coreObject->parentObjectUnignored();
        if (!coreParent || coreParent->accessibilityIsIgnoredByDefault())
            continue;

        // Look for the right object to emit the signal from.
        auto* atkParent = coreParent->wrapper();
        if (!atkParent)
            continue;

        size_t index = coreParent->children(false).find(coreObject);
        g_signal_emit_by_name(atkParent, "children-changed::add", index != notFound ? index : -1, wrapper);
    }
    m_deferredAttachedWrapperObjectList.clear();

    for (auto& wrapper : m_deferredDetachedWrapperList) {
        if (auto* atkParent = wrapperParent(wrapper.get())) {
            // The accessibility hierarchy is already invalid, so the parent-children relationships
            // in the AccessibilityObject tree are not there anymore, so we can't know the offset.
            g_signal_emit_by_name(atkParent, "children-changed::remove", -1, wrapper.get());
        }
    }
    m_deferredDetachedWrapperList.clear();
}

static AXCoreObject* getListObject(AXCoreObject* object)
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
    const AccessibilityObject::AccessibilityChildrenVector& children = object->children();
    if (!children.size())
        return 0;

    AXCoreObject* listObject = children.at(0).get();
    if (!listObject->isMenuListPopup())
        return 0;

    return listObject;
}

static void notifyChildrenSelectionChange(AXCoreObject* object)
{
    // This static variables are needed to keep track of the old
    // focused object and its associated list object, as per previous
    // calls to this function, in order to properly decide whether to
    // emit some signals or not.
    static NeverDestroyed<RefPtr<AXCoreObject>> oldListObject;
    static NeverDestroyed<RefPtr<AXCoreObject>> oldFocusedObject;

    // Only list boxes and menu lists supported so far.
    if (!object || !(object->isListBox() || object->isMenuList()))
        return;

    // Only support HTML select elements so far (ARIA selectors not supported).
    Node* node = object->node();
    if (!is<HTMLSelectElement>(node))
        return;

    // Emit signal from the listbox's point of view first.
    g_signal_emit_by_name(object->wrapper(), "selection-changed");

    // Find the item where the selection change was triggered from.
    HTMLSelectElement& select = downcast<HTMLSelectElement>(*node);
    int changedItemIndex = select.activeSelectionStartListIndex();

    AXCoreObject* listObject = getListObject(object);
    if (!listObject) {
        oldListObject.get() = nullptr;
        return;
    }

    const AccessibilityObject::AccessibilityChildrenVector& items = listObject->children();
    if (changedItemIndex < 0 || changedItemIndex >= static_cast<int>(items.size()))
        return;
    AXCoreObject* item = items.at(changedItemIndex).get();

    // Ensure the current list object is the same than the old one so
    // further comparisons make sense. Otherwise, just reset
    // oldFocusedObject so it won't be taken into account.
    if (oldListObject.get() != listObject)
        oldFocusedObject.get() = nullptr;

    WebKitAccessible* axItem = item ? item->wrapper() : nullptr;
    WebKitAccessible* axOldFocusedObject = oldFocusedObject.get() ? oldFocusedObject.get()->wrapper() : nullptr;

    // Old focused object just lost focus, so emit the events.
    if (axOldFocusedObject && axItem != axOldFocusedObject) {
        g_signal_emit_by_name(axOldFocusedObject, "focus-event", false);
        atk_object_notify_state_change(ATK_OBJECT(axOldFocusedObject), ATK_STATE_FOCUSED, false);
    }

    // Emit needed events for the currently (un)selected item.
    if (axItem) {
        bool isSelected = item->isSelected();
        atk_object_notify_state_change(ATK_OBJECT(axItem), ATK_STATE_SELECTED, isSelected);
        // When the selection changes in a collapsed widget such as a combo box
        // whose child menu is not showing, that collapsed widget retains focus.
        if (!object->isCollapsed()) {
            g_signal_emit_by_name(axItem, "focus-event", isSelected);
            atk_object_notify_state_change(ATK_OBJECT(axItem), ATK_STATE_FOCUSED, isSelected);
        }
    }

    // Update pointers to the previously involved objects.
    oldListObject.get() = listObject;
    oldFocusedObject.get() = item;
}

void AXObjectCache::postPlatformNotification(AXCoreObject* coreObject, AXNotification notification)
{
    auto* axObject = ATK_OBJECT(coreObject->wrapper());
    if (!axObject)
        return;

    switch (notification) {
    case AXCheckedStateChanged:
        if (!coreObject->isCheckboxOrRadio() && !coreObject->isSwitch())
            return;
        atk_object_notify_state_change(axObject, ATK_STATE_CHECKED, coreObject->isChecked());
        break;

    case AXSelectedChildrenChanged:
    case AXMenuListValueChanged:
        // Accessible focus claims should not be made if the associated widget is not focused.
        if (notification == AXMenuListValueChanged && coreObject->isMenuList() && coreObject->isFocused()) {
            g_signal_emit_by_name(axObject, "focus-event", true);
            atk_object_notify_state_change(axObject, ATK_STATE_FOCUSED, true);
        }
        notifyChildrenSelectionChange(coreObject);
        break;

    case AXValueChanged:
        if (ATK_IS_VALUE(axObject)) {
            AtkPropertyValues propertyValues;
            propertyValues.property_name = "accessible-value";

            memset(&propertyValues.new_value,  0, sizeof(GValue));

            double value;
            atk_value_get_value_and_text(ATK_VALUE(axObject), &value, nullptr);
            g_value_set_double(g_value_init(&propertyValues.new_value, G_TYPE_DOUBLE), value);

            g_signal_emit_by_name(axObject, "property-change::accessible-value", &propertyValues, NULL);
        }
        break;

    case AXInvalidStatusChanged:
        atk_object_notify_state_change(axObject, ATK_STATE_INVALID_ENTRY, coreObject->invalidStatus() != "false");
        break;

    case AXElementBusyChanged:
        atk_object_notify_state_change(axObject, ATK_STATE_BUSY, coreObject->isBusy());
        break;

    case AXCurrentStateChanged:
        atk_object_notify_state_change(axObject, ATK_STATE_ACTIVE, coreObject->currentState() != AccessibilityCurrentState::False);
        break;

    case AXRowExpanded:
        atk_object_notify_state_change(axObject, ATK_STATE_EXPANDED, true);
        break;

    case AXRowCollapsed:
        atk_object_notify_state_change(axObject, ATK_STATE_EXPANDED, false);
        break;

    case AXExpandedChanged:
        atk_object_notify_state_change(axObject, ATK_STATE_EXPANDED, coreObject->isExpanded());
        break;

    case AXDisabledStateChanged: {
        bool enabledState = coreObject->isEnabled();
        atk_object_notify_state_change(axObject, ATK_STATE_ENABLED, enabledState);
        atk_object_notify_state_change(axObject, ATK_STATE_SENSITIVE, enabledState);
        break;
    }

    case AXPressedStateChanged:
        atk_object_notify_state_change(axObject, ATK_STATE_PRESSED, coreObject->isPressed());
        break;

    case AXReadOnlyStatusChanged:
        atk_object_notify_state_change(axObject, ATK_STATE_READ_ONLY, !coreObject->canSetValueAttribute());
        break;

    case AXRequiredStatusChanged:
        atk_object_notify_state_change(axObject, ATK_STATE_REQUIRED, coreObject->isRequired());
        break;

    case AXActiveDescendantChanged:
        if (AXCoreObject* descendant = coreObject->activeDescendant())
            platformHandleFocusedUIElementChanged(nullptr, descendant->node());
        break;

    default:
        break;
    }
}

void AXObjectCache::nodeTextChangePlatformNotification(AccessibilityObject* object, AXTextChange textChange, unsigned offset, const String& text)
{
    if (!object || text.isEmpty())
        return;

    AXCoreObject* parentObject = object->isNonNativeTextControl() ? object : object->parentObjectUnignored();
    if (!parentObject)
        return;

    auto* wrapper = parentObject->wrapper();
    if (!wrapper || !ATK_IS_TEXT(wrapper))
        return;

    Node* node = object->node();
    if (!node)
        return;

    // Ensure document's layout is up-to-date before using TextIterator.
    Document& document = node->document();
    document.updateLayout();

    // Select the right signal to be emitted
    CString detail;
    switch (textChange) {
    case AXTextInserted:
        detail = "text-insert";
        break;
    case AXTextDeleted:
        detail = "text-remove";
        break;
    case AXTextAttributesChanged:
        detail = "text-attributes-changed";
        break;
    }

    String textToEmit = text;
    unsigned offsetToEmit = offset;

    // If the object we're emitting the signal from represents a
    // password field, we will emit the masked text.
    if (parentObject->isPasswordField()) {
        String maskedText = parentObject->passwordFieldValue();
        textToEmit = maskedText.substring(offset, text.length());
    } else {
        // Consider previous text objects that might be present for
        // the current accessibility object to ensure we emit the
        // right offset (e.g. multiline text areas).
        offsetToEmit = offset + characterCount(SimpleRange { { *node->parentNode(), 0 }, { *node, 0 } });
    }

    g_signal_emit_by_name(wrapper, detail.data(), offsetToEmit, textToEmit.length(), textToEmit.utf8().data());
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* object, AXLoadingEvent loadingEvent)
{
    if (!object)
        return;

    auto* axObject = ATK_OBJECT(object->wrapper());
    if (!axObject || !ATK_IS_DOCUMENT(axObject))
        return;

    switch (loadingEvent) {
    case AXObjectCache::AXLoadingStarted:
        atk_object_notify_state_change(axObject, ATK_STATE_BUSY, true);
        break;
    case AXObjectCache::AXLoadingReloaded:
        atk_object_notify_state_change(axObject, ATK_STATE_BUSY, true);
        g_signal_emit_by_name(axObject, "reload");
        break;
    case AXObjectCache::AXLoadingFailed:
        g_signal_emit_by_name(axObject, "load-stopped");
        atk_object_notify_state_change(axObject, ATK_STATE_BUSY, false);
        break;
    case AXObjectCache::AXLoadingFinished:
        g_signal_emit_by_name(axObject, "load-complete");
        atk_object_notify_state_change(axObject, ATK_STATE_BUSY, false);
        break;
    }
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode)
{
    RefPtr<AccessibilityObject> oldObject = getOrCreate(oldFocusedNode);
    if (oldObject) {
        auto* axObject = oldObject->wrapper();
        g_signal_emit_by_name(axObject, "focus-event", false);
        atk_object_notify_state_change(ATK_OBJECT(axObject), ATK_STATE_FOCUSED, false);
    }
    RefPtr<AccessibilityObject> newObject = getOrCreate(newFocusedNode);
    if (newObject) {
        auto* axObject = newObject->wrapper();
        g_signal_emit_by_name(axObject, "focus-event", true);
        atk_object_notify_state_change(ATK_OBJECT(axObject), ATK_STATE_FOCUSED, true);
    }
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
}

} // namespace WebCore

#endif
