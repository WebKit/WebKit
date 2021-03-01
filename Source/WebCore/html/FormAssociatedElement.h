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

#pragma once

#include "FormNamedItem.h"
#include "Node.h"
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ContainerNode;
class DOMFormData;
class Document;
class FormAttributeTargetObserver;
class HTMLElement;
class HTMLFormElement;
class ValidityState;

class FormAssociatedElement : public FormNamedItem {
    WTF_MAKE_NONCOPYABLE(FormAssociatedElement);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~FormAssociatedElement();

    void ref() { refFormAssociatedElement(); }
    void deref() { derefFormAssociatedElement(); }

    static HTMLFormElement* findAssociatedForm(const HTMLElement*, HTMLFormElement*);
    WEBCORE_EXPORT HTMLFormElement* form() const;
    ValidityState* validity();

    virtual bool isFormControlElement() const = 0;
    virtual bool isFormControlElementWithState() const;
    virtual bool isEnumeratable() const = 0;

    // Returns the 'name' attribute value. If this element has no name
    // attribute, it returns an empty string instead of null string.
    // Note that the 'name' IDL attribute doesn't use this function.
    virtual const AtomString& name() const;

    // Override in derived classes to get the encoded name=value pair for submitting.
    // Return true for a successful control (see HTML4-17.13.2).
    virtual bool appendFormData(DOMFormData&, bool) { return false; }

    void formWillBeDestroyed();

    void resetFormOwner();

    void formOwnerRemovedFromTree(const Node&);

    // ValidityState attribute implementations
    bool badInput() const { return hasBadInput(); }
    bool customError() const;

    // Implementations of patternMismatch, rangeOverflow, rangerUnderflow, stepMismatch, tooShort, tooLong and valueMissing must call willValidate.
    virtual bool hasBadInput() const;
    virtual bool patternMismatch() const;
    virtual bool rangeOverflow() const;
    virtual bool rangeUnderflow() const;
    virtual bool stepMismatch() const;
    virtual bool tooShort() const;
    virtual bool tooLong() const;
    virtual bool typeMismatch() const;
    virtual bool valueMissing() const;
    virtual String validationMessage() const;
    virtual bool isValid() const;
    virtual void setCustomValidity(const String&);

    void formAttributeTargetChanged();

protected:
    FormAssociatedElement(HTMLFormElement*);

    void insertedIntoAncestor(Node::InsertionType, ContainerNode&);
    void removedFromAncestor(Node::RemovalType, ContainerNode&);
    void didMoveToNewDocument(Document& oldDocument);

    void clearForm() { setForm(nullptr); }
    void setForm(HTMLFormElement*);
    void formAttributeChanged();

    // If you add an override of willChangeForm() or didChangeForm() to a class
    // derived from this one, you will need to add a call to setForm(0) to the
    // destructor of that class.
    virtual void willChangeForm();
    virtual void didChangeForm();

    String customValidationMessage() const;

private:
    // "willValidate" means "is a candidate for constraint validation".
    virtual bool willValidate() const = 0;
    virtual void refFormAssociatedElement() = 0;
    virtual void derefFormAssociatedElement() = 0;

    void resetFormAttributeTargetObserver();

    bool isFormAssociatedElement() const final { return true; }

    std::unique_ptr<FormAttributeTargetObserver> m_formAttributeTargetObserver;
    WeakPtr<HTMLFormElement> m_form;
    WeakPtr<HTMLFormElement> m_formSetByParser;
    String m_customValidationMessage;
};

} // namespace
