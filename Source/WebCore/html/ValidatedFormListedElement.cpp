/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ValidatedFormListedElement.h"

#include "ControlStates.h"
#include "ElementAncestorIterator.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FormAssociatedElement.h"
#include "FormController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLDataListElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLLegendElement.h"
#include "HTMLParserIdioms.h"
#include "PseudoClassChangeInvalidation.h"
#include "ScriptDisallowedScope.h"
#include "ValidationMessage.h"
#include <wtf/Ref.h>
#include <wtf/SetForScope.h>
#include <wtf/Vector.h>

namespace WebCore {

using namespace HTMLNames;

ValidatedFormListedElement::ValidatedFormListedElement(HTMLFormElement* form)
    : FormListedElement { form }
{
    if (supportsReadOnly())
        ASSERT(readOnlyBarsFromConstraintValidation());
}

ValidatedFormListedElement::~ValidatedFormListedElement() = default;

bool ValidatedFormListedElement::willValidate() const
{
    if (!m_willValidateInitialized || m_isInsideDataList == TriState::Indeterminate) {
        m_willValidateInitialized = true;
        bool newWillValidate = computeWillValidate();
        if (m_willValidate != newWillValidate)
            m_willValidate = newWillValidate;
    } else {
        // If the following assertion fails, updateWillValidateAndValidity() is not
        // called correctly when something which changes computeWillValidate() result
        // is updated.
        ASSERT(m_willValidate == computeWillValidate());
    }
    return m_willValidate;
}

bool ValidatedFormListedElement::computeWillValidate() const
{
    if (m_isInsideDataList == TriState::Indeterminate) {
#if ENABLE(DATALIST_ELEMENT)
        const HTMLElement& element = asHTMLElement();
        m_isInsideDataList = triState(element.document().hasDataListElements() && ancestorsOfType<HTMLDataListElement>(element).first());
#else
        m_isInsideDataList = TriState::False;
#endif
    }
    // readonly bars constraint validation for *all* <input> elements, regardless of the <input> type, for compat reasons.
    return m_isInsideDataList == TriState::False && !isDisabled() && !(m_hasReadOnlyAttribute && readOnlyBarsFromConstraintValidation());
}

void ValidatedFormListedElement::updateVisibleValidationMessage(Ref<HTMLElement> validationAnchor)
{
    HTMLElement& element = asHTMLElement();
    if (!element.document().page())
        return;
    String message;
    if (element.renderer() && willValidate())
        message = validationMessage().stripWhiteSpace();
    if (!m_validationMessage)
        m_validationMessage = makeUnique<ValidationMessage>(validationAnchor);
    m_validationMessage->updateValidationMessage(validationAnchor, message);
}

void ValidatedFormListedElement::hideVisibleValidationMessage()
{
    if (m_validationMessage)
        m_validationMessage->requestToHideMessage();
}

bool ValidatedFormListedElement::checkValidity(Vector<RefPtr<ValidatedFormListedElement>>* unhandledInvalidControls)
{
    if (!willValidate() || isValidFormControlElement())
        return true;
    // An event handler can deref this object.
    HTMLElement& element = asHTMLElement();
    Ref protectedThis { element };
    Ref originalDocument { element.document() };
    auto event = Event::create(eventNames().invalidEvent, Event::CanBubble::No, Event::IsCancelable::Yes);
    element.dispatchEvent(event);
    if (!event->defaultPrevented() && unhandledInvalidControls && element.isConnected() && originalDocument.ptr() == &element.document())
        unhandledInvalidControls->append(this);
    return false;
}

bool ValidatedFormListedElement::reportValidity()
{
    Vector<RefPtr<ValidatedFormListedElement>> elements;
    if (checkValidity(&elements))
        return true;

    if (elements.isEmpty())
        return false;

    // Needs to update layout now because we'd like to call isFocusable(),
    // which has !renderer()->needsLayout() assertion.
    asHTMLElement().document().updateLayoutIgnorePendingStylesheets();
    if (auto validationAnchor = focusableValidationAnchorElement())
        focusAndShowValidationMessage(validationAnchor.releaseNonNull());
    else
        reportNonFocusableControlError();

    return false;
}

RefPtr<HTMLElement> ValidatedFormListedElement::focusableValidationAnchorElement()
{
    if (RefPtr validationAnchor = validationAnchorElement()) {
        if (validationAnchor->isConnected() && validationAnchor->isFocusable())
            return validationAnchor;
    }
    return nullptr;
}

void ValidatedFormListedElement::focusAndShowValidationMessage(Ref<HTMLElement> validationAnchor)
{
    Ref protectedThis { *this };
    SetForScope isFocusingWithValidationMessageScope(m_isFocusingWithValidationMessage, true);

    // Calling focus() will scroll the element into view.
    validationAnchor->focus();

    // focus() will scroll the element into view and this scroll may happen asynchronously.
    // Because scrolling the view hides the validation message, we need to show the validation
    // message asynchronously as well.
    callOnMainThread([this, protectedThis, validationAnchor] {
        updateVisibleValidationMessage(validationAnchor);
    });
}

void ValidatedFormListedElement::reportNonFocusableControlError()
{
    auto& document = asHTMLElement().document();
    if (document.frame()) {
        auto message = makeString("An invalid form control with name='", name(), "' is not focusable.");
        document.addConsoleMessage(MessageSource::Rendering, MessageLevel::Error, message);
    }
}

bool ValidatedFormListedElement::isShowingValidationMessage() const
{
    return m_validationMessage && m_validationMessage->isVisible();
}

bool ValidatedFormListedElement::validationMessageShadowTreeContains(const Node& node) const
{
    return m_validationMessage && m_validationMessage->shadowTreeContains(node);
}

bool ValidatedFormListedElement::isFocusingWithValidationMessage() const
{
    return m_isFocusingWithValidationMessage;
}

void ValidatedFormListedElement::setDisabledByAncestorFieldset(bool isDisabled)
{
    ASSERT(computeIsDisabledByFieldsetAncestor() == isDisabled);
    if (m_disabledByAncestorFieldset == isDisabled)
        return;

    Style::PseudoClassChangeInvalidation disabledInvalidation(asHTMLElement(), { { CSSSelector::PseudoClassDisabled, isDisabled }, { CSSSelector::PseudoClassEnabled, !isDisabled } });

    m_disabledByAncestorFieldset = isDisabled;
    disabledStateChanged();
}

static void addInvalidElementToAncestorFromInsertionPoint(const HTMLElement& element, ContainerNode* insertionPoint)
{
    if (!is<Element>(insertionPoint))
        return;

    for (auto& ancestor : lineageOfType<HTMLFieldSetElement>(downcast<Element>(*insertionPoint)))
        ancestor.addInvalidDescendant(element);
}

static void removeInvalidElementToAncestorFromInsertionPoint(const HTMLElement& element, ContainerNode* insertionPoint)
{
    if (!is<Element>(insertionPoint))
        return;

    for (auto& ancestor : lineageOfType<HTMLFieldSetElement>(downcast<Element>(*insertionPoint)))
        ancestor.removeInvalidDescendant(element);
}

void ValidatedFormListedElement::updateValidity()
{
    if (m_delayedUpdateValidityCount)
        return;

    bool willValidate = this->willValidate();
    bool newIsValid = this->computeValidity();

    if (newIsValid != m_isValid) {
        HTMLElement& element = asHTMLElement();
        Style::PseudoClassChangeInvalidation styleInvalidation(element, {
            { CSSSelector::PseudoClassValid, newIsValid },
            { CSSSelector::PseudoClassInvalid, !newIsValid },
            { CSSSelector::PseudoClassUserValid, m_wasInteractedWithSinceLastFormSubmitEvent && newIsValid },
            { CSSSelector::PseudoClassUserInvalid, m_wasInteractedWithSinceLastFormSubmitEvent && !newIsValid },
        });

        m_isValid = newIsValid;

        if (willValidate) {
            if (!newIsValid) {
                if (element.isConnected())
                    addInvalidElementToAncestorFromInsertionPoint(element, element.parentNode());
                if (auto* form = this->form())
                    form->addInvalidFormControl(element);
            } else {
                if (element.isConnected())
                    removeInvalidElementToAncestorFromInsertionPoint(element, element.parentNode());
                if (auto* form = this->form())
                    form->removeInvalidFormControlIfNeeded(element);
            }
        }
    }

    // Updates only if this control already has a validation message.
    if (isShowingValidationMessage()) {
        // Calls updateVisibleValidationMessage() even if m_isValid is not
        // changed because a validation message can be changed.
        if (auto validationAnchor = focusableValidationAnchorElement())
            updateVisibleValidationMessage(validationAnchor.releaseNonNull());
    }
}

void ValidatedFormListedElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == disabledAttr && asHTMLElement().canBeActuallyDisabled())
        parseDisabledAttribute(value);
    else if (name == readonlyAttr && readOnlyBarsFromConstraintValidation())
        parseReadOnlyAttribute(value);
    else
        FormListedElement::parseAttribute(name, value);
}

void ValidatedFormListedElement::parseDisabledAttribute(const AtomString& value)
{
    ASSERT(asHTMLElement().canBeActuallyDisabled());

    bool newDisabled = !value.isNull();
    if (m_disabled != newDisabled) {
        Style::PseudoClassChangeInvalidation disabledInvalidation(asHTMLElement(), { { CSSSelector::PseudoClassDisabled, newDisabled }, { CSSSelector::PseudoClassEnabled, !newDisabled } });
        m_disabled = newDisabled;
        disabledAttributeChanged();
    }
}

void ValidatedFormListedElement::parseReadOnlyAttribute(const AtomString& value)
{
    ASSERT(readOnlyBarsFromConstraintValidation());

    bool newHasReadOnlyAttribute = !value.isNull();
    if (m_hasReadOnlyAttribute != newHasReadOnlyAttribute) {
        bool newMatchesReadWrite = supportsReadOnly() && !newHasReadOnlyAttribute;
        Style::PseudoClassChangeInvalidation readWriteInvalidation(asHTMLElement(), { { CSSSelector::PseudoClassReadWrite, newMatchesReadWrite }, { CSSSelector::PseudoClassReadOnly, !newMatchesReadWrite } });
        m_hasReadOnlyAttribute = newHasReadOnlyAttribute;
        readOnlyStateChanged();
    }
}

void ValidatedFormListedElement::disabledAttributeChanged()
{
    disabledStateChanged();
}

void ValidatedFormListedElement::insertedIntoAncestor(Node::InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    m_isInsideDataList = TriState::Indeterminate;
    updateWillValidateAndValidity();
    syncWithFieldsetAncestors(&parentOfInsertedTree);

    FormListedElement::elementInsertedIntoAncestor(asHTMLElement(), insertionType);
}

void ValidatedFormListedElement::setDataListAncestorState(TriState isInsideDataList)
{
    m_isInsideDataList = isInsideDataList;
}

void ValidatedFormListedElement::syncWithFieldsetAncestors(ContainerNode* insertionPoint)
{
    if (matchesInvalidPseudoClass())
        addInvalidElementToAncestorFromInsertionPoint(asHTMLElement(), insertionPoint);
    if (asHTMLElement().document().hasDisabledFieldsetElement())
        setDisabledByAncestorFieldset(computeIsDisabledByFieldsetAncestor());
}

void ValidatedFormListedElement::removedFromAncestor(Node::RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement& element = asHTMLElement();
    bool wasMatchingInvalidPseudoClass = matchesInvalidPseudoClass();

    m_validationMessage = nullptr;
    if (m_disabledByAncestorFieldset)
        setDisabledByAncestorFieldset(computeIsDisabledByFieldsetAncestor());

    bool wasInsideDataList = m_isInsideDataList == TriState::True;
    if (wasInsideDataList)
        m_isInsideDataList = TriState::Indeterminate;

    FormListedElement::elementRemovedFromAncestor(element, removalType);

    if (wasMatchingInvalidPseudoClass)
        removeInvalidElementToAncestorFromInsertionPoint(element, &oldParentOfRemovedTree);

    if (wasInsideDataList)
        updateWillValidateAndValidity();
}

bool ValidatedFormListedElement::computeIsDisabledByFieldsetAncestor() const
{
    RefPtr<const Element> previousAncestor;
    for (auto& ancestor : ancestorsOfType<Element>(asHTMLElement())) {
        if (is<HTMLFieldSetElement>(ancestor) && ancestor.hasAttributeWithoutSynchronization(disabledAttr)) {
            bool isInFirstLegend = is<HTMLLegendElement>(previousAncestor) && previousAncestor == downcast<HTMLFieldSetElement>(ancestor).legend();
            return !isInFirstLegend;
        }
        previousAncestor = &ancestor;
    }
    return false;
}

void ValidatedFormListedElement::willChangeForm()
{
    if (auto* form = this->form())
        form->removeInvalidFormControlIfNeeded(asHTMLElement());
    FormListedElement::willChangeForm();
}

void ValidatedFormListedElement::didChangeForm()
{
    ScriptDisallowedScope::InMainThread scriptDisallowedScope;

    FormListedElement::didChangeForm();
    if (auto* form = this->form()) {
        if (m_willValidateInitialized && m_willValidate && !isValidFormControlElement())
            form->addInvalidFormControl(asHTMLElement());
    }
}

void ValidatedFormListedElement::disabledStateChanged()
{
    updateWillValidateAndValidity();
}

void ValidatedFormListedElement::readOnlyStateChanged()
{
    updateWillValidateAndValidity();
}

void ValidatedFormListedElement::updateWillValidateAndValidity()
{
    // We need to recalculate willValidate immediately because willValidate change can causes style change.
    bool newWillValidate = computeWillValidate();
    if (m_willValidateInitialized && m_willValidate == newWillValidate)
        return;

    bool wasValid = m_isValid;

    m_willValidateInitialized = true;
    m_willValidate = newWillValidate;

    updateValidity();

    if (!m_willValidate && !wasValid) {
        HTMLElement& element = asHTMLElement();
        removeInvalidElementToAncestorFromInsertionPoint(element, element.parentNode());
        if (RefPtr form = this->form())
            form->removeInvalidFormControlIfNeeded(element);
    }

    if (!m_willValidate)
        hideVisibleValidationMessage();
}

void ValidatedFormListedElement::didFinishInsertingNode()
{
    resetFormOwner();
}

void ValidatedFormListedElement::setCustomValidity(const String& error)
{
    FormListedElement::setCustomValidity(error);
    updateValidity();
}

void ValidatedFormListedElement::endDelayingUpdateValidity()
{
    ASSERT(m_delayedUpdateValidityCount);
    if (!--m_delayedUpdateValidityCount)
        updateValidity();
}

bool ValidatedFormListedElement::isCandidateForSavingAndRestoringState() const
{
    // We don't save/restore control state in a form with autocomplete=off.
    return shouldSaveAndRestoreFormControlState() && asHTMLElement().isConnected() && shouldAutocomplete();
}

bool ValidatedFormListedElement::shouldAutocomplete() const
{
    if (!form())
        return true;
    return form()->shouldAutocomplete();
}

FormControlState ValidatedFormListedElement::saveFormControlState() const
{
    return { };
}

void ValidatedFormListedElement::restoreFormControlStateIfNecessary()
{
    asHTMLElement().document().formController().restoreControlStateFor(*this);
}

bool ValidatedFormListedElement::matchesValidPseudoClass() const
{
    return willValidate() && isValidFormControlElement();
}

bool ValidatedFormListedElement::matchesInvalidPseudoClass() const
{
    return willValidate() && !isValidFormControlElement();
}

bool ValidatedFormListedElement::matchesUserInvalidPseudoClass() const
{
    return m_wasInteractedWithSinceLastFormSubmitEvent && matchesInvalidPseudoClass();
}

bool ValidatedFormListedElement::matchesUserValidPseudoClass() const
{
    return m_wasInteractedWithSinceLastFormSubmitEvent && matchesValidPseudoClass();
}

void ValidatedFormListedElement::setInteractedWithSinceLastFormSubmitEvent(bool interactedWith)
{
    if (m_wasInteractedWithSinceLastFormSubmitEvent == interactedWith)
        return;

    Style::PseudoClassChangeInvalidation styleInvalidation(asHTMLElement(), {
        { CSSSelector::PseudoClassUserValid, interactedWith && matchesValidPseudoClass() },
        { CSSSelector::PseudoClassUserInvalid, interactedWith && matchesInvalidPseudoClass() },
    });

    m_wasInteractedWithSinceLastFormSubmitEvent = interactedWith;
}

} // namespace Webcore
