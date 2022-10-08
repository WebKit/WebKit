/*
 * Copyright (C) 2021 Igalia S.L.
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

#if USE(ATSPI)
#include "AXTextStateChangeIntent.h"
#include "AccessibilityObject.h"
#include "AccessibilityObjectAtspi.h"
#include "AccessibilityRenderObject.h"
#include "Document.h"
#include "Element.h"
#include "HTMLSelectElement.h"
#include "Range.h"
#include "TextIterator.h"

namespace WebCore {

void AXObjectCache::attachWrapper(AccessibilityObject* axObject)
{
    auto wrapper = AccessibilityObjectAtspi::create(axObject, document().page()->accessibilityRootObject());
    axObject->setWrapper(wrapper.ptr());

    m_deferredParentChangedList.add(axObject);
    m_performCacheUpdateTimer.startOneShot(0_s);
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
    auto handleParentChanged = [&](const AXCoreObject& axObject) {
        auto* wrapper = axObject.wrapper();
        if (!wrapper)
            return;

        auto* axParent = axObject.parentObjectUnignored();
        if (!axParent) {
            if (axObject.isScrollView() && axObject.scrollView() == document().view())
                wrapper->setParent(nullptr); // nullptr means root.
            return;
        }

        if (auto* axParentWrapper = axParent->wrapper())
            wrapper->setParent(axParentWrapper);
    };

    for (const auto& axObject : m_deferredParentChangedList)
        handleParentChanged(*axObject);
    m_deferredParentChangedList.clear();
}

void AXObjectCache::postPlatformNotification(AXCoreObject* coreObject, AXNotification notification)
{
    auto* wrapper = coreObject->wrapper();
    if (!wrapper)
        return;

    switch (notification) {
    case AXCheckedStateChanged:
        if (coreObject->isCheckboxOrRadio() || coreObject->isSwitch())
            wrapper->stateChanged("checked", coreObject->isChecked());
        break;
    case AXSelectedCellChanged:
    case AXSelectedStateChanged:
        wrapper->stateChanged("selected", coreObject->isSelected());
        break;
    case AXMenuListItemSelected: {
        // Menu list popup items are handled by AXSelectedStateChanged.
        auto* parent = coreObject->parentObjectUnignored();
        if (parent && !parent->isMenuListPopup())
            wrapper->stateChanged("selected", coreObject->isSelected());
        break;
    }
    case AXSelectedChildrenChanged:
        wrapper->selectionChanged();
        break;
    case AXMenuListValueChanged: {
        const auto& children = coreObject->children();
        if (children.size() == 1) {
            if (auto* childWrapper = children[0]->wrapper())
                childWrapper->selectionChanged();
        }
        break;
    }
    case AXValueChanged:
        if (wrapper->interfaces().contains(AccessibilityObjectAtspi::Interface::Value))
            wrapper->valueChanged(coreObject->valueForRange());
        break;
    case AXInvalidStatusChanged:
        wrapper->stateChanged("invalid-entry", coreObject->invalidStatus() != "false"_s);
        break;
    case AXElementBusyChanged:
        wrapper->stateChanged("busy", coreObject->isBusy());
        break;
    case AXCurrentStateChanged:
        wrapper->stateChanged("active", coreObject->currentState() != AccessibilityCurrentState::False);
        break;
    case AXRowExpanded:
        wrapper->stateChanged("expanded", true);
        break;
    case AXRowCollapsed:
        wrapper->stateChanged("expanded", false);
        break;
    case AXExpandedChanged:
        wrapper->stateChanged("expanded", coreObject->isExpanded());
        break;
    case AXDisabledStateChanged: {
        bool enabledState = coreObject->isEnabled();
        wrapper->stateChanged("enabled", enabledState);
        wrapper->stateChanged("sensitive", enabledState);
        break;
    }
    case AXPressedStateChanged:
        wrapper->stateChanged("pressed", coreObject->isPressed());
        break;
    case AXReadOnlyStatusChanged:
        wrapper->stateChanged("read-only", coreObject->canSetValueAttribute());
        break;
    case AXRequiredStatusChanged:
        wrapper->stateChanged("required", coreObject->isRequired());
        break;
    case AXActiveDescendantChanged:
        if (auto* descendant = coreObject->activeDescendant())
            platformHandleFocusedUIElementChanged(nullptr, descendant->node());
        break;
    case AXChildrenChanged:
        coreObject->updateChildrenIfNecessary();
        break;
    default:
        break;
    }
}

void AXObjectCache::postTextStateChangePlatformNotification(AXCoreObject* coreObject, const AXTextStateChangeIntent&, const VisibleSelection& selection)
{
    if (!coreObject)
        coreObject = rootWebArea();

    if (!coreObject)
        return;

    auto* wrapper = coreObject->wrapper();
    if (!wrapper)
        return;

    wrapper->selectionChanged(selection);
}

void AXObjectCache::postTextStateChangePlatformNotification(AccessibilityObject* coreObject, AXTextEditType editType, const String& text, const VisiblePosition& position)
{
    if (text.isEmpty())
        return;

    auto* wrapper = coreObject->wrapper();
    if (!wrapper)
        return;

    switch (editType) {
    case AXTextEditTypeDelete:
    case AXTextEditTypeCut:
        wrapper->textDeleted(text, position);
        break;
    case AXTextEditTypeInsert:
    case AXTextEditTypeTyping:
    case AXTextEditTypeDictation:
    case AXTextEditTypePaste:
        wrapper->textInserted(text, position);
        break;
    case AXTextEditTypeAttributesChange:
        wrapper->textAttributesChanged();
        break;
    case AXTextEditTypeUnknown:
        break;
    }
}

void AXObjectCache::postTextReplacementPlatformNotificationForTextControl(AXCoreObject* coreObject, const String& deletedText, const String& insertedText, HTMLTextFormControlElement&)
{
    if (!coreObject)
        coreObject = rootWebArea();

    if (!coreObject)
        return;

    if (deletedText.isEmpty() && insertedText.isEmpty())
        return;

    auto* wrapper = coreObject->wrapper();
    if (!wrapper)
        return;

    if (!deletedText.isEmpty())
        wrapper->textDeleted(deletedText, coreObject->visiblePositionForIndex(0));
    if (!insertedText.isEmpty())
        wrapper->textInserted(insertedText, coreObject->visiblePositionForIndex(insertedText.length()));
}

void AXObjectCache::postTextReplacementPlatformNotification(AXCoreObject* coreObject, AXTextEditType, const String& deletedText, AXTextEditType, const String& insertedText, const VisiblePosition& position)
{
    if (!coreObject)
        coreObject = rootWebArea();

    if (!coreObject)
        return;

    if (deletedText.isEmpty() && insertedText.isEmpty())
        return;

    auto* wrapper = coreObject->wrapper();
    if (!wrapper)
        return;

    if (!deletedText.isEmpty())
        wrapper->textDeleted(deletedText, position);
    if (!insertedText.isEmpty())
        wrapper->textInserted(insertedText, position);
}

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* coreObject, AXLoadingEvent loadingEvent)
{
    if (!coreObject)
        return;

    if (coreObject->roleValue() != AccessibilityRole::WebArea)
        return;

    auto* wrapper = coreObject->wrapper();
    if (!wrapper)
        return;

    switch (loadingEvent) {
    case AXObjectCache::AXLoadingStarted:
        wrapper->stateChanged("busy", true);
        break;
    case AXObjectCache::AXLoadingReloaded:
        wrapper->stateChanged("busy", true);
        wrapper->loadEvent("Reload");
        break;
    case AXObjectCache::AXLoadingFailed:
        wrapper->stateChanged("busy", false);
        wrapper->loadEvent("LoadStopped");
        break;
    case AXObjectCache::AXLoadingFinished:
        wrapper->stateChanged("busy", false);
        wrapper->loadEvent("LoadComplete");
        break;
    }
}

void AXObjectCache::platformHandleFocusedUIElementChanged(Node* oldFocusedNode, Node* newFocusedNode)
{
    if (auto* axObject = get(oldFocusedNode)) {
        if (auto* wrapper = axObject->wrapper())
            wrapper->stateChanged("focused", false);
    }
    if (auto* axObject = getOrCreate(newFocusedNode)) {
        if (auto* wrapper = axObject->wrapper())
            wrapper->stateChanged("focused", true);
    }
}

void AXObjectCache::handleScrolledToAnchor(const Node*)
{
}

} // namespace WebCore

#endif // USE(ATSPI)
