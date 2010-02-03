/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "HTMLElement.h"

namespace WebCore {

class FormDataList;
class HTMLFormElement;
class RenderTextControl;
class ValidityState;
class VisibleSelection;

class HTMLFormControlElement : public HTMLElement {
public:
    HTMLFormControlElement(const QualifiedName& tagName, Document*, HTMLFormElement*);
    virtual ~HTMLFormControlElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    HTMLFormElement* form() const { return m_form; }
    ValidityState* validity();

    bool formNoValidate() const;
    void setFormNoValidate(bool);

    virtual bool isTextFormControl() const { return false; }
    virtual bool isEnabledFormControl() const { return !disabled(); }

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void attach();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);

    virtual void reset() {}

    virtual bool formControlValueMatchesRenderer() const { return m_valueMatchesRenderer; }
    virtual void setFormControlValueMatchesRenderer(bool b) { m_valueMatchesRenderer = b; }

    virtual void dispatchFormControlChangeEvent();

    bool disabled() const { return m_disabled; }
    void setDisabled(bool);

    virtual bool supportsFocus() const;
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return false; }

    virtual bool isReadOnlyFormControl() const { return m_readOnly; }
    void setReadOnly(bool);

    // Determines whether or not a control will be automatically focused
    virtual bool autofocus() const;
    void setAutofocus(bool);

    bool required() const;
    void setRequired(bool);

    virtual void recalcStyle(StyleChange);

    virtual const AtomicString& formControlName() const;
    virtual const AtomicString& formControlType() const = 0;

    const AtomicString& type() const { return formControlType(); }
    const AtomicString& name() const { return formControlName(); }

    void setName(const AtomicString& name);

    virtual bool isFormControlElement() const { return true; }
    virtual bool isRadioButton() const { return false; }

    /* Override in derived classes to get the encoded name=value pair for submitting.
     * Return true for a successful control (see HTML4-17.13.2).
     */
    virtual bool appendFormData(FormDataList&, bool) { return false; }

    virtual bool isSuccessfulSubmitButton() const { return false; }
    virtual bool isActivatedSubmit() const { return false; }
    virtual void setActivatedSubmit(bool) { }

    virtual short tabIndex() const;

    virtual bool willValidate() const;
    String validationMessage();
    bool checkValidity();
    // This must be called when a validation constraint or control value is changed.
    void setNeedsValidityCheck();
    void setCustomValidity(const String&);
    virtual bool valueMissing() const { return false; }
    virtual bool patternMismatch() const { return false; }
    virtual bool tooLong() const { return false; }

    void formDestroyed();

    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

protected:
    void removeFromForm();
    // This must be called any time the result of willValidate() has changed.
    void setNeedsWillValidateCheck();

private:
    virtual HTMLFormElement* virtualForm() const;
    virtual bool isDefaultButtonForForm() const;
    virtual bool isValidFormControlElement();

    HTMLFormElement* m_form;
    OwnPtr<ValidityState> m_validityState;
    bool m_hasName : 1;
    bool m_disabled : 1;
    bool m_readOnly : 1;
    bool m_required : 1;
    bool m_valueMatchesRenderer : 1;
};

class HTMLFormControlElementWithState : public HTMLFormControlElement {
public:
    HTMLFormControlElementWithState(const QualifiedName& tagName, Document*, HTMLFormElement*);
    virtual ~HTMLFormControlElementWithState();

    virtual void finishParsingChildren();

protected:
    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();
};

class HTMLTextFormControlElement : public HTMLFormControlElementWithState {
public:
    HTMLTextFormControlElement(const QualifiedName&, Document*, HTMLFormElement*);
    virtual ~HTMLTextFormControlElement();
    virtual void dispatchFocusEvent();
    virtual void dispatchBlurEvent();

    int selectionStart();
    int selectionEnd();
    void setSelectionStart(int);
    void setSelectionEnd(int);
    void select();
    void setSelectionRange(int start, int end);
    VisibleSelection selection() const;

protected:
    bool placeholderShouldBeVisible() const;
    void updatePlaceholderVisibility(bool);
    virtual int cachedSelectionStart() const = 0;
    virtual int cachedSelectionEnd() const = 0;
    virtual void parseMappedAttribute(MappedAttribute*);

private:
    // A subclass should return true if placeholder processing is needed.
    virtual bool supportsPlaceholder() const = 0;
    // Returns true if user-editable value is empty.  This is used to check placeholder visibility.
    virtual bool isEmptyValue() const = 0;
    // Called in dispatchFocusEvent(), after placeholder process, before calling parent's dispatchFocusEvent().
    virtual void handleFocusEvent() { }
    // Called in dispatchBlurEvent(), after placeholder process, before calling parent's dispatchBlurEvent().
    virtual void handleBlurEvent() { }
    RenderTextControl* textRendererAfterUpdateLayout();
};

} //namespace

#endif
