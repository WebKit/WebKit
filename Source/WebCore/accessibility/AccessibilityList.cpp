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
#include "AccessibilityList.h"

#include "AXObjectCache.h"
#include "HTMLNames.h"
#include "RenderListItem.h"
#include "RenderObject.h"
#include "RenderStyle.h"

namespace WebCore {
    
using namespace HTMLNames;

AccessibilityList::AccessibilityList(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityList::~AccessibilityList()
{
}

PassRefPtr<AccessibilityList> AccessibilityList::create(RenderObject* renderer)
{
    return adoptRef(new AccessibilityList(renderer));
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
    if (ariaRoleAttribute() == ListRole)
        return true;
    
    return node && node->hasTagName(ulTag);
}

bool AccessibilityList::isOrderedList() const
{
    if (!m_renderer)
        return false;

    // ARIA says a directory is like a static table of contents, which sounds like an ordered list.
    if (ariaRoleAttribute() == DirectoryRole)
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

AccessibilityRole AccessibilityList::determineAccessibilityRole()
{
    m_ariaRole = determineAriaRoleAttribute();
    
    // Directory is mapped to list for now, but does not adhere to the same heuristics.
    if (ariaRoleAttribute() == DirectoryRole)
        return ListRole;
    
    // Heuristic to determine if this list is being used for layout or for content.
    //   1. If it's a named list, like ol or aria=list, then it's a list.
    //      1a. Unless the list has no children, then it's not a list.
    //   2. If it displays visible list markers, it's a list.
    //   3. If it does not display list markers and has only one child, it's not a list.
    //   4. If it does not have any listitem children, it's not a list.
    //   5. Otherwise it's a list (for now).
    
    AccessibilityRole role = ListRole;
    
    // Temporarily set role so that we can query children (otherwise canHaveChildren returns false).
    m_role = role;
    
    unsigned listItemCount = 0;
    bool hasVisibleMarkers = false;

    const auto& children = this->children();
    // DescriptionLists are always semantically a description list, so do not apply heuristics.
    if (isDescriptionList() && children.size())
        return DescriptionListRole;

    for (const auto& child : children) {
        if (child->ariaRoleAttribute() == ListItemRole)
            listItemCount++;
        else if (child->roleValue() == ListItemRole) {
            RenderObject* listItem = child->renderer();
            if (listItem && listItem->isListItem()) {
                if (listItem->style().listStyleType() != NoneListStyle || listItem->style().listStyleImage())
                    hasVisibleMarkers = true;
                listItemCount++;
            }
        }
    }
    
    bool unorderedList = isUnorderedList();
    // Non <ul> lists and ARIA lists only need to have one child.
    // <ul> lists need to have 1 child, or visible markers.
    if (!unorderedList || ariaRoleAttribute() != UnknownRole) {
        if (!listItemCount)
            role = GroupRole;
    } else if (unorderedList && listItemCount <= 1 && !hasVisibleMarkers)
        role = GroupRole;

    return role;
}
    
AccessibilityRole AccessibilityList::roleValue() const
{
    ASSERT(m_role != UnknownRole);
    return m_role;
}
    
} // namespace WebCore
