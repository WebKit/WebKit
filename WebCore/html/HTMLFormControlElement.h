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

#include "FormControlElement.h"
#include "FormControlElementWithState.h"
#include "HTMLElement.h"

namespace WebCore {

class FormDataList;
class HTMLFormElement;

class HTMLFormControlElement : public HTMLElement, public FormControlElement {
public:
    HTMLFormControlElement(const QualifiedName& tagName, Document*, HTMLFormElement*);
    virtual ~HTMLFormControlElement();

    virtual HTMLTagStatus endTagRequirement() const { return TagStatusRequired; }
    virtual int tagPriority() const { return 1; }

    HTMLFormElement* form() const { return m_form; }

    virtual const AtomicString& type() const = 0;

    virtual bool isControl() const { return true; }
    virtual bool isEnabled() const { return !disabled(); }

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void attach();
    virtual void insertedIntoTree(bool deep);
    virtual void removedFromTree(bool deep);

    virtual void reset() {}

    virtual bool valueMatchesRenderer() const { return m_valueMatchesRenderer; }
    virtual void setValueMatchesRenderer(bool b = true) { m_valueMatchesRenderer = b; }

    void onChange();

    bool disabled() const;
    void setDisabled(bool);

    virtual bool supportsFocus() const;
    virtual bool isFocusable() const;
    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;
    virtual bool isEnumeratable() const { return false; }

    virtual bool isReadOnlyControl() const { return m_readOnly; }
    void setReadOnly(bool);

    // Determines whether or not a control will be automatically focused
    virtual bool autofocus() const;
    void setAutofocus(bool);

    virtual void recalcStyle(StyleChange);

    virtual const AtomicString& name() const;
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

    void formDestroyed() { m_form = 0; }

protected:
    void removeFromForm();

private:
    virtual HTMLFormElement* virtualForm() const;

    HTMLFormElement* m_form;
    bool m_disabled;
    bool m_readOnly;
    bool m_valueMatchesRenderer;
};

class HTMLFormControlElementWithState : public HTMLFormControlElement, public FormControlElementWithState  {
public:
    HTMLFormControlElementWithState(const QualifiedName& tagName, Document*, HTMLFormElement*);
    virtual ~HTMLFormControlElementWithState();

    virtual bool isFormControlElementWithState() const { return true; }

    virtual FormControlElement* toFormControlElement() { return this; }
    virtual void finishParsingChildren();

protected:
    virtual void willMoveToNewOwnerDocument();
    virtual void didMoveToNewOwnerDocument();
};

} //namespace

#endif
