/*
 * Copyright (C) 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2011, 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "FormController.h"

#include "HTMLFormControlElementWithState.h"

namespace WebCore {

using namespace HTMLNames;

FormController::FormController()
{
}

FormController::~FormController()
{
}

Vector<String> FormController::formElementsState() const
{
    Vector<String> stateVector;
    stateVector.reserveInitialCapacity(m_formElementsWithState.size() * 3);
    typedef FormElementListHashSet::const_iterator Iterator;
    Iterator end = m_formElementsWithState.end();
    for (Iterator it = m_formElementsWithState.begin(); it != end; ++it) {
        HTMLFormControlElementWithState* elementWithState = *it;
        if (!elementWithState->shouldSaveAndRestoreFormControlState())
            continue;
        FormControlState state = elementWithState->saveFormControlState();
        if (!state.hasValue())
            continue;
        stateVector.append(elementWithState->name().string());
        stateVector.append(elementWithState->formControlType().string());
        stateVector.append(state.value());
    }
    return stateVector;
}

static bool isNotFormControlTypeCharacter(UChar ch)
{
    return ch != '-' && (ch > 'z' || ch < 'a');
}

void FormController::setStateForNewFormElements(const Vector<String>& stateVector)
{
    // Walk the state vector backwards so that the value to use for each
    // name/type pair first is the one at the end of each individual vector
    // in the FormElementStateMap. We're using them like stacks.
    typedef FormElementStateMap::iterator Iterator;
    m_formElementsWithState.clear();

    if (stateVector.size() % 3)
        return;
    for (size_t i = 0; i < stateVector.size(); i += 3) {
        if (stateVector[i + 1].find(isNotFormControlTypeCharacter) != notFound)
            return;
    }

    for (size_t i = stateVector.size() / 3 * 3; i; i -= 3) {
        AtomicString name = stateVector[i - 3];
        AtomicString type = stateVector[i - 2];
        const String& value = stateVector[i - 1];
        FormElementKey key(name.impl(), type.impl());
        Iterator it = m_stateForNewFormElements.find(key);
        if (it != m_stateForNewFormElements.end())
            it->second.append(value);
        else {
            Vector<String> valueList(1);
            valueList[0] = value;
            m_stateForNewFormElements.set(key, valueList);
        }
    }
}

bool FormController::hasStateForNewFormElements() const
{
    return !m_stateForNewFormElements.isEmpty();
}

FormControlState FormController::takeStateForFormElement(AtomicStringImpl* name, AtomicStringImpl* type)
{
    typedef FormElementStateMap::iterator Iterator;
    Iterator it = m_stateForNewFormElements.find(FormElementKey(name, type));
    if (it == m_stateForNewFormElements.end())
        return FormControlState();
    ASSERT(it->second.size());
    FormControlState state(it->second.last());
    if (it->second.size() > 1)
        it->second.removeLast();
    else
        m_stateForNewFormElements.remove(it);
    return state;
}

void FormController::registerFormElementWithFormAttribute(FormAssociatedElement* element)
{
    ASSERT(toHTMLElement(element)->fastHasAttribute(formAttr));
    m_formElementsWithFormAttribute.add(element);
}

void FormController::unregisterFormElementWithFormAttribute(FormAssociatedElement* element)
{
    m_formElementsWithFormAttribute.remove(element);
}

void FormController::resetFormElementsOwner()
{
    typedef FormAssociatedElementListHashSet::iterator Iterator;
    Iterator end = m_formElementsWithFormAttribute.end();
    for (Iterator it = m_formElementsWithFormAttribute.begin(); it != end; ++it)
        (*it)->resetFormOwner();
}

FormElementKey::FormElementKey(AtomicStringImpl* name, AtomicStringImpl* type)
    : m_name(name), m_type(type)
{
    ref();
}

FormElementKey::~FormElementKey()
{
    deref();
}

FormElementKey::FormElementKey(const FormElementKey& other)
    : m_name(other.name()), m_type(other.type())
{
    ref();
}

FormElementKey& FormElementKey::operator=(const FormElementKey& other)
{
    other.ref();
    deref();
    m_name = other.name();
    m_type = other.type();
    return *this;
}

void FormElementKey::ref() const
{
    if (name())
        name()->ref();
    if (type())
        type()->ref();
}

void FormElementKey::deref() const
{
    if (name())
        name()->deref();
    if (type())
        type()->deref();
}

unsigned FormElementKeyHash::hash(const FormElementKey& key)
{
    return StringHasher::hashMemory<sizeof(FormElementKey)>(&key);
}

} // namespace WebCore

