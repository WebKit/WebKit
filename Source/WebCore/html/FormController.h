/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
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
 *
 */

#ifndef FormController_h
#define FormController_h

#include "CheckedRadioButtons.h"
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FormAssociatedElement;
class FormKeyGenerator;
class HTMLFormControlElementWithState;
class HTMLFormElement;

class FormElementKey {
public:
    FormElementKey(AtomicStringImpl* = 0, AtomicStringImpl* = 0, AtomicStringImpl* = 0);
    ~FormElementKey();
    FormElementKey(const FormElementKey&);
    FormElementKey& operator=(const FormElementKey&);

    AtomicStringImpl* name() const { return m_name; }
    AtomicStringImpl* type() const { return m_type; }
    AtomicStringImpl* formKey() const { return m_formKey; }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    FormElementKey(WTF::HashTableDeletedValueType) : m_name(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_name == hashTableDeletedValue(); }

private:
    void ref() const;
    void deref() const;

    static AtomicStringImpl* hashTableDeletedValue() { return reinterpret_cast<AtomicStringImpl*>(-1); }

    AtomicStringImpl* m_name;
    AtomicStringImpl* m_type;
    AtomicStringImpl* m_formKey;
};

inline bool operator==(const FormElementKey& a, const FormElementKey& b)
{
    return a.name() == b.name() && a.type() == b.type() && a.formKey() == b.formKey();
}

struct FormElementKeyHash {
    static unsigned hash(const FormElementKey&);
    static bool equal(const FormElementKey& a, const FormElementKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct FormElementKeyHashTraits : WTF::GenericHashTraits<FormElementKey> {
    static void constructDeletedValue(FormElementKey& slot) { new (NotNull, &slot) FormElementKey(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const FormElementKey& value) { return value.isHashTableDeletedValue(); }
};

class FormControlState {
public:
    FormControlState() : m_type(TypeSkip) { }
    explicit FormControlState(const String& value) : m_type(TypeRestore) { m_values.append(value); }
    static FormControlState deserialize(const Vector<String>& stateVector, size_t& index);
    FormControlState(const FormControlState& another) : m_type(another.m_type), m_values(another.m_values) { }
    FormControlState& operator=(const FormControlState&);

    bool isFailure() const { return m_type == TypeFailure; }
    size_t valueSize() const { return m_values.size(); }
    const String& operator[](size_t i) const { return m_values[i]; }
    void append(const String&);
    void serializeTo(Vector<String>& stateVector) const;

private:
    enum Type { TypeSkip, TypeRestore, TypeFailure };
    explicit FormControlState(Type type) : m_type(type) { }

    Type m_type;
    Vector<String> m_values;
};

inline FormControlState& FormControlState::operator=(const FormControlState& another)
{
    m_type = another.m_type;
    m_values = another.m_values;
    return *this;
}

inline void FormControlState::append(const String& value)
{
    m_type = TypeRestore;
    m_values.append(value);
}

class FormController {
public:
    static PassOwnPtr<FormController> create()
    {
        return adoptPtr(new FormController);
    }
    ~FormController();

    CheckedRadioButtons& checkedRadioButtons() { return m_checkedRadioButtons; }
    
    void registerFormElementWithState(HTMLFormControlElementWithState* control) { m_formElementsWithState.add(control); }
    void unregisterFormElementWithState(HTMLFormControlElementWithState* control) { m_formElementsWithState.remove(control); }
    // This should be callled only by Document::formElementsState().
    Vector<String> formElementsState() const;
    // This should be callled only by Document::setStateForNewFormElements().
    void setStateForNewFormElements(const Vector<String>&);
    FormControlState takeStateForFormElement(const HTMLFormControlElementWithState&);
    void willDeleteForm(HTMLFormElement*);

    void registerFormElementWithFormAttribute(FormAssociatedElement*);
    void unregisterFormElementWithFormAttribute(FormAssociatedElement*);
    void resetFormElementsOwner();

private:
    FormController();

    CheckedRadioButtons m_checkedRadioButtons;

    typedef ListHashSet<HTMLFormControlElementWithState*, 64> FormElementListHashSet;
    FormElementListHashSet m_formElementsWithState;
    typedef ListHashSet<RefPtr<FormAssociatedElement>, 32> FormAssociatedElementListHashSet;
    FormAssociatedElementListHashSet m_formElementsWithFormAttribute;

    typedef HashMap<FormElementKey, Deque<FormControlState>, FormElementKeyHash, FormElementKeyHashTraits> FormElementStateMap;
    FormElementStateMap m_stateForNewFormElements;
    OwnPtr<FormKeyGenerator> m_formKeyGenerator;
};

} // namespace WebCore
#endif
