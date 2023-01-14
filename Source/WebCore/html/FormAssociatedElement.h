/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "ElementInlines.h"
#include "HTMLFormElement.h"

namespace WebCore {

// https://html.spec.whatwg.org/multipage/forms.html#form-associated-element
class FormAssociatedElement {
public:
    void ref() { refFormAssociatedElement(); }
    void deref() { derefFormAssociatedElement(); }

    virtual ~FormAssociatedElement() { RELEASE_ASSERT(!m_form); }
    virtual HTMLElement& asHTMLElement() = 0;
    virtual const HTMLElement& asHTMLElement() const = 0;
    virtual bool isFormListedElement() const = 0;

    virtual void formWillBeDestroyed() { m_form = nullptr; }

    HTMLFormElement* form() const { return m_form.get(); }

    virtual void setForm(HTMLFormElement*);
    virtual void elementInsertedIntoAncestor(Element&, Node::InsertionType);
    virtual void elementRemovedFromAncestor(Element&, Node::RemovalType);

protected:
    explicit FormAssociatedElement(HTMLFormElement*);

    virtual void resetFormOwner() = 0;
    virtual void setFormInternal(HTMLFormElement*);

private:
    virtual void refFormAssociatedElement() = 0;
    virtual void derefFormAssociatedElement() = 0;

    WeakPtr<HTMLFormElement, WeakPtrImplWithEventTargetData> m_form;
    WeakPtr<HTMLFormElement, WeakPtrImplWithEventTargetData> m_formSetByParser;
};

inline FormAssociatedElement::FormAssociatedElement(HTMLFormElement* form)
    : m_formSetByParser(form)
{
}

inline void FormAssociatedElement::setForm(HTMLFormElement* newForm)
{
    if (m_form != newForm)
        setFormInternal(newForm);
}

inline void FormAssociatedElement::setFormInternal(HTMLFormElement* newForm)
{
    ASSERT(m_form != newForm);
    m_form = newForm;
}

inline void FormAssociatedElement::elementInsertedIntoAncestor(Element& element, Node::InsertionType)
{
    ASSERT(&asHTMLElement() == &element);
    if (m_formSetByParser) {
        // The form could have been removed by a script during parsing.
        if (m_formSetByParser->isConnected())
            setForm(m_formSetByParser.get());
        m_formSetByParser = nullptr;
    }

    if (m_form && element.rootElement() != m_form->rootElement())
        setForm(nullptr);
}

inline void FormAssociatedElement::elementRemovedFromAncestor(Element& element, Node::RemovalType)
{
    ASSERT(&asHTMLElement() == &element);
    // Do not rely on rootNode() because m_form's IsInTreeScope can be outdated.
    if (m_form && &element.traverseToRootNode() != &m_form->traverseToRootNode()) {
        setForm(nullptr);
        resetFormOwner();
    }
}

} // namespace WebCore
