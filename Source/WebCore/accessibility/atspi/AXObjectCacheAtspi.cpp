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

#if ENABLE(ACCESSIBILITY) && USE(ATSPI)
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

void AXObjectCache::attachWrapper(AXCoreObject* axObject)
{
    auto wrapper = AccessibilityObjectAtspi::create(axObject);
    axObject->setWrapper(wrapper.ptr());

    auto* axParent = axObject->parentObjectUnignored();
    if (!axParent)
        return;

    auto* axParentWrapper = axParent->wrapper();
    if (!axParentWrapper)
        return;

    wrapper->setParent(axParentWrapper);
}

void AXObjectCache::platformPerformDeferredCacheUpdate()
{
}

bool AXObjectCache::isIsolatedTreeEnabled()
{
    return true;
}

void AXObjectCache::initializeSecondaryAXThread()
{
}

bool AXObjectCache::usedOnAXThread()
{
    return true;
}

void AXObjectCache::postPlatformNotification(AXCoreObject* coreObject, AXNotification notification)
{
    RELEASE_ASSERT(isMainThread());
    auto* wrapper = coreObject->wrapper();
    if (!wrapper)
        return;

    switch (notification) {
    case AXCheckedStateChanged:
        if (coreObject->isCheckboxOrRadio() || coreObject->isSwitch())
            wrapper->stateChanged("checked", coreObject->isChecked());
        break;
    case AXSelectedStateChanged:
        wrapper->stateChanged("selected", coreObject->isSelected());
        break;
    case AXSelectedChildrenChanged:
    case AXMenuListValueChanged:
        break;
    case AXValueChanged:
        break;
    case AXInvalidStatusChanged:
        wrapper->stateChanged("invalid-entry", coreObject->invalidStatus() != "false");
        break;
    case AXElementBusyChanged:
        wrapper->stateChanged("busy", coreObject->isBusy());
        break;
    case AXCurrentStateChanged:
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
    case AXAriaAttributeChanged:
        break;
    case AXAriaRoleChanged:
        break;
    case AXAutocorrectionOccured:
        break;
    case AXChildrenChanged:
        break;
    case AXFocusedUIElementChanged:
        break;
    case AXFrameLoadComplete:
        break;
    case AXIdAttributeChanged:
        break;
    case AXImageOverlayChanged:
        break;
    case AXLanguageChanged:
        break;
    case AXLayoutComplete:
        break;
    case AXLoadComplete:
        break;
    case AXNewDocumentLoadComplete:
        break;
    case AXPageScrolled:
        break;
    case AXSelectedTextChanged:
        break;
    case AXScrolledToAnchor:
        break;
    case AXLiveRegionCreated:
        break;
    case AXLiveRegionChanged:
        break;
    case AXMenuListItemSelected:
        break;
    case AXMenuClosed:
        break;
    case AXMenuOpened:
        break;
    case AXRowCountChanged:
        break;
    case AXPressDidSucceed:
        break;
    case AXPressDidFail:
        break;
    case AXSortDirectionChanged:
        break;
    case AXTextChanged:
        break;
    case AXDraggingStarted:
        break;
    case AXDraggingEnded:
        break;
    case AXDraggingEnteredDropZone:
        break;
    case AXDraggingDropped:
        break;
    case AXDraggingExitedDropZone:
        break;
    }
}

void AXObjectCache::postTextStateChangePlatformNotification(AXCoreObject* coreObject, const AXTextStateChangeIntent&, const VisibleSelection& selection)
{
    RELEASE_ASSERT(isMainThread());
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
    RELEASE_ASSERT(isMainThread());
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
    RELEASE_ASSERT(isMainThread());
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
    RELEASE_ASSERT(isMainThread());
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

void AXObjectCache::frameLoadingEventPlatformNotification(AccessibilityObject* object, AXLoadingEvent loadingEvent)
{
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

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
