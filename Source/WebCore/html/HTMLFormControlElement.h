/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
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

#pragma once

#include "Autofill.h"
#include "FormAssociatedElement.h"
#include "LabelableElement.h"

#if ENABLE(AUTOCAPITALIZE)
#include "Autocapitalize.h"
#endif

namespace WebCore {

class DOMFormData;
class HTMLFieldSetElement;
class HTMLFormElement;
class HTMLLegendElement;
class ValidationMessage;

// HTMLFormControlElement is the default implementation of FormAssociatedElement,
// and form-associated element implementations should use HTMLFormControlElement
// unless there is a special reason.
class HTMLFormControlElement : public LabelableElement, public FormAssociatedElement {
    WTF_MAKE_ISO_ALLOCATED(HTMLFormControlElement);
public:
    virtual ~HTMLFormControlElement();

    HTMLFormElement* form() const final { return FormAssociatedElement::form(); }

    WEBCORE_EXPORT String formEnctype() const;
    WEBCORE_EXPORT void setFormEnctype(const String&);
    WEBCORE_EXPORT String formMethod() const;
    WEBCORE_EXPORT void setFormMethod(const String&);
    bool formNoValidate() const;
    WEBCORE_EXPORT String formAction() const;
    WEBCORE_EXPORT void setFormAction(const AtomString&);

    void setAncestorDisabled(bool isDisabled);

    virtual void reset() { }

    bool formControlValueMatchesRenderer() const { return m_valueMatchesRenderer; }
    void setFormControlValueMatchesRenderer(bool b) { m_valueMatchesRenderer = b; }

    bool wasChangedSinceLastFormControlChangeEvent() const { return m_wasChangedSinceLastFormControlChangeEvent; }
    void setChangedSinceLastFormControlChangeEvent(bool);

    virtual void dispatchFormControlChangeEvent();
    void dispatchChangeEvent();
    void dispatchFormControlInputEvent();

    bool isDisabledFormControl() const override;

    bool isEnumeratable() const override { return false; }

    bool isRequired() const;

    const AtomString& type() const { return formControlType(); }

    virtual const AtomString& formControlType() const = 0;

    virtual bool canTriggerImplicitSubmission() const { return false; }

    // Override in derived classes to get the encoded name=value pair for submitting.
    // Return true for a successful control (see HTML4-17.13.2).
    bool appendFormData(DOMFormData&, bool) override { return false; }

    virtual bool isSuccessfulSubmitButton() const { return false; }
    virtual bool isActivatedSubmit() const { return false; }
    virtual void setActivatedSubmit(bool) { }

#if ENABLE(AUTOCORRECT)
    WEBCORE_EXPORT bool shouldAutocorrect() const final;
#endif

#if ENABLE(AUTOCAPITALIZE)
    WEBCORE_EXPORT AutocapitalizeType autocapitalizeType() const final;
#endif

    WEBCORE_EXPORT bool willValidate() const final;
    void updateVisibleValidationMessage();
    void hideVisibleValidationMessage();
    WEBCORE_EXPORT bool checkValidity(Vector<RefPtr<HTMLFormControlElement>>* unhandledInvalidControls = nullptr);
    bool reportValidity();
    void focusAndShowValidationMessage();
    bool isShowingValidationMessage() const;
    // This must be called when a validation constraint or control value is changed.
    void updateValidity();
    void setCustomValidity(const String&) override;

    bool isReadOnly() const { return m_isReadOnly; }
    bool isDisabledOrReadOnly() const { return isDisabledFormControl() || m_isReadOnly; }

    bool hasAutofocused() { return m_hasAutofocused; }
    void setAutofocused() { m_hasAutofocused = true; }

    WEBCORE_EXPORT String autocomplete() const;
    WEBCORE_EXPORT void setAutocomplete(const String&);

    AutofillMantle autofillMantle() const;

    WEBCORE_EXPORT AutofillData autofillData() const;

    using Node::ref;
    using Node::deref;

protected:
    HTMLFormControlElement(const QualifiedName& tagName, Document&, HTMLFormElement*);

    bool disabledByAncestorFieldset() const { return m_disabledByAncestorFieldset; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    virtual void disabledAttributeChanged();
    virtual void disabledStateChanged();
    virtual void readOnlyStateChanged();
    virtual void requiredStateChanged();
    void didAttachRenderers() override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void didFinishInsertingNode() override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;
    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;

    bool supportsFocus() const override;
    bool isKeyboardFocusable(KeyboardEvent*) const override;
    bool isMouseFocusable() const override;

    void didRecalcStyle(Style::Change) override;

    void dispatchBlurEvent(RefPtr<Element>&& newFocusedElement) override;

    // This must be called any time the result of willValidate() has changed.
    void setNeedsWillValidateCheck();
    virtual bool computeWillValidate() const;

    bool validationMessageShadowTreeContains(const Node&) const;

    void willChangeForm() override;
    void didChangeForm() override;

private:
    void refFormAssociatedElement() override { ref(); }
    void derefFormAssociatedElement() override { deref(); }

    bool matchesValidPseudoClass() const override;
    bool matchesInvalidPseudoClass() const override;

    bool isFormControlElement() const final { return true; }

    bool isValidFormControlElement() const;

    bool computeIsDisabledByFieldsetAncestor() const;

    HTMLElement& asHTMLElement() final { return *this; }
    const HTMLFormControlElement& asHTMLElement() const final { return *this; }
    FormNamedItem* asFormNamedItem() final { return this; }
    FormAssociatedElement* asFormAssociatedElement() final { return this; }

    bool needsMouseFocusableQuirk() const;

    std::unique_ptr<ValidationMessage> m_validationMessage;
    unsigned m_disabled : 1;
    unsigned m_isReadOnly : 1;
    unsigned m_isRequired : 1;
    unsigned m_valueMatchesRenderer : 1;
    unsigned m_disabledByAncestorFieldset : 1;

    enum DataListAncestorState { Unknown, InsideDataList, NotInsideDataList };
    mutable unsigned m_dataListAncestorState : 2;

    // The initial value of m_willValidate depends on the derived class. We can't
    // initialize it with a virtual function in the constructor. m_willValidate
    // is not deterministic as long as m_willValidateInitialized is false.
    mutable bool m_willValidateInitialized: 1;
    mutable bool m_willValidate : 1;

    // Cache of validity()->valid().
    // But "candidate for constraint validation" doesn't affect m_isValid.
    unsigned m_isValid : 1;

    unsigned m_wasChangedSinceLastFormControlChangeEvent : 1;

    unsigned m_hasAutofocused : 1;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLFormControlElement)
    static bool isType(const WebCore::Element& element) { return element.isFormControlElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::Element>(node) && isType(downcast<WebCore::Element>(node)); }
    static bool isType(const WebCore::FormAssociatedElement& element) { return element.isFormControlElement(); }
SPECIALIZE_TYPE_TRAITS_END()
