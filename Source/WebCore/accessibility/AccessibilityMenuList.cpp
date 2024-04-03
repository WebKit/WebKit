/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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

#include "AXObjectCache.h"
#include "AccessibilityMenuListPopup.h"
#include "RenderMenuList.h"
#include <wtf/Scope.h>

namespace WebCore {

AccessibilityMenuList::AccessibilityMenuList(RenderMenuList* renderer)
    : AccessibilityRenderObject(renderer)
{
}

Ref<AccessibilityMenuList> AccessibilityMenuList::create(RenderMenuList* renderer)
{
    return adoptRef(*new AccessibilityMenuList(renderer));
}

bool AccessibilityMenuList::press()
{
    if (!m_renderer)
        return false;

#if !PLATFORM(IOS_FAMILY)
    auto element = this->element();
    AXObjectCache::AXNotification notification = AXObjectCache::AXPressDidFail;
    if (CheckedPtr menuList = dynamicDowncast<RenderMenuList>(renderer()); menuList && element && !element->isDisabledFormControl()) {
        if (menuList->popupIsVisible())
            menuList->hidePopup();
        else
            menuList->showPopup();
        notification = AXObjectCache::AXPressDidSucceed;
    }
    if (auto cache = axObjectCache())
        cache->postNotification(element, notification);
    return true;
#endif
    return false;
}

void AccessibilityMenuList::addChildren()
{
    auto clearDirtySubtree = makeScopeExit([&] {
        m_subtreeDirty = false;
    });

    if (!m_renderer)
        return;
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return;
    
    auto list = cache->create(AccessibilityRole::MenuListPopup);
    if (!list)
        return;

    downcast<AccessibilityMockObject>(*list).setParent(this);
    if (list->accessibilityIsIgnored()) {
        cache->remove(list->objectID());
        return;
    }

    m_childrenInitialized = true;
    addChild(list);
    list->addChildren();
}

bool AccessibilityMenuList::isCollapsed() const
{
    // Collapsed is the "default" state, so if the renderer doesn't exist
    // this makes slightly more sense than returning false.
    if (!m_renderer)
        return true;

#if !PLATFORM(IOS_FAMILY)
    CheckedPtr menuList = dynamicDowncast<RenderMenuList>(renderer());
    return !(menuList && menuList->popupIsVisible());
#else
    return true;
#endif
}

bool AccessibilityMenuList::canSetFocusAttribute() const
{
    if (!node())
        return false;

    return !downcast<Element>(*node()).isDisabledFormControl();
}

void AccessibilityMenuList::didUpdateActiveOption(int optionIndex)
{
    Ref<Document> document(m_renderer->document());

    const auto& childObjects = children();
    if (!childObjects.isEmpty()) {
        ASSERT(childObjects.size() == 1);
        ASSERT(is<AccessibilityMenuListPopup>(*childObjects[0]));

        // We might be calling this method in situations where the renderers for list items
        // associated to the menu list have not been created (e.g. they might be rendered
        // in the UI process, as it's the case in the GTK+ port, which uses GtkMenuItem).
        // So, we need to make sure that the accessibility popup object has some children
        // before asking it to update its active option, or it will read invalid memory.
        // You can reproduce the issue in the GTK+ port by removing this check and running
        // accessibility/insert-selected-option-into-select-causes-crash.html (will crash).
        int popupChildrenSize = static_cast<int>(childObjects[0]->children().size());
        if (auto* accessibilityMenuListPopup = dynamicDowncast<AccessibilityMenuListPopup>(*childObjects[0]); accessibilityMenuListPopup && optionIndex >= 0 && optionIndex < popupChildrenSize)
            accessibilityMenuListPopup->didUpdateActiveOption(optionIndex);
    }

    if (auto* cache = document->axObjectCache())
        cache->deferMenuListValueChange(element());
}

} // namespace WebCore
