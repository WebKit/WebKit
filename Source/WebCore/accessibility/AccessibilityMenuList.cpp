/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "AccessibilityMenuList.h"

#include "AXLoggerBase.h"
#include "AXNotifications.h"
#include "AccessibilityObjectInlines.h"
#include "AXObjectCache.h"
#include "AccessibilityMenuListPopup.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLSelectElement.h"
#include "RenderMenuList.h"
#include "RenderObjectDocument.h"
#include <wtf/Scope.h>

namespace WebCore {

AccessibilityMenuList::AccessibilityMenuList(AXID axID, RenderMenuList& renderer, AXObjectCache& cache)
    : AccessibilityRenderObject(axID, renderer, cache)
    , m_popup(downcast<AccessibilityMenuListPopup>(*cache.create(AccessibilityRole::MenuListPopup)))
{
}

Ref<AccessibilityMenuList> AccessibilityMenuList::create(AXID axID, RenderMenuList& renderer, AXObjectCache& cache)
{
    Ref menuList = adoptRef(*new AccessibilityMenuList(axID, renderer, cache));
    // We have to do this setup here and not in the constructor to avoid an
    // adoptionIsRequired ASSERT in RefCounted.h.
    menuList->m_popup->setParent(menuList.ptr());
    menuList->addChild(menuList->m_popup.get());
    menuList->m_childrenInitialized = true;
    return menuList;
}

bool AccessibilityMenuList::press()
{
    if (!m_renderer)
        return false;

#if !PLATFORM(IOS_FAMILY)
    RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(element());
    auto notification = AXNotification::PressDidFail;
    if (selectElement && !selectElement->isDisabledFormControl()) {
        if (selectElement->popupIsVisible())
            selectElement->hidePopup();
        else
            selectElement->showPopup();
        notification = AXNotification::PressDidSucceed;
    }
    if (CheckedPtr cache = axObjectCache())
        cache->postNotification(selectElement.get(), notification);
    return true;
#endif
    return false;
}

void AccessibilityMenuList::updateChildrenIfNecessary()
{
    // Typically for AccessibilityNodeObject subclasses, updateChildrenIfNecessary() is what
    // calls addChildren(), which in turn passes m_subtreeDirty down the tree as objects are inserted.
    // However, we purposely never allow our children to be cleared or become unitialized, which
    // by the definition of the AccessibilityNodeObject::updateChildrenIfNecessary() means addChildren()
    // will never be called. (We add our only child, m_popup, once in the constructor).
    //
    // Despite this, we still want to pass down the m_subtreeDirty flag if we have it set, so do that here.
    if (m_subtreeDirty)
        m_popup->setNeedsToUpdateSubtree();

    m_subtreeDirty = false;
}

void AccessibilityMenuList::addChildren()
{
    // This class sets its children once in the create function, and should never
    // have dirty or uninitialized children afterwards.
    AX_ASSERT(m_childrenInitialized);
    AX_ASSERT(!m_childrenDirty);
}

bool AccessibilityMenuList::isCollapsed() const
{
    // Collapsed is the "default" state, so if the renderer doesn't exist
    // this makes slightly more sense than returning false.
    if (!m_renderer)
        return true;

#if !PLATFORM(IOS_FAMILY)
    RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(element());
    return !(selectElement && selectElement->usesMenuList() && selectElement->popupIsVisible());
#else
    return true;
#endif
}

bool AccessibilityMenuList::canSetFocusAttribute() const
{
    RefPtr element = this->element();
    return element && !element->isDisabledFormControl();
}

void AccessibilityMenuList::didUpdateActiveOption(int optionIndex)
{
    RefPtr document = m_renderer ? &m_renderer->document() : nullptr;
    if (!document)
        return;

    const auto& childObjects = unignoredChildren();
    if (!childObjects.isEmpty()) {
        AX_ASSERT(childObjects.size() == 1);
        AX_ASSERT(is<AccessibilityMenuListPopup>(childObjects[0].get()));

        // We might be calling this method in situations where the renderers for list items
        // associated to the menu list have not been created (e.g. they might be rendered
        // in the UI process, as it's the case in the GTK+ port, which uses GtkMenuItem).
        // So, we need to make sure that the accessibility popup object has some children
        // before asking it to update its active option, or it will read invalid memory.
        // You can reproduce the issue in the GTK+ port by removing this check and running
        // accessibility/insert-selected-option-into-select-causes-crash.html (will crash).
        int popupChildrenSize = static_cast<int>(childObjects[0]->unignoredChildren().size());
        RefPtr accessibilityMenuListPopup = dynamicDowncast<AccessibilityMenuListPopup>(childObjects[0].get());
        if (accessibilityMenuListPopup && optionIndex >= 0 && optionIndex < popupChildrenSize)
            accessibilityMenuListPopup->didUpdateActiveOption(optionIndex);
    }

    if (CheckedPtr cache = document->axObjectCache())
        cache->deferMenuListValueChange(element());
}

} // namespace WebCore
