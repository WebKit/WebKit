/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "AccessibilityTree.h"

#include "AXObjectCache.h"
#include "AccessibilityTreeItem.h"
#include "Element.h"
#include "HTMLNames.h"

#include <wtf/Deque.h>

namespace WebCore {

using namespace HTMLNames;
    
AccessibilityTree::AccessibilityTree(RenderObject& renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityTree::AccessibilityTree(Node& node)
    : AccessibilityRenderObject(node)
{
}

AccessibilityTree::~AccessibilityTree() = default;
    
Ref<AccessibilityTree> AccessibilityTree::create(RenderObject& renderer)
{
    return adoptRef(*new AccessibilityTree(renderer));
}

Ref<AccessibilityTree> AccessibilityTree::create(Node& node)
{
    return adoptRef(*new AccessibilityTree(node));
}

bool AccessibilityTree::computeIsIgnored() const
{
    return isIgnoredByDefault();
}

AccessibilityRole AccessibilityTree::determineAccessibilityRole()
{
    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Tree)
        return AccessibilityRenderObject::determineAccessibilityRole();

    return isTreeValid() ? AccessibilityRole::Tree : AccessibilityRole::Generic;
}

bool AccessibilityTree::isTreeValid() const
{
    // A valid tree can only have treeitem or group of treeitems as a child.
    // https://www.w3.org/TR/wai-aria/#tree
    Node* node = this->node();
    if (!node)
        return false;
    
    Deque<Ref<Node>> queue;
    for (RefPtr child = node->firstChild(); child; child = queue.last()->nextSibling())
        queue.append(child.releaseNonNull());

    while (!queue.isEmpty()) {
        Ref child = queue.takeFirst();

        if (!is<Element>(child.get()))
            continue;
        if (nodeHasRole(child.ptr(), "treeitem"_s))
            continue;
        if (!nodeHasRole(child.ptr(), "group"_s) && !nodeHasRole(child.ptr(), "presentation"_s))
            return false;

        for (RefPtr groupChild = child->firstChild(); groupChild; groupChild = queue.last()->nextSibling())
            queue.append(groupChild.releaseNonNull());
    }
    return true;
}

} // namespace WebCore
