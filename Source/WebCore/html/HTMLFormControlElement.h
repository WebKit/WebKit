/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLFormControlElement_h
#define HTMLFormControlElement_h

#include "FormAssociatedElement.h"
#include "LabelableElement.h"

namespace WebCore {

class FormDataList;
class HTMLFieldSetElement;
class HTMLFormElement;
class HTMLLegendElement;
class ValidationMessage;
class ValidityState;

// HTMLFormControlElement is the default implementation of FormAssociatedElement,
// and form-associated element implementations should use HTMLFormControlElement
// unless there is a special reason.
class HTMLFormControlElement : public LabelableElement, public FormAssociatedElement {
public:
    virtual ~HTMLFormControlElement();

    HTMLFormElement* form() const { return FormAssociatedElement::form(); }

    void willAddAuthorShadowRoot() OVERRIDE;

    String formEnctype() const;
    void setFormEnctype(const String&);
    String formMethod() const;
    void setFormMethod(const String&);
    bool formNoValidate() const;

    void ancestorDisabledStateWasChanged();

    virtual void reset() { }

    virtual bool formControlValueMatchesRenderer() const { return m_valueMatchesRenderer; }
    virtual void setFormControlValueMatchesRenderer(bool b) { m_valueMatchesRenderer = b; }

    virtual bool wasChangedSinceLastFormControlChangeEvent() const;
    virtual void setChangedSinceLastFormControlChangeEvent(bool);

    virtual void dispatchFormControlChangeEvent();
    virtual void dispatchFormControlInputEvent();

    virtual bool disabled() const;

    virtual bool isFocusable() const;
    virtual bool isEnumeratable() const { return false; }

    // Determines whether or not a control will be automatically focused.
    virtual bool autofocus() const;

    bool isRequired() const;

    const AtomicString& type() const { return formControlType(); }

    virtual const AtomicString& formControlType() const OVERRIDE = 0;
    virtual bool isEnabledFormControl() const { return !disabled(); }

    virtual bool canTriggerImplicitSubmission() const { return false; }

    // Override in derived classes to get the encoded name=value pair for submitting.
    // Return true for a successful control (see HTML4-17.13.2).
    virtual bool appendFormData(FormDataList&, bool) { return false; }

    virtual bool isSuccessfulSubmitButton() const { return false; }
    virtual bool isActivatedSubmit() const { return false; }
    virtual void setActivatedSubmit(bool) { }

    virtual bool willValidate() const;
    void updateVisibleValidationMessage();
    void hideVisibleValidationMessage();
    bool checkValidity(Vector<RefPtr<FormAssociatedElement> >* unhandledInvalidControls = 0);
    // This must be called when a validation constraint or control value is changed.
    void setNeedsValidityCheck();
    virtual void setCustomValidity(const String&) OVERRIDE;

    bool readOnly() const { return m_readOnly; }
    bool isDisabledOrReadOnly() const { return disabled() || m_readOnly; }

    bool hasAutofocused() { return m_hasAutofocused; }
    void setAutofocused() { m_hasAutofocused = true; }

    static HTMLFormControlElement* enclosingFormControlElement(Node*);

    using Node::ref;
    using Node::deref;

    virtual void reportMemoryUsage(MemoryObjectInfo*) const OVERRIDE;

protected:
    HTMLFormControlElement(const QualifiedName& tagName, Document*, HTMLFormElement*);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual void requiredAttributeChanged();
    virtual void disabledAttributeChanged();
    virtual void attach();
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void didMoveToNewDocument(Document* oldDocument) OVERRIDE;

    virtual bool supportsFocus() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;

    virtual void didRecalcStyle(StyleChange) OVERRIDE;

    virtual void dispatchBlurEvent(PassRefPtr<Node> newFocusedNode);

    // This must be called any time the result of willValidate() has changed.
    void setNeedsWillValidateCheck();
    virtual bool recalcWillValidate() const;

    bool validationMessageShadowTreeContains(Node*) const;

private:
    virtual void refFormAssociatedElement() { ref(); }
    virtual void derefFormAssociatedElement() { deref(); }

    virtual bool isFormControlElement() const { return true; }

    virtual short tabIndex() const;

    virtual HTMLFormElement* virtualForm() const;
    virtual bool isDefaultButtonForForm() const;
    virtual bool isValidFormControlElement();
    void updateAncestorDisabledState() const;

    OwnPtr<ValidationMessage> m_validationMessage;
    bool m_disabled : 1;
    bool m_readOnly : 1;
    bool m_isRequired : 1;
    bool m_valueMatchesRenderer : 1;

    enum AncestorDisabledState { AncestorDisabledStateUnknown, AncestorDisabledStateEnabled, AncestorDisabledStateDisabled };
    mutable AncestorDisabledState m_ancestorDisabledState;
    enum DataListAncestorState { Unknown, InsideDataList, NotInsideDataList };
    mutable enum DataListAncestorState m_dataListAncestorState;

    // The initial value of m_willValidate depends on the derived class. We can't
    // initialize it with a virtual function in the constructor. m_willValidate
    // is not deterministic as long as m_willValidateInitialized is false.
    mutable bool m_willValidateInitialized: 1;
    mutable bool m_willValidate : 1;

    // Cache of validity()->valid().
    // But "candidate for constraint validation" doesn't affect m_isValid.
    bool m_isValid : 1;

    bool m_wasChangedSinceLastFormControlChangeEvent : 1;

    bool m_hasAutofocused : 1;
};

} // namespace

#endif
