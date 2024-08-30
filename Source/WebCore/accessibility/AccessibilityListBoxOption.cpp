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
#include "AccessibilityListBoxOption.h"

#include "AXObjectCache.h"
#include "AccessibilityListBox.h"
#include "ElementInlines.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "IntRect.h"
#include "RenderListBox.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityListBoxOption::AccessibilityListBoxOption(HTMLElement& element)
    : AccessibilityNodeObject(&element)
{
}

AccessibilityListBoxOption::~AccessibilityListBoxOption() = default;

Ref<AccessibilityListBoxOption> AccessibilityListBoxOption::create(HTMLElement& element)
{
    return adoptRef(*new AccessibilityListBoxOption(element));
}

bool AccessibilityListBoxOption::isEnabled() const
{
    return !(is<HTMLOptGroupElement>(m_node.get())
        || equalLettersIgnoringASCIICase(getAttribute(aria_disabledAttr), "true"_s)
        || hasAttribute(disabledAttr));
}

bool AccessibilityListBoxOption::isSelected() const
{
    RefPtr option = dynamicDowncast<HTMLOptionElement>(m_node.get());
    return option && option->selected();
}

bool AccessibilityListBoxOption::isSelectedOptionActive() const
{
    HTMLSelectElement* listBoxParentNode = listBoxOptionParentNode();
    if (!listBoxParentNode)
        return false;

    return listBoxParentNode->activeSelectionEndListIndex() == listBoxOptionIndex();
}

LayoutRect AccessibilityListBoxOption::elementRect() const
{
    if (!m_node)
        return { };

    RefPtr listBoxParentNode = listBoxOptionParentNode();
    if (!listBoxParentNode)
        return { };

    auto* listBoxRenderer = dynamicDowncast<RenderListBox>(listBoxParentNode->renderer());
    if (!listBoxRenderer)
        return { };

    WeakPtr cache = listBoxRenderer->document().axObjectCache();
    RefPtr listbox = cache ? cache->getOrCreate(*listBoxRenderer) : nullptr;
    if (!listbox)
        return { };

    auto parentRect = listbox->boundingBoxRect();
    int index = listBoxOptionIndex();
    if (index != -1)
        return listBoxRenderer->itemBoundingBoxRect(parentRect.location(), index);
    return { };
}

bool AccessibilityListBoxOption::computeIsIgnored() const
{
    if (!m_node || isIgnoredByDefault())
        return true;

    auto* parent = parentObject();
    return parent ? parent->isIgnored() : true;
}

bool AccessibilityListBoxOption::canSetSelectedAttribute() const
{
    RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(m_node.get());
    if (!optionElement)
        return false;

    if (optionElement->isDisabledFormControl())
        return false;

    RefPtr selectElement = listBoxOptionParentNode();
    return !selectElement || !selectElement->isDisabledFormControl();
}

String AccessibilityListBoxOption::stringValue() const
{
    if (!m_node)
        return { };

    auto ariaLabel = getAttributeTrimmed(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*m_node))
        return option->label();

    if (RefPtr optgroup = dynamicDowncast<HTMLOptGroupElement>(*m_node))
        return optgroup->groupLabelText();

    return { };
}

Element* AccessibilityListBoxOption::actionElement() const
{
    ASSERT(is<HTMLElement>(m_node.get()));
    return dynamicDowncast<Element>(m_node.get());
}

Node* AccessibilityListBoxOption::node() const
{
    return m_node.get();
}

AccessibilityObject* AccessibilityListBoxOption::parentObject() const
{
    auto* parentNode = listBoxOptionParentNode();
    if (!parentNode)
        return nullptr;

    auto* cache = m_node->document().axObjectCache();
    return cache ? cache->getOrCreate(*parentNode) : nullptr;
}

void AccessibilityListBoxOption::setSelected(bool selected)
{
    HTMLSelectElement* selectElement = listBoxOptionParentNode();
    if (!selectElement)
        return;
    
    if (!canSetSelectedAttribute())
        return;
    
    bool isOptionSelected = isSelected();
    if ((isOptionSelected && selected) || (!isOptionSelected && !selected))
        return;
    
    // Convert from the entire list index to the option index.
    int optionIndex = selectElement->listToOptionIndex(listBoxOptionIndex());
    selectElement->accessKeySetSelectedIndex(optionIndex);
}

HTMLSelectElement* AccessibilityListBoxOption::listBoxOptionParentNode() const
{
    if (!m_node)
        return nullptr;

    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*m_node))
        return option->ownerSelectElement();

    if (RefPtr optgroup = dynamicDowncast<HTMLOptGroupElement>(*m_node))
        return optgroup->ownerSelectElement();

    return nullptr;
}

int AccessibilityListBoxOption::listBoxOptionIndex() const
{
    if (!m_node)
        return -1;

    auto* selectElement = listBoxOptionParentNode();
    if (!selectElement)
        return -1;

    const auto& listItems = selectElement->listItems();
    unsigned length = listItems.size();
    for (unsigned i = 0; i < length; i++) {
        if (listItems[i] == m_node)
            return i;
    }

    return -1;
}

} // namespace WebCore
