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
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"

namespace WebCore {

using namespace HTMLNames;

AccessibilityMenuListOption::AccessibilityMenuListOption(HTMLOptionElement& element)
    : m_element(makeWeakPtr(element))
{
}

Ref<AccessibilityMenuListOption> AccessibilityMenuListOption::create(HTMLOptionElement& element)
{
    return adoptRef(*new AccessibilityMenuListOption(element));
}

Element* AccessibilityMenuListOption::actionElement() const
{
    return m_element.get();
}

Node* AccessibilityMenuListOption::node() const
{
    return m_element.get();
}
    
bool AccessibilityMenuListOption::isEnabled() const
{
    return m_element && !m_element->ownElementDisabled();
}

bool AccessibilityMenuListOption::isVisible() const
{
    if (!m_element)
        return false;

    // In a single-option select with the popup collapsed, only the selected item is considered visible.
    auto parent = m_element->document().axObjectCache()->getOrCreate(m_element->ownerSelectElement());
    return parent && (!parent->isOffScreen() || isSelected());
}

bool AccessibilityMenuListOption::isOffScreen() const
{
    // Invisible list options are considered to be offscreen.
    return !isVisible();
}

bool AccessibilityMenuListOption::isSelected() const
{
    return m_element && m_element->selected();
}

void AccessibilityMenuListOption::setSelected(bool selected)
{
    if (!canSetSelectedAttribute())
        return;
    
    m_element->setSelected(selected);
}

String AccessibilityMenuListOption::nameForMSAA() const
{
    return stringValue();
}

bool AccessibilityMenuListOption::canSetSelectedAttribute() const
{
    return isEnabled();
}

bool AccessibilityMenuListOption::computeAccessibilityIsIgnored() const
{
    return accessibilityIsIgnoredByDefault();
}

LayoutRect AccessibilityMenuListOption::elementRect() const
{
    AccessibilityObject* parent = parentObject();
    if (!parent)
        return boundingBoxRect();
    ASSERT(parent->isMenuListPopup());

    AccessibilityObject* grandparent = parent->parentObject();
    if (!grandparent)
        return boundingBoxRect();
    ASSERT(grandparent->isMenuList());

    return grandparent->elementRect();
}

String AccessibilityMenuListOption::stringValue() const
{
    return m_element ? m_element->label() : String();
}

} // namespace WebCore
