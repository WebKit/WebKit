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
#include "AccessibilityMenuListOption.h"

#include "AXObjectCache.h"
#include "AccessibilityMenuListPopup.h"
#include "Document.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityMenuListOption::AccessibilityMenuListOption(AXID axID, HTMLOptionElement& element)
    : AccessibilityNodeObject(axID, &element)
    , m_parent(nullptr)
{
}

Ref<AccessibilityMenuListOption> AccessibilityMenuListOption::create(AXID axID, HTMLOptionElement& element)
{
    return adoptRef(*new AccessibilityMenuListOption(axID, element));
}

HTMLOptionElement* AccessibilityMenuListOption::optionElement() const
{
    return downcast<HTMLOptionElement>(node());
}

Element* AccessibilityMenuListOption::actionElement() const
{
    return downcast<Element>(node());
}

bool AccessibilityMenuListOption::isEnabled() const
{
    auto* optionElement = this->optionElement();
    return optionElement && !optionElement->ownElementDisabled();
}

bool AccessibilityMenuListOption::isVisible() const
{
    WeakPtr optionElement = this->optionElement();
    if (!optionElement)
        return false;

    // In a single-option select with the popup collapsed, only the selected item is considered visible.
    auto* ownerSelectElement = optionElement->document().axObjectCache()->getOrCreate(optionElement->ownerSelectElement());
    return ownerSelectElement && (!ownerSelectElement->isOffScreen() || isSelected());
}

bool AccessibilityMenuListOption::isOffScreen() const
{
    // Invisible list options are considered to be offscreen.
    return !isVisible();
}

bool AccessibilityMenuListOption::isSelected() const
{
    auto* optionElement = this->optionElement();
    return optionElement && optionElement->selected();
}

void AccessibilityMenuListOption::setSelected(bool selected)
{
    if (!canSetSelectedAttribute())
        return;
    
    if (auto* optionElement = this->optionElement())
        optionElement->setSelected(selected);
}

bool AccessibilityMenuListOption::canSetSelectedAttribute() const
{
    return isEnabled();
}

bool AccessibilityMenuListOption::computeIsIgnored() const
{
    return isIgnoredByDefault();
}

LayoutRect AccessibilityMenuListOption::elementRect() const
{
    RefPtr parent = parentObject();
    // Our parent should've been set to be a menu-list popup before this method is called.
    ASSERT(parent && parent->isMenuListPopup());
    if (!parent)
        return boundingBoxRect();

    RefPtr grandparent = parent->parentObject();
    ASSERT(!grandparent || grandparent->isMenuList());

    return grandparent ? grandparent->elementRect() : boundingBoxRect();
}

String AccessibilityMenuListOption::stringValue() const
{
    auto* optionElement = this->optionElement();
    return optionElement ? optionElement->label() : String();
}

} // namespace WebCore
