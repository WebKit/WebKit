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

// ----------------------------------------------------------------------------

// Serilized form of FormControlState:
//  (',' means strings around it are separated in stateVector.)
//
// SerializedControlState ::= SkipState | RestoreState
// SkipState ::= '0'
// RestoreState ::= UnsignedNumber, ControlValue+
// UnsignedNumber ::= [0-9]+
// ControlValue ::= arbitrary string
//
// RestoreState has a sequence of ControlValues. The length of the
// sequence is represented by UnsignedNumber.

void FormControlState::serializeTo(Vector<String>& stateVector) const
{
    ASSERT(!isFailure());
    stateVector.append(String::number(m_values.size()));
    for (size_t i = 0; i < m_values.size(); ++i)
        stateVector.append(m_values[i].isNull() ? emptyString() : m_values[i]);
}

FormControlState FormControlState::deserialize(const Vector<String>& stateVector, size_t& index)
{
    if (index >= stateVector.size())
        return FormControlState(TypeFailure);
    size_t valueSize = stateVector[index++].toUInt();
    if (!valueSize)
        return FormControlState();
    if (index + valueSize > stateVector.size())
        return FormControlState(TypeFailure);
    FormControlState state;
    state.m_values.reserveCapacity(valueSize);
    for (size_t i = 0; i < valueSize; ++i)
        state.append(stateVector[index++]);
    return state;
}

// ----------------------------------------------------------------------------


FormController::FormController()
{
}

FormController::~FormController()
{
}

static String formStateSignature()
{
    // In the legacy version of serialized state, the first item was a name
    // attribute value of a form control. The following string literal should
    // contain some characters which are rarely used for name attribute values.
    DEFINE_STATIC_LOCAL(String, signature, ("\n\r?% WebKit serialized form state version 3 \n\r=&"));
    return signature;
}

Vector<String> FormController::formElementsState() const
{
    Vector<String> stateVector;
    stateVector.reserveInitialCapacity(m_formElementsWithState.size() * 4 + 1);
    stateVector.append(formStateSignature());
    typedef FormElementListHashSet::const_iterator Iterator;
    Iterator end = m_formElementsWithState.end();
    for (Iterator it = m_formElementsWithState.begin(); it != end; ++it) {
        HTMLFormControlElementWithState* elementWithState = *it;
        if (!elementWithState->shouldSaveAndRestoreFormControlState())
            continue;
        stateVector.append(elementWithState->name().string());
        stateVector.append(elementWithState->formControlType().string());
        elementWithState->saveFormControlState().serializeTo(stateVector);
    }
    return stateVector;
}

static bool isNotFormControlTypeCharacter(UChar ch)
{
    return ch != '-' && (ch > 'z' || ch < 'a');
}

void FormController::setStateForNewFormElements(const Vector<String>& stateVector)
{
    typedef FormElementStateMap::iterator Iterator;
    m_formElementsWithState.clear();

    size_t i = 0;
    if (stateVector.size() < 1 || stateVector[i++] != formStateSignature())
        return;

    while (i + 2 < stateVector.size()) {
        AtomicString name = stateVector[i++];
        AtomicString type = stateVector[i++];
        FormControlState state = FormControlState::deserialize(stateVector, i);
        if (type.isEmpty() || type.impl()->find(isNotFormControlTypeCharacter) != notFound || state.isFailure())
            break;

        FormElementKey key(name.impl(), type.impl());
        Iterator it = m_stateForNewFormElements.find(key);
        if (it != m_stateForNewFormElements.end())
            it->second.append(state);
        else {
            Deque<FormControlState> stateList;
            stateList.append(state);
            m_stateForNewFormElements.set(key, stateList);
        }
    }
    if (i != stateVector.size())
        m_stateForNewFormElements.clear();
}

FormControlState FormController::takeStateForFormElement(const HTMLFormControlElementWithState& control)
{
    if (m_stateForNewFormElements.isEmpty())
        return FormControlState();
    typedef FormElementStateMap::iterator Iterator;
    Iterator it = m_stateForNewFormElements.find(FormElementKey(control.name().impl(), control.type().impl()));
    if (it == m_stateForNewFormElements.end())
        return FormControlState();
    ASSERT(it->second.size());
    FormControlState state = it->second.takeFirst();
    if (!it->second.size())
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

