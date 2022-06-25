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
#include "AccessibilityListBox.h"

#include "AXObjectCache.h"
#include "AccessibilityListBoxOption.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "HitTestResult.h"
#include "RenderListBox.h"
#include "RenderObject.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityListBox::AccessibilityListBox(RenderObject* renderer)
    : AccessibilityRenderObject(renderer)
{
}

AccessibilityListBox::~AccessibilityListBox() = default;

Ref<AccessibilityListBox> AccessibilityListBox::create(RenderObject* renderer)
{
    return adoptRef(*new AccessibilityListBox(renderer));
}

bool AccessibilityListBox::canSetSelectedChildren() const
{
    Node* selectNode = m_renderer->node();
    if (!selectNode)
        return false;
    
    return !downcast<HTMLSelectElement>(*selectNode).isDisabledFormControl();
}

void AccessibilityListBox::addChildren()
{
    if (!m_renderer)
        return;

    Node* selectNode = m_renderer->node();
    if (!selectNode)
        return;

    m_childrenInitialized = true;

    for (const auto& listItem : downcast<HTMLSelectElement>(*selectNode).listItems())
        addChild(listBoxOptionAccessibilityObject(listItem), DescendIfIgnored::No);
}

void AccessibilityListBox::setSelectedChildren(const AccessibilityChildrenVector& children)
{
    if (!canSetSelectedChildren())
        return;

    Node* selectNode = m_renderer->node();
    if (!selectNode)
        return;
    
    // disable any selected options
    for (const auto& child : m_children) {
        auto& listBoxOption = downcast<AccessibilityListBoxOption>(*child);
        if (listBoxOption.isSelected())
            listBoxOption.setSelected(false);
    }
    
    for (const auto& obj : children) {
        if (obj->roleValue() != AccessibilityRole::ListBoxOption)
            continue;

        downcast<AccessibilityListBoxOption>(*obj).setSelected(true);
    }
}
    
void AccessibilityListBox::selectedChildren(AccessibilityChildrenVector& result)
{
    ASSERT(result.isEmpty());

    if (!childrenInitialized())
        addChildren();

    for (const auto& child : m_children) {
        if (downcast<AccessibilityListBoxOption>(*child).isSelected())
            result.append(child.get());
    }    
}

void AccessibilityListBox::visibleChildren(AccessibilityChildrenVector& result)
{
    ASSERT(result.isEmpty());
    
    if (!childrenInitialized())
        addChildren();
    
    unsigned length = m_children.size();
    for (unsigned i = 0; i < length; i++) {
        if (downcast<RenderListBox>(*m_renderer).listIndexIsVisible(i))
            result.append(m_children[i]);
    }
}

AccessibilityObject* AccessibilityListBox::listBoxOptionAccessibilityObject(HTMLElement* element) const
{
    // FIXME: Why does AccessibilityMenuListPopup::menuListOptionAccessibilityObject check inRenderedDocument, but this does not?
    return m_renderer->document().axObjectCache()->getOrCreate(element);
}

AXCoreObject* AccessibilityListBox::elementAccessibilityHitTest(const IntPoint& point) const
{
    // the internal HTMLSelectElement methods for returning a listbox option at a point
    // ignore optgroup elements.
    if (!m_renderer)
        return nullptr;
    
    Node* node = m_renderer->node();
    if (!node)
        return nullptr;
    
    LayoutRect parentRect = boundingBoxRect();
    
    AXCoreObject* listBoxOption = nullptr;
    unsigned length = m_children.size();
    for (unsigned i = 0; i < length; ++i) {
        LayoutRect rect = downcast<RenderListBox>(*m_renderer).itemBoundingBoxRect(parentRect.location(), i);
        // The cast to HTMLElement below is safe because the only other possible listItem type
        // would be a WMLElement, but WML builds don't use accessibility features at all.
        if (rect.contains(point)) {
            listBoxOption = m_children[i].get();
            break;
        }
    }
    
    if (listBoxOption && !listBoxOption->accessibilityIsIgnored())
        return listBoxOption;
    
    return axObjectCache()->getOrCreate(renderer());
}

} // namespace WebCore
