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

#pragma once

#include "FormController.h"
#include "FormListedElement.h"
#include "HTMLElement.h"
#include "ValidationMessage.h"
#include <wtf/TriState.h>

namespace WebCore {

class HTMLMaybeFormAssociatedCustomElement;

class ValidatedFormListedElement : public FormListedElement {
    WTF_MAKE_NONCOPYABLE(ValidatedFormListedElement);
    WTF_MAKE_FAST_ALLOCATED;
    friend class DelayedUpdateValidityScope;
    friend class HTMLMaybeFormAssociatedCustomElement;
public:
    ValidatedFormListedElement(HTMLFormElement*);
    virtual ~ValidatedFormListedElement();

    // "willValidate" means "is a candidate for constraint validation".
    WEBCORE_EXPORT bool willValidate() const override;
    void updateVisibleValidationMessage(Ref<HTMLElement> validationAnchor);
    void hideVisibleValidationMessage();
    WEBCORE_EXPORT bool checkValidity(Vector<RefPtr<ValidatedFormListedElement>>* unhandledInvalidControls = nullptr);
    bool reportValidity();
    RefPtr<HTMLElement> focusableValidationAnchorElement();
    void reportNonFocusableControlError();
    WEBCORE_EXPORT void focusAndShowValidationMessage(Ref<HTMLElement> validationAnchor);
    bool isShowingValidationMessage() const;
    WEBCORE_EXPORT bool isFocusingWithValidationMessage() const;
    // This must be called when a validation constraint or control value is changed.
    void updateValidity();
    WEBCORE_EXPORT void setCustomValidity(const String&) override;

    void setDisabledByAncestorFieldset(bool isDisabled);
    virtual void reset() { }

    virtual bool supportsReadOnly() const { return false; }
    bool isDisabled() const { return m_disabled || m_disabledByAncestorFieldset; }
    bool isReadOnly() const { return supportsReadOnly() && m_hasReadOnlyAttribute; }
    bool isMutable() const { return !isDisabled() && !isReadOnly(); }

    // This must be called any time the result of willValidate() has changed.
    bool isValidFormControlElement() const { return m_isValid; }

    bool isEnumeratable() const override { return false; }

    bool wasInteractedWithSinceLastFormSubmitEvent() const { return m_wasInteractedWithSinceLastFormSubmitEvent; }
    void setInteractedWithSinceLastFormSubmitEvent(bool);

    bool matchesValidPseudoClass() const;
    bool matchesInvalidPseudoClass() const;
    bool matchesUserInvalidPseudoClass() const;
    bool matchesUserValidPseudoClass() const;

    bool isCandidateForSavingAndRestoringState() const;
    virtual bool shouldAutocomplete() const;
    virtual bool shouldSaveAndRestoreFormControlState() const { return false; }
    virtual FormControlState saveFormControlState() const;
    virtual void restoreFormControlState(const FormControlState&) { } // Called only if state is not empty.
    virtual const AtomString& formControlType() const = 0;

protected:
    bool hasDisabledAttribute() const { return m_disabled; }
    virtual bool computeWillValidate() const;
    virtual bool readOnlyBarsFromConstraintValidation() const { return false; }
    void updateWillValidateAndValidity();
    bool disabledByAncestorFieldset() const { return m_disabledByAncestorFieldset; }

    bool validationMessageShadowTreeContains(const Node&) const;

    void insertedIntoAncestor(Node::InsertionType, ContainerNode&);
    void didFinishInsertingNode();
    void removedFromAncestor(Node::RemovalType, ContainerNode&);
    void parseAttribute(const QualifiedName&, const AtomString&);
    void parseDisabledAttribute(const AtomString&);
    void parseReadOnlyAttribute(const AtomString&);

    virtual void disabledStateChanged();
    virtual void readOnlyStateChanged();

    void willChangeForm() override;
    void didChangeForm() override;
    void formWillBeDestroyed() final;
    bool belongsToFormThatIsBeingDestroyed() const { return m_belongsToFormThatIsBeingDestroyed; }

    void setDataListAncestorState(TriState);
    void syncWithFieldsetAncestors(ContainerNode* insertionNode);
    void restoreFormControlStateIfNecessary();

private:
    bool computeIsDisabledByFieldsetAncestor() const;
    void setDisabledInternal(bool disabled, bool disabledByAncestorFieldset);
    virtual HTMLElement* validationAnchorElement() = 0;

    void startDelayingUpdateValidity() { ++m_delayedUpdateValidityCount; }
    void endDelayingUpdateValidity();

    std::unique_ptr<ValidationMessage> m_validationMessage;

    // Cache of validity()->valid().
    // But "candidate for constraint validation" doesn't affect isValid.
    bool m_isValid : 1 { true };

    // The initial value of willValidate depends on the derived class.
    // We can't initialize it with a virtual function in the constructor.
    // willValidate is not deterministic as long as willValidateInitialized is false.
    mutable bool m_willValidate : 1 { true };
    mutable bool m_willValidateInitialized : 1 { false };

    bool m_disabled : 1 { false };
    bool m_disabledByAncestorFieldset : 1 { false };

    bool m_hasReadOnlyAttribute : 1 { false };
    bool m_wasInteractedWithSinceLastFormSubmitEvent : 1 { false };
    bool m_belongsToFormThatIsBeingDestroyed : 1 { false };
    bool m_isFocusingWithValidationMessage { false };

    mutable TriState m_isInsideDataList : 2 { TriState::Indeterminate };

    unsigned m_delayedUpdateValidityCount { 0 };
};

class DelayedUpdateValidityScope {
public:
    DelayedUpdateValidityScope(ValidatedFormListedElement& element)
        : m_element { element }
    {
        m_element.startDelayingUpdateValidity();
    }
    
    ~DelayedUpdateValidityScope()
    {
        m_element.endDelayingUpdateValidity();
    }

private:
    ValidatedFormListedElement& m_element;
};

} // namespace WebCore
