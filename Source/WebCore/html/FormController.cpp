/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
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

#include "FileChooser.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "HTMLMaybeFormAssociatedCustomElement.h"
#include "ScriptDisallowedScope.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakHashMap.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FormController);

HTMLFormElement* FormController::ownerForm(const FormListedElement& control)
{
    // Assume controls with form attribute have no owners because we restore
    // state during parsing and form owners of such controls might be
    // indeterminate.
    return control.asHTMLElement().hasAttributeWithoutSynchronization(HTMLNames::formAttr) ? nullptr : control.form();
}

struct AtomStringVectorReader {
    const Vector<RefPtr<AtomStringImpl>>& vector;
    size_t index { 0 };

    NODELETE const RefPtr<AtomStringImpl>& consumeString();
    Vector<RefPtr<AtomStringImpl>> consumeSubvector(size_t subvectorSize);
};

NODELETE const RefPtr<AtomStringImpl>& AtomStringVectorReader::consumeString()
{
    if (index == vector.size())
        return nullAtom();
    return vector[index++];
}

Vector<RefPtr<AtomStringImpl>> AtomStringVectorReader::consumeSubvector(size_t subvectorSize)
{
    if (subvectorSize > vector.size() - index)
        return { };
    auto subvectorIndex = index;
    index += subvectorSize;
    return vector.subvector(subvectorIndex, subvectorSize);
}

// ----------------------------------------------------------------------------

// Serialized form of FormControlState:
//  (',' means strings around it are separated in stateVector.)
//
// SerializedControlState ::= SkipState | RestoreState
// SkipState ::= '0'
// RestoreState ::= UnsignedNumber, ControlValue+
// UnsignedNumber ::= [0-9]+
// ControlValue ::= arbitrary string
//
// The UnsignedNumber in RestoreState is the length of the sequence of ControlValues.

static void appendSerializedFormControlState(Vector<RefPtr<AtomStringImpl>>& vector, const FormControlState& state)
{
    vector.append(AtomStringImpl::number(state.size()));
    for (auto& value : state)
        vector.append(!value ? emptyAtom() : value);
}

static std::optional<FormControlState> consumeSerializedFormControlState(AtomStringVectorReader& reader)
{
    auto sizeString = reader.consumeString();
    if (!sizeString)
        return std::nullopt;
    return reader.consumeSubvector(parseInteger<size_t>(sizeString).value_or(0));
}

// ----------------------------------------------------------------------------

class FormController::SavedFormState {
public:
    static SavedFormState consumeSerializedState(AtomStringVectorReader&);

    NODELETE bool isEmpty() const { return m_map.isEmpty(); }

    using FormElementKey = std::pair<RefPtr<AtomStringImpl>, RefPtr<AtomStringImpl>>;
    FormControlState takeControlState(const FormElementKey&);

    void appendReferencedFilePaths(Vector<RefPtr<StringImpl>>&) const;

private:
    HashMap<FormElementKey, Deque<FormControlState>> m_map;
};

FormController::SavedFormState FormController::SavedFormState::consumeSerializedState(AtomStringVectorReader& reader)
{
    auto isNotFormControlTypeCharacter = [](char16_t character) {
        return !(character == '-' || isASCIILower(character));
    };

    SavedFormState result;
    auto count = parseInteger<size_t>(reader.consumeString()).value_or(0);
    while (count--) {
        auto& name = reader.consumeString();
        auto& type = reader.consumeString();
        if (WTF::isEmpty(type) || StringView { type }.contains(isNotFormControlTypeCharacter))
            return { };
        auto state = consumeSerializedFormControlState(reader);
        if (!state)
            return { };
        result.m_map.add({ name, type }, Deque<FormControlState> { }).iterator->value.append(WTF::move(*state));
    }
    return result;
}

FormControlState FormController::SavedFormState::takeControlState(const FormElementKey& key)
{
    auto iterator = m_map.find(key);
    if (iterator == m_map.end())
        return { };
    auto state = iterator->value.takeFirst();
    if (iterator->value.isEmpty())
        m_map.remove(iterator);
    return state;
}

void FormController::SavedFormState::appendReferencedFilePaths(Vector<RefPtr<StringImpl>>& vector) const
{
    for (auto& element : m_map) {
        if (!equal(element.key.second, "file"_s)) // type
            continue;
        for (auto& state : element.value) {
            for (auto& file : HTMLInputElement::filesFromFileInputFormControlState(state))
                vector.append(file.path);
        }
    }
}

// ----------------------------------------------------------------------------


class FormController::FormKeyGenerator {
    typedef FormController::FormKeyGenerator FormControllerFormKeyGenerator;

    WTF_MAKE_TZONE_ALLOCATED(FormKeyGenerator);
    WTF_MAKE_NONCOPYABLE(FormKeyGenerator);

public:
    FormKeyGenerator() = default;
    RefPtr<StringImpl> formKey(const ValidatedFormListedElement&);
    void willDeleteForm(HTMLFormElement&);

private:
    WeakHashMap<HTMLFormElement, RefPtr<StringImpl>, WeakPtrImplWithEventTargetData> m_formToKeyMap;
    HashMap<RefPtr<StringImpl>, unsigned> m_formSignatureToNextIndexMap;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(FormController::FormKeyGenerator);

static bool shouldBeUsedForFormSignature(const Element& element)
{
    if (auto* formControl = dynamicDowncast<HTMLFormControlElement>(element))
        return formControl->shouldSaveAndRestoreFormControlState();
    if (auto* customElement = dynamicDowncast<HTMLMaybeFormAssociatedCustomElement>(element))
        return customElement->hasFormAssociatedInterface() || element.isCustomElementUpgradeCandidate();
    return false;
}

static RefPtr<StringImpl> formSignature(const HTMLFormElement& form)
{
    StringBuilder builder;

    // Remove the query part because it might contain volatile parameters such as a session key.
    // FIXME: But leave the fragment identifier? Perhaps we should switch to removeQueryAndFragmentIdentifier.

    URL actionURL = form.getURLAttribute(HTMLNames::actionAttr);
    actionURL.setQuery({ });
    builder.append(actionURL.string());

    // Two named controls seems to be enough to distinguish similar but different forms.
    constexpr unsigned maxNamedControlsToBeRecorded = 2;

    ScriptDisallowedScope::InMainThread scriptDisallowedScope;
    unsigned count = 0;
    builder.append(" ["_s);
    for (Ref element : descendantsOfType<Element>(form)) {
        if (!shouldBeUsedForFormSignature(element.get()) || element->hasAttributeWithoutSynchronization(HTMLNames::formAttr))
            continue;
        auto& name = element->getNameAttribute();
        if (!name || isEmpty(name))
            continue;
        builder.append(name, ' ');
        if (++count >= maxNamedControlsToBeRecorded)
            break;
    }
    builder.append(']');

    return builder.toString();
}

RefPtr<StringImpl> FormController::FormKeyGenerator::formKey(const ValidatedFormListedElement& control)
{
    RefPtr form = ownerForm(control);
    if (!form) {
        static MainThreadNeverDestroyed<RefPtr<StringImpl>> formKeyForNoOwner(MAKE_STATIC_STRING_IMPL("No owner"));
        return formKeyForNoOwner;
    }
    return m_formToKeyMap.ensure(*form, [this, form] {
        auto signature = formSignature(*form);
        auto nextIndex = m_formSignatureToNextIndexMap.add(signature, 0).iterator->value++;
        return makeString(signature, " #"_s, nextIndex);
    }).iterator->value;
}

void FormController::FormKeyGenerator::willDeleteForm(HTMLFormElement& form)
{
    m_formToKeyMap.remove(form);
}

// ----------------------------------------------------------------------------

FormController::FormController() = default;

FormController::~FormController() = default;

static RefPtr<StringImpl> formStateSignature()
{
    // In the legacy version of serialized state, the first item was a name attribute
    // value of a form control. The following string literal contains some characters
    // which are rarely used for name attribute values so it won't match.
    static MainThreadNeverDestroyed<RefPtr<StringImpl>> signature(MAKE_STATIC_STRING_IMPL("\n\r?% WebKit serialized form state version 8 \n\r=&"));
    return signature;
}

Vector<RefPtr<AtomStringImpl>> FormController::formElementsState(const Document& document) const
{
    HashMap<RefPtr<AtomStringImpl>, Vector<Ref<const ValidatedFormListedElement>>> formKeyToControlsMap;

    {
        // FIXME: We should be saving the state of form controls in shadow trees, too.
        FormKeyGenerator keyGenerator;
        for (Ref element : descendantsOfType<Element>(document)) {
            RefPtr control = const_cast<Element&>(element.get()).asValidatedFormListedElement();
            if (!control || !control->isCandidateForSavingAndRestoringState())
                continue;

            RefPtr<AtomStringImpl> formKey { AtomStringImpl::add(keyGenerator.formKey(*control)) };
            auto& vector = formKeyToControlsMap.ensure(formKey, [] {
                return Vector<Ref<const ValidatedFormListedElement>> { };
            }).iterator->value;
            vector.append(control.releaseNonNull());
        }
    }

    if (formKeyToControlsMap.isEmpty())
        return { };

    Vector<RefPtr<AtomStringImpl>> stateVector;
    stateVector.append(AtomStringImpl::add(formStateSignature()));

    for (const auto& entry : formKeyToControlsMap) {
        stateVector.append(entry.key);
        stateVector.append(AtomStringImpl::number(entry.value.size()));

        for (const auto& control : entry.value) {
            stateVector.append(control->name());
            stateVector.append(control->formControlType());
            appendSerializedFormControlState(stateVector, control->saveFormControlState());
        }
    }

    stateVector.shrinkToFit();
    return stateVector;
}

void FormController::setStateForNewFormElements(const Vector<RefPtr<AtomStringImpl>>& stateVector)
{
    m_savedFormStateMap = parseStateVector(stateVector);
}

FormControlState FormController::takeStateForFormElement(const ValidatedFormListedElement& control)
{
    if (m_savedFormStateMap.isEmpty())
        return { };
    if (!m_formKeyGenerator)
        lazyInitialize(m_formKeyGenerator, makeUnique<FormKeyGenerator>());
    auto iterator = m_savedFormStateMap.find(m_formKeyGenerator->formKey(control));
    if (iterator == m_savedFormStateMap.end())
        return { };
    auto state = iterator->value.takeControlState({ control.name(), control.formControlType() });
    if (iterator->value.isEmpty())
        m_savedFormStateMap.remove(iterator);
    return state;
}

FormController::SavedFormStateMap FormController::parseStateVector(const Vector<RefPtr<AtomStringImpl>>& stateVector)
{
    AtomStringVectorReader reader { stateVector };

    if (!equal(reader.consumeString(), formStateSignature()))
        return { };

    SavedFormStateMap map;
    while (true) {
        auto formKey = reader.consumeString();
        if (!formKey)
            return map;
        auto state = SavedFormState::consumeSerializedState(reader);
        if (state.isEmpty())
            return { };
        map.add(formKey, WTF::move(state));
    }
}

void FormController::willDeleteForm(HTMLFormElement& form)
{
    if (m_formKeyGenerator)
        m_formKeyGenerator->willDeleteForm(form);
}

void FormController::restoreControlStateFor(ValidatedFormListedElement& control)
{
    // We don't save state of a control when shouldSaveAndRestoreFormControlState()
    // is false. But we need to skip restoring process too because a control in
    // another form might have the same pair of name and type and saved its state.
    if (!control.isCandidateForSavingAndRestoringState())
        return;
    auto state = takeStateForFormElement(control);
    if (!state.isEmpty())
        control.restoreFormControlState(state);
}

void FormController::restoreControlStateIn(HTMLFormElement& form)
{
    for (Ref element : form.copyValidatedListedElementsVector()) {
        if (!element->isCandidateForSavingAndRestoringState() || ownerForm(element) != &form)
            continue;
        auto state = takeStateForFormElement(element);
        if (!state.isEmpty())
            element->restoreFormControlState(state);
    }
}

bool FormController::hasFormStateToRestore() const
{
    return !m_savedFormStateMap.isEmpty();
}

Vector<RefPtr<StringImpl>> FormController::referencedFilePaths(const Vector<RefPtr<AtomStringImpl>>& stateVector)
{
    Vector<RefPtr<StringImpl>> paths;
    auto parsedState = parseStateVector(stateVector);
    for (auto& state : parsedState.values())
        state.appendReferencedFilePaths(paths);
    return paths;
}

} // namespace WebCore
