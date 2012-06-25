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
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

// ----------------------------------------------------------------------------

// Serilized form of FormControlState:
//
// SerializedControlState ::= SkipState | RestoreState
// SkipState ::= ''
// RestoreState ::= (',' EscapedValue )+
// EscapedValue ::= ('\\' | '\,' | [^\,])+

String FormControlState::serialize() const
{
    ASSERT(!isFailure());
    if (!m_values.size())
        return emptyString();

    size_t enoughSize = 0;
    for (size_t i = 0; i < m_values.size(); ++i)
        enoughSize += 1 + m_values[i].length() * 2;
    StringBuilder builder;
    builder.reserveCapacity(enoughSize);
    for (size_t i = 0; i < m_values.size(); ++i) {
        builder.append(',');
        builder.appendEscaped(m_values[i], '\\', ',');
    }
    return builder.toString();
}

FormControlState FormControlState::deserialize(const String& escaped)
{
    if (!escaped.length())
        return FormControlState();
    if (escaped[0] != ',')
        return FormControlState(TypeFailure);

    size_t valueSize = 1;
    for (unsigned i = 1; i < escaped.length(); ++i) {
        if (escaped[i] == '\\') {
            if (++i >= escaped.length())
                return FormControlState(TypeFailure);
        } else if (escaped[i] == ',')
            valueSize++;
    }

    FormControlState state;
    state.m_values.reserveCapacity(valueSize);
    StringBuilder builder;
    for (unsigned i = 1; i < escaped.length(); ++i) {
        if (escaped[i] == '\\') {
            if (++i >= escaped.length())
                return FormControlState(TypeFailure);
            builder.append(escaped[i]);
        } else if (escaped[i] == ',') {
            state.append(builder.toString());
            builder.clear();
        } else
            builder.append(escaped[i]);
    }
    state.append(builder.toString());
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
    DEFINE_STATIC_LOCAL(String, signature, ("\n\r?% WebKit serialized form state version 4 \n\r=&"));
    return signature;
}

Vector<String> FormController::formElementsState() const
{
    Vector<String> stateVector;
    stateVector.reserveInitialCapacity(m_formElementsWithState.size() * 3 + 1);
    stateVector.append(formStateSignature());
    typedef FormElementListHashSet::const_iterator Iterator;
    Iterator end = m_formElementsWithState.end();
    for (Iterator it = m_formElementsWithState.begin(); it != end; ++it) {
        HTMLFormControlElementWithState* elementWithState = *it;
        if (!elementWithState->shouldSaveAndRestoreFormControlState())
            continue;
        stateVector.append(elementWithState->name().string());
        stateVector.append(elementWithState->formControlType().string());
        stateVector.append(elementWithState->saveFormControlState().serialize());
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

    if (stateVector.size() < 1 || stateVector[0] != formStateSignature())
        return;
    if ((stateVector.size() - 1) % 3)
        return;

    for (size_t i = 1; i < stateVector.size(); i += 3) {
        AtomicString name = stateVector[i];
        AtomicString type = stateVector[i + 1];
        FormControlState state = FormControlState::deserialize(stateVector[i + 2]);
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

