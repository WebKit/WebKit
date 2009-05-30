/**
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(WML)
#include "WMLSelectElement.h"

#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {

WMLSelectElement::WMLSelectElement(const QualifiedName& tagName, Document* document)
    : WMLFormControlElement(tagName, document)
{
}

WMLSelectElement::~WMLSelectElement()
{
}

const AtomicString& WMLSelectElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, selectMultiple, ("select-multiple"));
    DEFINE_STATIC_LOCAL(const AtomicString, selectOne, ("select-one"));
    return m_data.multiple() ? selectMultiple : selectOne;
}

bool WMLSelectElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (renderer())
        return isFocusable();

    return WMLFormControlElement::isKeyboardFocusable(event);
}

bool WMLSelectElement::isMouseFocusable() const
{
    if (renderer())
        return isFocusable();

    return WMLFormControlElement::isMouseFocusable();
}

void WMLSelectElement::selectAll()
{
    SelectElement::selectAll(m_data, this);
}

void WMLSelectElement::recalcStyle(StyleChange change)
{
    SelectElement::recalcStyle(m_data, this);
    WMLFormControlElement::recalcStyle(change);
}

void WMLSelectElement::dispatchFocusEvent()
{
    SelectElement::dispatchFocusEvent(m_data, this);
    WMLFormControlElement::dispatchFocusEvent();
}

void WMLSelectElement::dispatchBlurEvent()
{
    SelectElement::dispatchBlurEvent(m_data, this);
    WMLFormControlElement::dispatchBlurEvent();
}

int WMLSelectElement::selectedIndex() const
{
    return SelectElement::selectedIndex(m_data, this);
}
    
void WMLSelectElement::setSelectedIndex(int index, bool deselect, bool fireOnChange)
{
    SelectElement::setSelectedIndex(m_data, this, index, deselect, fireOnChange);
}

bool WMLSelectElement::saveFormControlState(String& value) const
{
    return SelectElement::saveFormControlState(m_data, this, value);
}

void WMLSelectElement::restoreFormControlState(const String& state)
{
    SelectElement::restoreFormControlState(m_data, this, state);
}

void WMLSelectElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    SelectElement::setRecalcListItems(m_data, this);
    WMLFormControlElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}

void WMLSelectElement::parseMappedAttribute(MappedAttribute* attr) 
{
    if (attr->name() == HTMLNames::multipleAttr)
        SelectElement::parseMultipleAttribute(m_data, this, attr);
    else
        WMLFormControlElement::parseMappedAttribute(attr);
}

RenderObject* WMLSelectElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (m_data.usesMenuList())
        return new (arena) RenderMenuList(this);
    return new (arena) RenderListBox(this);
}

bool WMLSelectElement::appendFormData(FormDataList& list, bool)
{
    return SelectElement::appendFormData(m_data, this, list);
}

int WMLSelectElement::optionToListIndex(int optionIndex) const
{
    return SelectElement::optionToListIndex(m_data, this, optionIndex);
}

int WMLSelectElement::listToOptionIndex(int listIndex) const
{
    return SelectElement::listToOptionIndex(m_data, this, listIndex);
}

void WMLSelectElement::reset()
{
    SelectElement::reset(m_data, this);
}

void WMLSelectElement::defaultEventHandler(Event* event)
{
    SelectElement::defaultEventHandler(m_data, this, event);
}

void WMLSelectElement::accessKeyAction(bool sendToAnyElement)
{
    focus();
    dispatchSimulatedClick(0, sendToAnyElement);
}

void WMLSelectElement::setActiveSelectionAnchorIndex(int index)
{
    SelectElement::setActiveSelectionAnchorIndex(m_data, this, index);
}
    
void WMLSelectElement::setActiveSelectionEndIndex(int index)
{
    SelectElement::setActiveSelectionEndIndex(m_data, index);
}

void WMLSelectElement::updateListBoxSelection(bool deselectOtherOptions)
{
    SelectElement::updateListBoxSelection(m_data, this, deselectOtherOptions);
}

void WMLSelectElement::listBoxOnChange()
{
    SelectElement::listBoxOnChange(m_data, this);
}

void WMLSelectElement::menuListOnChange()
{
    SelectElement::menuListOnChange(m_data, this);
}

int WMLSelectElement::activeSelectionStartListIndex() const
{
    if (m_data.activeSelectionAnchorIndex() >= 0)
        return m_data.activeSelectionAnchorIndex();
    return optionToListIndex(selectedIndex());
}

int WMLSelectElement::activeSelectionEndListIndex() const
{
    if (m_data.activeSelectionEndIndex() >= 0)
        return m_data.activeSelectionEndIndex();
    return SelectElement::lastSelectedListIndex(m_data, this);
}

void WMLSelectElement::accessKeySetSelectedIndex(int index)
{
    SelectElement::accessKeySetSelectedIndex(m_data, this, index);
}

void WMLSelectElement::setRecalcListItems()
{
    SelectElement::setRecalcListItems(m_data, this);
}

void WMLSelectElement::scrollToSelection()
{
    SelectElement::scrollToSelection(m_data, this);
}

void WMLSelectElement::insertedIntoTree(bool deep)
{
    SelectElement::insertedIntoTree(m_data, this);
    WMLFormControlElement::insertedIntoTree(deep);
}

}

#endif
