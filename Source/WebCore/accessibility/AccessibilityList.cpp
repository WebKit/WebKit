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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "AccessibilityList.h"

#include "AXObjectCache.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "PseudoElement.h"
#include "RenderListItem.h"
#include "RenderObject.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityList::AccessibilityList(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityList::~AccessibilityList() = default;

Ref<AccessibilityList> AccessibilityList::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityList(renderer));
}

bool AccessibilityList::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}
    
bool AccessibilityList::isUnorderedList() const
{
    if (!m_renderer)
        return false;
    
    Node* node = m_renderer->node();

    // The ARIA spec says the "list" role is supposed to mimic a UL or OL tag.
    // Since it can't be both, it's probably OK to say that it's an un-ordered list.
    // On the Mac, there's no distinction to the client.
    if (ariaRoleAttribute() == AccessibilityRole::List)
        return true;
    
    return node && node->hasTagName(ulTag);
}

bool AccessibilityList::isOrderedList() const
{
    if (!m_renderer)
        return false;

    // ARIA says a directory is like a static table of contents, which sounds like an ordered list.
    if (ariaRoleAttribute() == AccessibilityRole::Directory)
        return true;

    Node* node = m_renderer->node();
    return node && node->hasTagName(olTag);    
}

bool AccessibilityList::isDescriptionList() const
{
    if (!m_renderer)
        return false;
    
    Node* node = m_renderer->node();
    return node && node->hasTagName(dlTag);
}

bool AccessibilityList::childHasPseudoVisibleListItemMarkers(RenderObject* listItem)
{
    // Check if the list item has a pseudo-element that should be accessible (e.g. an image or text)
    Element* listItemElement = downcast<Element>(listItem->node());
    if (!listItemElement || !listItemElement->beforePseudoElement())
        return false;

    AccessibilityObject* axObj = axObjectCache()->getOrCreate(listItemElement->beforePseudoElement()->renderer());
    if (!axObj)
        return false;
    
    if (!axObj->accessibilityIsIgnored())
        return true;
    
    for (const auto& child : axObj->children()) {
        if (!child->accessibilityIsIgnored())
            return true;
    }
    
    // Platforms which expose rendered text content through the parent element will treat
    // those renderers as "ignored" objects.
#if USE(ATK)
    String text = axObj->textUnderElement();
    return !text.isEmpty() && !text.isAllSpecialCharacters<isHTMLSpace>();
#else
    return false;
#endif
}
    
AccessibilityRole AccessibilityList::determineAccessibilityRole()
{
    m_ariaRole = determineAriaRoleAttribute();
    
    // Directory is mapped to list for now, but does not adhere to the same heuristics.
    if (ariaRoleAttribute() == AccessibilityRole::Directory)
        return AccessibilityRole::List;
    
    // Heuristic to determine if this list is being used for layout or for content.
    //   1. If it's a named list, like ol or aria=list, then it's a list.
    //      1a. Unless the list has no children, then it's not a list.
    //   2. If it displays visible list markers, it's a list.
    //   3. If it does not display list markers and has only one child, it's not a list.
    //   4. If it does not have any listitem children, it's not a list.
    //   5. Otherwise it's a list (for now).
    
    AccessibilityRole role = AccessibilityRole::List;
    
    // Temporarily set role so that we can query children (otherwise canHaveChildren returns false).
    m_role = role;
    
    unsigned listItemCount = 0;
    bool hasVisibleMarkers = false;

    const auto& children = this->children();
    // DescriptionLists are always semantically a description list, so do not apply heuristics.
    if (isDescriptionList() && children.size())
        return AccessibilityRole::DescriptionList;

    for (const auto& child : children) {
        if (child->ariaRoleAttribute() == AccessibilityRole::ListItem)
            listItemCount++;
        else if (child->roleValue() == AccessibilityRole::ListItem) {
            RenderObject* listItem = child->renderer();
            if (!listItem)
                continue;
            
            // Rendered list items always count.
            if (listItem->isListItem()) {
                if (!hasVisibleMarkers && (listItem->style().listStyleType() != ListStyleType::None || listItem->style().listStyleImage() || childHasPseudoVisibleListItemMarkers(listItem)))
                    hasVisibleMarkers = true;
                listItemCount++;
            } else if (listItem->node() && listItem->node()->hasTagName(liTag)) {
                // Inline elements that are in a list with an explicit role should also count.
                if (m_ariaRole == AccessibilityRole::List)
                    listItemCount++;

                if (childHasPseudoVisibleListItemMarkers(listItem)) {
                    hasVisibleMarkers = true;
                    listItemCount++;
                }
            }
        }
    }
    
    // Non <ul> lists and ARIA lists only need to have one child.
    // <ul>, <ol> lists need to have visible markers.
    if (ariaRoleAttribute() != AccessibilityRole::Unknown) {
        if (!listItemCount)
            role = AccessibilityRole::ApplicationGroup;
    } else if (!hasVisibleMarkers)
        role = AccessibilityRole::Group;

    return role;
}
    
AccessibilityRole AccessibilityList::roleValue() const
{
    ASSERT(m_role != AccessibilityRole::Unknown);
    return m_role;
}
    
} // namespace WebCore
