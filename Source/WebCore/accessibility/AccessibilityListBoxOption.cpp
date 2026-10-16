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

#include "AXLoggerBase.h"
#include "AXObjectCacheInlines.h"
#include "AccessibilityNodeObjectInlines.h"
#include "AccessibilityObjectInlines.h"
#include "ContainerNodeInlines.h"
#include "ElementInlines.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "IntRect.h"
#include "RenderListBox.h"
#include "RenderObjectInlines.h"

namespace WebCore {

using namespace HTMLNames;

// This needs to match how AccessibilityRenderObject and AccessibilityNodeObject add the children of a select.
static bool listItemsAreChildrenOfSelect(const HTMLSelectElement& select)
{
    if (CheckedPtr renderer = select.renderer())
        return is<RenderListBox>(*renderer);
    return !select.isDropdownBox();
}

AccessibilityListBoxOption::AccessibilityListBoxOption(AXID axID, RenderObject& renderer, AXObjectCache& cache)
    : AccessibilityRenderObject(axID, renderer, cache)
{
}

AccessibilityListBoxOption::AccessibilityListBoxOption(AXID axID, HTMLElement& element, AXObjectCache& cache)
    : AccessibilityRenderObject(axID, static_cast<Node&>(element), cache)
{
}

AccessibilityListBoxOption::~AccessibilityListBoxOption() = default;

AccessibilityRole AccessibilityListBoxOption::determineAccessibilityRole()
{
    // Base-appearance select options live inside a popover and behave as menu
    // items (activate to select and close), not listbox options (toggle selection
    // in an always-visible list).
    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(m_node)) {
        if (RefPtr select = option->ownerSelectElement(); select && select->usesBaseAppearancePicker())
            return AccessibilityRole::MenuItem;
    }
    return AccessibilityRole::ListBoxOption;
}

Ref<AccessibilityListBoxOption> AccessibilityListBoxOption::create(AXID axID, HTMLElement& element, AXObjectCache& cache)
{
    if (CheckedPtr renderer = element.renderer())
        return adoptRef(*new AccessibilityListBoxOption(axID, *renderer, cache));
    return adoptRef(*new AccessibilityListBoxOption(axID, element, cache));
}

bool AccessibilityListBoxOption::isEnabled() const
{
    if (is<HTMLOptGroupElement>(m_node) || equalLettersIgnoringASCIICase(getAttribute(aria_disabledAttr), "true"_s))
        return false;

    RefPtr element = dynamicDowncast<Element>(m_node);
    return !element || !element->isDisabledFormControl();
}

bool AccessibilityListBoxOption::isSelected() const
{
    RefPtr option = dynamicDowncast<HTMLOptionElement>(m_node);
    return option && option->selected();
}

bool AccessibilityListBoxOption::isSelectedOptionActive() const
{
    RefPtr listBoxParentNode = listBoxOptionParentNode();
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

    CheckedPtr listBoxRenderer = dynamicDowncast<RenderListBox>(listBoxParentNode->renderer());
    if (!listBoxRenderer)
        return AccessibilityRenderObject::elementRect();

    CheckedPtr cache = protect(listBoxRenderer->document())->axObjectCache();
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

    // An optgroup that renders its own content is represented by its label instead.
    if (canHaveChildren())
        return true;

    RefPtr select = listBoxOptionParentNode();
    CheckedPtr cache = axObjectCache();
    RefPtr selectObject = select && cache ? cache->getOrCreate(*select) : nullptr;
    return !selectObject || selectObject->isIgnored();
}

bool AccessibilityListBoxOption::canHaveChildren() const
{
    RefPtr select = listBoxOptionParentNode();
    return select && is<HTMLOptGroupElement>(m_node) && !listItemsAreChildrenOfSelect(*select);
}

void AccessibilityListBoxOption::addChildren()
{
    if (canHaveChildren()) {
        AccessibilityRenderObject::addChildren();
        return;
    }

    m_childrenInitialized = true;
    m_childrenDirty = false;
    m_subtreeDirty = false;
}

bool AccessibilityListBoxOption::canSetSelectedAttribute() const
{
    RefPtr optionElement = dynamicDowncast<HTMLOptionElement>(m_node);
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
        return option->displayLabel();

    if (RefPtr optgroup = dynamicDowncast<HTMLOptGroupElement>(*m_node))
        return optgroup->groupLabelText();

    return { };
}

Element* AccessibilityListBoxOption::actionElement() const
{
    AX_ASSERT(is<HTMLElement>(m_node));
    return dynamicDowncast<Element>(m_node);
}

AccessibilityObject* AccessibilityListBoxOption::parentObject() const
{
    RefPtr parentNode = listBoxOptionParentNode();
    if (!parentNode)
        return nullptr;

    if (!listItemsAreChildrenOfSelect(*parentNode))
        return AccessibilityRenderObject::parentObject();

    CheckedPtr cache = protect(protect(m_node)->document())->axObjectCache();
    return cache ? cache->getOrCreate(*parentNode) : nullptr;
}

HTMLSelectElement* AccessibilityListBoxOption::listBoxOptionParentNode() const
{
    // A node that is being destroyed has no parent, and its tag name can no longer be checked.
    if (!m_node || !m_node->parentNode())
        return nullptr;

    if (auto* option = dynamicDowncast<HTMLOptionElement>(*m_node))
        return option->ownerSelectElement();

    if (auto* optgroup = dynamicDowncast<HTMLOptGroupElement>(*m_node))
        return optgroup->ownerSelectElement();

    return nullptr;
}

int AccessibilityListBoxOption::listBoxOptionIndex() const
{
    if (!m_node)
        return -1;

    RefPtr selectElement = listBoxOptionParentNode();
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
