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
#include "OptionElement.h"
#include "RenderListBox.h"
#include "RenderMenuList.h"
#include "WMLDocument.h"
#include "WMLNames.h"
#include "WMLVariables.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

using namespace WMLNames;

WMLSelectElement::WMLSelectElement(const QualifiedName& tagName, Document* document)
    : WMLFormControlElement(tagName, document)
    , m_initialized(false)
{
}

WMLSelectElement::~WMLSelectElement()
{
}

const AtomicString& WMLSelectElement::formControlName() const
{
    AtomicString name = this->name();
    return name.isNull() ? emptyAtom : name;
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
    
void WMLSelectElement::setSelectedIndex(int optionIndex, bool deselect)
{
    SelectElement::setSelectedIndex(m_data, this, optionIndex, deselect, false, false);
}

void WMLSelectElement::setSelectedIndexByUser(int optionIndex, bool deselect, bool fireOnChangeNow)
{
    SelectElement::setSelectedIndex(m_data, this, optionIndex, deselect, fireOnChangeNow, true);
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

    // FIXME: There must be a better place to update the page variable state. Investigate.
    updateVariables();

    if (event->defaultHandled())
        return;

    WMLFormControlElement::defaultEventHandler(event);
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

void WMLSelectElement::selectInitialOptions()
{
    // Spec: Step 1 - the default option index is determined using iname and ivalue
    calculateDefaultOptionIndices();

    if (m_defaultOptionIndices.isEmpty()) {
        m_initialized = true;
        return;
    }

    // Spec: Step 2 – initialise variables
    initializeVariables();

    // Spec: Step 3 – pre-select option(s) specified by the default option index 
    selectDefaultOptions();
    m_initialized = true;
}

void WMLSelectElement::insertedIntoTree(bool deep)
{
    SelectElement::insertedIntoTree(m_data, this);
    WMLFormControlElement::insertedIntoTree(deep);
}

void WMLSelectElement::calculateDefaultOptionIndices()
{
    WMLPageState* pageState = wmlPageStateForDocument(document());
    if (!pageState)
        return;

    String variable;

    // Spec: If the 'iname' attribute is specified and names a variable that is set,
    // then the default option index is the validated value of that variable. 
    String iname = this->iname();
    if (!iname.isEmpty()) {
        variable = pageState->getVariable(iname);
        if (!variable.isEmpty())
            m_defaultOptionIndices = parseIndexValueString(variable);
    }

    // Spec: If the default option index is empty and the 'ivalue' attribute is specified,
    // then the default option index is the validated attribute value.
    String ivalue = this->ivalue();
    if (m_defaultOptionIndices.isEmpty() && !ivalue.isEmpty())
        m_defaultOptionIndices = parseIndexValueString(ivalue);

    // Spec: If the default option index is empty, and the 'name' attribute is specified
    // and the 'name' ttribute names a variable that is set, then for each value in the 'name'
    // variable that is present as a value in the select's option elements, the index of the
    // first option element containing that value is added to the default index if that
    // index has not been previously added. 
    String name = this->name();
    if (m_defaultOptionIndices.isEmpty() && !name.isEmpty()) {
        variable = pageState->getVariable(name);
        if (!variable.isEmpty())
            m_defaultOptionIndices = valueStringToOptionIndices(variable);
    }

    String value = parseValueSubstitutingVariableReferences(getAttribute(HTMLNames::valueAttr));

    // Spec: If the default option index is empty and the 'value' attribute is specified then
    // for each  value in the 'value' attribute that is present as a value in the select's
    // option elements, the index of the first option element containing that value is added
    // to the default index if that index has not been previously added. 
    if (m_defaultOptionIndices.isEmpty() && !value.isEmpty())
        m_defaultOptionIndices = valueStringToOptionIndices(value);

    // Spec: If the default option index is empty and the select is a multi-choice, then the
    // default option index is set to zero. If the select is single-choice, then the default
    // option index is set to one.
    if (m_defaultOptionIndices.isEmpty())
        m_defaultOptionIndices.append((unsigned) !m_data.multiple());
}

void WMLSelectElement::selectDefaultOptions()
{
    ASSERT(!m_defaultOptionIndices.isEmpty());

    if (!m_data.multiple()) {
        setSelectedIndex(m_defaultOptionIndices.first() - 1, false);
        return;
    }

    Vector<unsigned>::const_iterator end = m_defaultOptionIndices.end();
    for (Vector<unsigned>::const_iterator it = m_defaultOptionIndices.begin(); it != end; ++it)
        setSelectedIndex((*it) - 1, false);
}

void WMLSelectElement::initializeVariables()
{
    ASSERT(!m_defaultOptionIndices.isEmpty());

    WMLPageState* pageState = wmlPageStateForDocument(document());
    if (!pageState)
        return;

    const Vector<Element*>& items = m_data.listItems(this);
    if (items.isEmpty())
        return;

    // Spec: If the 'iname' attribute is specified, then the named variable is set with the default option index. 
    String iname = this->iname();
    if (!iname.isEmpty())
        pageState->storeVariable(iname, optionIndicesToString());

    String name = this->name();
    if (name.isEmpty())
        return;

    if (m_data.multiple()) {
        // Spec: If the 'name' attribute is specified and the select is a multiple-choice element,
        // then for each index greater than zero, the value of the 'value' attribute on the option
        // element at the index is added to the name variable. 
        pageState->storeVariable(name, optionIndicesToValueString());
        return;
    }

    // Spec: If the 'name' attribute is specified and the select is a single-choice element,
    // then the named variable is set with the value of the 'value' attribute on the option
    // element at the default option index. 
    unsigned optionIndex = m_defaultOptionIndices.first();
    ASSERT(optionIndex >= 1);

    int listIndex = optionToListIndex(optionIndex - 1);
    ASSERT(listIndex >= 0);
    ASSERT(listIndex < (int) items.size());

    if (OptionElement* optionElement = toOptionElement(items[listIndex]))
        pageState->storeVariable(name, optionElement->value());
}

void WMLSelectElement::updateVariables()
{
    WMLPageState* pageState = wmlPageStateForDocument(document());
    if (!pageState)
        return;

    String name = this->name();
    String iname = this->iname();
    if (iname.isEmpty() && name.isEmpty())
        return;

    String nameString;
    String inameString;

    unsigned optionIndex = 0;
    const Vector<Element*>& items = m_data.listItems(this);

    for (unsigned i = 0; i < items.size(); ++i) {
        OptionElement* optionElement = toOptionElement(items[i]);
        if (!optionElement)
            continue;

        ++optionIndex;
        if (!optionElement->selected())
            continue;

        if (!nameString.isEmpty())
            nameString += ";";

        if (!inameString.isEmpty())
            inameString += ";";

        nameString += optionElement->value();
        inameString += String::number(optionIndex);
    }

    if (!name.isEmpty())
        pageState->storeVariable(name, nameString);

    if (!iname.isEmpty())
        pageState->storeVariable(iname, inameString);
}

Vector<unsigned> WMLSelectElement::parseIndexValueString(const String& indexValue) const
{
    Vector<unsigned> indices;
    if (indexValue.isEmpty())
        return indices;

    Vector<String> indexStrings;
    indexValue.split(';', indexStrings);

    bool ok = false;
    unsigned optionCount = SelectElement::optionCount(m_data, this);

    Vector<String>::const_iterator end = indexStrings.end();
    for (Vector<String>::const_iterator it = indexStrings.begin(); it != end; ++it) {
        unsigned parsedValue = (*it).toUIntStrict(&ok);
        // Spec: Remove all non-integer indices from the value. Remove all out-of-range indices
        // from the value, where out-of-range is defined as any index with a value greater than
        // the number of options in the select or with a value less than one. 
        if (!ok || parsedValue < 1 || parsedValue > optionCount)
            continue;

        // Spec: Remove duplicate indices.
        if (indices.find(parsedValue) == notFound)
            indices.append(parsedValue);
    }

    return indices;
}

Vector<unsigned> WMLSelectElement::valueStringToOptionIndices(const String& value) const
{
    Vector<unsigned> indices;
    if (value.isEmpty())
        return indices;

    const Vector<Element*>& items = m_data.listItems(this);
    if (items.isEmpty())
        return indices;

    Vector<String> indexStrings;
    value.split(';', indexStrings);

    unsigned optionIndex = 0;

    Vector<String>::const_iterator end = indexStrings.end();
    for (Vector<String>::const_iterator it = indexStrings.begin(); it != end; ++it) {
        String value = *it;

        for (unsigned i = 0; i < items.size(); ++i) {
            if (!isOptionElement(items[i]))
                continue;

            ++optionIndex;
            if (OptionElement* optionElement = toOptionElement(items[i])) {
                if (optionElement->value() == value) {
                    indices.append(optionIndex);
                    break;
                }
            }
        }
    }

    return indices;
}

String WMLSelectElement::optionIndicesToValueString() const
{
    String valueString;
    if (m_defaultOptionIndices.isEmpty())
        return valueString;

    const Vector<Element*>& items = m_data.listItems(this);
    if (items.isEmpty())
        return valueString;

    Vector<unsigned>::const_iterator end = m_defaultOptionIndices.end();
    for (Vector<unsigned>::const_iterator it = m_defaultOptionIndices.begin(); it != end; ++it) {
        unsigned optionIndex = (*it);
        if (optionIndex < 1 || optionIndex > items.size())
            continue;

        int listIndex = optionToListIndex((*it) - 1);
        ASSERT(listIndex >= 0);
        ASSERT(listIndex < (int) items.size());

        if (OptionElement* optionElement = toOptionElement(items[listIndex])) {
            if (!valueString.isEmpty())
                valueString += ";";

            valueString += optionElement->value();
        }
    }

    return valueString;
}

String WMLSelectElement::optionIndicesToString() const
{
    String valueString;
    if (m_defaultOptionIndices.isEmpty())
        return valueString;

    Vector<unsigned>::const_iterator end = m_defaultOptionIndices.end();
    for (Vector<unsigned>::const_iterator it = m_defaultOptionIndices.begin(); it != end; ++it) {
        if (!valueString.isEmpty())
            valueString += ";";

        valueString += String::number(*it);
    }

    return valueString;
}

String WMLSelectElement::name() const
{
    return parseValueForbiddingVariableReferences(getAttribute(HTMLNames::nameAttr));
}

String WMLSelectElement::value() const
{
    return parseValueSubstitutingVariableReferences(getAttribute(HTMLNames::valueAttr));
}

String WMLSelectElement::iname() const
{
    return parseValueForbiddingVariableReferences(getAttribute(inameAttr));
}

String WMLSelectElement::ivalue() const
{
    return parseValueSubstitutingVariableReferences(getAttribute(ivalueAttr));
}

void WMLSelectElement::listBoxSelectItem(int listIndex, bool allowMultiplySelections, bool shift, bool fireOnChangeNow)
{
    /* Dummy implementation as listBoxSelectItem is pure virtual in SelectElement class */
}

}

#endif
