/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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

#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "ScriptDisallowedScope.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

using namespace HTMLNames;

static inline HTMLFormElement* ownerFormForState(const HTMLFormControlElementWithState& control)
{
    // Assume controls with form attribute have no owners because we restore
    // state during parsing and form owners of such controls might be
    // indeterminate.
    return control.hasAttributeWithoutSynchronization(formAttr) ? 0 : control.form();
}

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

static inline void serializeFormControlStateTo(const FormControlState& formControlState, Vector<String>& stateVector)
{
    stateVector.append(String::number(formControlState.size()));
    for (auto& value : formControlState)
        stateVector.append(value.isNull() ? emptyString() : value);
}

static inline std::optional<FormControlState> deserializeFormControlState(const Vector<String>& stateVector, size_t& index)
{
    if (index >= stateVector.size())
        return std::nullopt;
    size_t size = stateVector[index++].toUInt();
    if (index + size > stateVector.size())
        return std::nullopt;
    Vector<String> subvector;
    subvector.reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i)
        subvector.uncheckedAppend(stateVector[index++]);
    return WTFMove(subvector);
}

// ----------------------------------------------------------------------------

class FormElementKey {
public:
    FormElementKey(AtomicStringImpl* = 0, AtomicStringImpl* = 0);
    ~FormElementKey();
    FormElementKey(const FormElementKey&);
    FormElementKey& operator=(const FormElementKey&);

    AtomicStringImpl* name() const { return m_name; }
    AtomicStringImpl* type() const { return m_type; }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    FormElementKey(WTF::HashTableDeletedValueType) : m_name(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_name == hashTableDeletedValue(); }

private:
    void ref() const;
    void deref() const;

    static AtomicStringImpl* hashTableDeletedValue() { return reinterpret_cast<AtomicStringImpl*>(-1); }

    AtomicStringImpl* m_name;
    AtomicStringImpl* m_type;
};

FormElementKey::FormElementKey(AtomicStringImpl* name, AtomicStringImpl* type)
    : m_name(name)
    , m_type(type)
{
    ref();
}

FormElementKey::~FormElementKey()
{
    deref();
}

FormElementKey::FormElementKey(const FormElementKey& other)
    : m_name(other.name())
    , m_type(other.type())
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

inline bool operator==(const FormElementKey& a, const FormElementKey& b)
{
    return a.name() == b.name() && a.type() == b.type();
}

struct FormElementKeyHash {
    static unsigned hash(const FormElementKey&);
    static bool equal(const FormElementKey& a, const FormElementKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

unsigned FormElementKeyHash::hash(const FormElementKey& key)
{
    return StringHasher::hashMemory<sizeof(FormElementKey)>(&key);
}

struct FormElementKeyHashTraits : WTF::GenericHashTraits<FormElementKey> {
    static void constructDeletedValue(FormElementKey& slot) { new (NotNull, &slot) FormElementKey(WTF::HashTableDeletedValue); }
    static bool isDeletedValue(const FormElementKey& value) { return value.isHashTableDeletedValue(); }
};

// ----------------------------------------------------------------------------

class SavedFormState {
    WTF_MAKE_NONCOPYABLE(SavedFormState);
    WTF_MAKE_FAST_ALLOCATED;

public:
    SavedFormState() = default;
    static std::unique_ptr<SavedFormState> deserialize(const Vector<String>&, size_t& index);
    void serializeTo(Vector<String>&) const;
    bool isEmpty() const { return m_stateForNewFormElements.isEmpty(); }
    void appendControlState(const AtomicString& name, const AtomicString& type, const FormControlState&);
    FormControlState takeControlState(const AtomicString& name, const AtomicString& type);

    Vector<String> referencedFilePaths() const;

private:
    HashMap<FormElementKey, Deque<FormControlState>, FormElementKeyHash, FormElementKeyHashTraits> m_stateForNewFormElements;
    size_t m_controlStateCount { 0 };
};

static bool isNotFormControlTypeCharacter(UChar ch)
{
    return !(ch == '-' || isASCIILower(ch));
}

std::unique_ptr<SavedFormState> SavedFormState::deserialize(const Vector<String>& stateVector, size_t& index)
{
    if (index >= stateVector.size())
        return nullptr;
    // FIXME: We need String::toSizeT().
    size_t itemCount = stateVector[index++].toUInt();
    if (!itemCount)
        return nullptr;
    auto savedFormState = std::make_unique<SavedFormState>();
    while (itemCount--) {
        if (index + 1 >= stateVector.size())
            return nullptr;
        String name = stateVector[index++];
        String type = stateVector[index++];
        auto state = deserializeFormControlState(stateVector, index);
        if (type.isEmpty() || type.find(isNotFormControlTypeCharacter) != notFound || !state)
            return nullptr;
        savedFormState->appendControlState(name, type, state.value());
    }
    return savedFormState;
}

void SavedFormState::serializeTo(Vector<String>& stateVector) const
{
    stateVector.append(String::number(m_controlStateCount));
    for (auto& element : m_stateForNewFormElements) {
        const FormElementKey& key = element.key;
        for (auto& controlState : element.value) {
            stateVector.append(key.name());
            stateVector.append(key.type());
            serializeFormControlStateTo(controlState, stateVector);
        }
    }
}

void SavedFormState::appendControlState(const AtomicString& name, const AtomicString& type, const FormControlState& state)
{
    m_stateForNewFormElements.add({ name.impl(), type.impl() }, Deque<FormControlState> { }).iterator->value.append(state);
    ++m_controlStateCount;
}

FormControlState SavedFormState::takeControlState(const AtomicString& name, const AtomicString& type)
{
    auto iterator = m_stateForNewFormElements.find({ name.impl(), type.impl() });
    if (iterator == m_stateForNewFormElements.end())
        return { };

    auto state = iterator->value.takeFirst();
    --m_controlStateCount;
    if (iterator->value.isEmpty())
        m_stateForNewFormElements.remove(iterator);
    return state;
}

Vector<String> SavedFormState::referencedFilePaths() const
{
    Vector<String> toReturn;
    for (auto& element : m_stateForNewFormElements) {
        if (!equal(element.key.type(), "file", 4))
            continue;
        for (auto& state : element.value) {
            for (auto& file : HTMLInputElement::filesFromFileInputFormControlState(state))
                toReturn.append(file.path);
        }
    }
    return toReturn;
}

// ----------------------------------------------------------------------------

class FormKeyGenerator {
    WTF_MAKE_NONCOPYABLE(FormKeyGenerator);
    WTF_MAKE_FAST_ALLOCATED;

public:
    FormKeyGenerator() = default;
    AtomicString formKey(const HTMLFormControlElementWithState&);
    void willDeleteForm(HTMLFormElement*);

private:
    typedef HashMap<HTMLFormElement*, AtomicString> FormToKeyMap;
    typedef HashMap<String, unsigned> FormSignatureToNextIndexMap;
    FormToKeyMap m_formToKeyMap;
    FormSignatureToNextIndexMap m_formSignatureToNextIndexMap;
};

static inline void recordFormStructure(const HTMLFormElement& form, StringBuilder& builder)
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    // 2 is enough to distinguish forms in webkit.org/b/91209#c0
    const size_t namedControlsToBeRecorded = 2;
    auto& controls = form.unsafeAssociatedElements();
    builder.appendLiteral(" [");
    for (size_t i = 0, namedControls = 0; i < controls.size() && namedControls < namedControlsToBeRecorded; ++i) {
        if (!controls[i]->isFormControlElementWithState())
            continue;
        RefPtr<HTMLFormControlElementWithState> control = static_cast<HTMLFormControlElementWithState*>(controls[i]);
        if (!ownerFormForState(*control))
            continue;
        AtomicString name = control->name();
        if (name.isEmpty())
            continue;
        namedControls++;
        builder.append(name);
        builder.append(' ');
    }
    builder.append(']');
}

static inline String formSignature(const HTMLFormElement& form)
{
    URL actionURL = form.getURLAttribute(actionAttr);
    // Remove the query part because it might contain volatile parameters such
    // as a session key.
    actionURL.setQuery(String());
    StringBuilder builder;
    if (!actionURL.isEmpty())
        builder.append(actionURL.string());

    recordFormStructure(form, builder);
    return builder.toString();
}

AtomicString FormKeyGenerator::formKey(const HTMLFormControlElementWithState& control)
{
    auto form = makeRefPtr(ownerFormForState(control));
    if (!form) {
        static NeverDestroyed<AtomicString> formKeyForNoOwner("No owner", AtomicString::ConstructFromLiteral);
        return formKeyForNoOwner;
    }

    return m_formToKeyMap.ensure(form.get(), [this, &form] {
        auto signature = formSignature(*form);
        auto nextIndex = m_formSignatureToNextIndexMap.add(signature, 0).iterator->value++;
        // FIXME: Would be nice to have makeAtomicString to use here.
        return makeString(signature, " #", nextIndex);
    }).iterator->value;
}

void FormKeyGenerator::willDeleteForm(HTMLFormElement* form)
{
    ASSERT(form);
    m_formToKeyMap.remove(form);
}

// ----------------------------------------------------------------------------

FormController::FormController() = default;

FormController::~FormController() = default;

unsigned FormController::formElementsCharacterCount() const
{
    unsigned count = 0;
    for (auto& element : m_formElementsWithState) {
        if (element->isTextField())
            count += element->saveFormControlState()[0].length();
    }
    return count;
}

static String formStateSignature()
{
    // In the legacy version of serialized state, the first item was a name
    // attribute value of a form control. The following string literal should
    // contain some characters which are rarely used for name attribute values.
    static NeverDestroyed<String> signature(MAKE_STATIC_STRING_IMPL("\n\r?% WebKit serialized form state version 8 \n\r=&"));
    return signature;
}

std::unique_ptr<FormController::SavedFormStateMap> FormController::createSavedFormStateMap(const FormElementListHashSet& controlList)
{
    FormKeyGenerator keyGenerator;
    auto stateMap = std::make_unique<SavedFormStateMap>();
    for (auto& control : controlList) {
        if (!control->shouldSaveAndRestoreFormControlState())
            continue;
        auto& formState = stateMap->add(keyGenerator.formKey(*control).impl(), nullptr).iterator->value;
        if (!formState)
            formState = std::make_unique<SavedFormState>();
        formState->appendControlState(control->name(), control->type(), control->saveFormControlState());
    }
    return stateMap;
}

Vector<String> FormController::formElementsState() const
{
    std::unique_ptr<SavedFormStateMap> stateMap = createSavedFormStateMap(m_formElementsWithState);
    Vector<String> stateVector;
    stateVector.reserveInitialCapacity(m_formElementsWithState.size() * 4);
    stateVector.append(formStateSignature());
    for (auto& state : *stateMap) {
        stateVector.append(state.key.get());
        state.value->serializeTo(stateVector);
    }
    bool hasOnlySignature = stateVector.size() == 1;
    if (hasOnlySignature)
        stateVector.clear();
    return stateVector;
}

void FormController::setStateForNewFormElements(const Vector<String>& stateVector)
{
    formStatesFromStateVector(stateVector, m_savedFormStateMap);
}

FormControlState FormController::takeStateForFormElement(const HTMLFormControlElementWithState& control)
{
    if (m_savedFormStateMap.isEmpty())
        return FormControlState();
    if (!m_formKeyGenerator)
        m_formKeyGenerator = std::make_unique<FormKeyGenerator>();
    SavedFormStateMap::iterator it = m_savedFormStateMap.find(m_formKeyGenerator->formKey(control).impl());
    if (it == m_savedFormStateMap.end())
        return FormControlState();
    FormControlState state = it->value->takeControlState(control.name(), control.type());
    if (it->value->isEmpty())
        m_savedFormStateMap.remove(it);
    return state;
}

void FormController::formStatesFromStateVector(const Vector<String>& stateVector, SavedFormStateMap& map)
{
    map.clear();

    size_t i = 0;
    if (stateVector.size() < 1 || stateVector[i++] != formStateSignature())
        return;

    while (i + 1 < stateVector.size()) {
        AtomicString formKey = stateVector[i++];
        auto state = SavedFormState::deserialize(stateVector, i);
        if (!state) {
            i = 0;
            break;
        }
        map.add(formKey.impl(), WTFMove(state));
    }
    if (i != stateVector.size())
        map.clear();
}

void FormController::willDeleteForm(HTMLFormElement& form)
{
    if (m_formKeyGenerator)
        m_formKeyGenerator->willDeleteForm(&form);
}

void FormController::restoreControlStateFor(HTMLFormControlElementWithState& control)
{
    // We don't save state of a control with shouldSaveAndRestoreFormControlState()
    // == false. But we need to skip restoring process too because a control in
    // another form might have the same pair of name and type and saved its state.
    if (!control.shouldSaveAndRestoreFormControlState())
        return;
    if (ownerFormForState(control))
        return;
    auto state = takeStateForFormElement(control);
    if (!state.isEmpty())
        control.restoreFormControlState(state);
}

void FormController::restoreControlStateIn(HTMLFormElement& form)
{
    for (auto& element : form.copyAssociatedElementsVector()) {
        if (!is<HTMLFormControlElementWithState>(element.get()))
            continue;
        auto& control = downcast<HTMLFormControlElementWithState>(element.get());
        if (!control.shouldSaveAndRestoreFormControlState())
            continue;
        if (ownerFormForState(control) != &form)
            continue;
        auto state = takeStateForFormElement(control);
        if (!state.isEmpty())
            control.restoreFormControlState(state);
    }
}

bool FormController::hasFormStateToRestore() const
{
    return !m_savedFormStateMap.isEmpty();
}

Vector<String> FormController::referencedFilePaths(const Vector<String>& stateVector)
{
    Vector<String> paths;
    SavedFormStateMap map;
    formStatesFromStateVector(stateVector, map);
    for (auto& state : map.values())
        paths.appendVector(state->referencedFilePaths());
    return paths;
}

void FormController::registerFormElementWithState(HTMLFormControlElementWithState& control)
{
    ASSERT(!m_formElementsWithState.contains(&control));
    m_formElementsWithState.add(&control);
}

void FormController::unregisterFormElementWithState(HTMLFormControlElementWithState& control)
{
    ASSERT(m_formElementsWithState.contains(&control));
    m_formElementsWithState.remove(&control);
}

} // namespace WebCore
