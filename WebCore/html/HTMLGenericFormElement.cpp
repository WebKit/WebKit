/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "HTMLGenericFormElement.h"

#include "Document.h"
#include "EventNames.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "render_replaced.h"
#include "RenderTheme.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLGenericFormElement::HTMLGenericFormElement(const QualifiedName& tagName, Document* doc, HTMLFormElement* f)
    : HTMLElement(tagName, doc), m_form(f), m_disabled(false), m_readOnly(false)
{
    if (!m_form)
        m_form = getForm();
    if (m_form)
        m_form->registerFormElement(this);
}

HTMLGenericFormElement::~HTMLGenericFormElement()
{
    if (m_form)
        m_form->removeFormElement(this);
}

void HTMLGenericFormElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == nameAttr) {
        // Do nothing.
    } else if (attr->name() == disabledAttr) {
        bool oldDisabled = m_disabled;
        m_disabled = !attr->isNull();
        if (oldDisabled != m_disabled) {
            setChanged();
            if (renderer() && renderer()->style()->hasAppearance())
                theme()->stateChanged(renderer(), EnabledState);
        }
    } else if (attr->name() == readonlyAttr) {
        bool oldReadOnly = m_readOnly;
        m_readOnly = !attr->isNull();
        if (oldReadOnly != m_readOnly) setChanged();
    } else
        HTMLElement::parseMappedAttribute(attr);
}

void HTMLGenericFormElement::attach()
{
    assert(!attached());

    HTMLElement::attach();

    // The call to updateFromElement() needs to go after the call through
    // to the base class's attach() because that can sometimes do a close
    // on the renderer.
    if (renderer()) {
        renderer()->updateFromElement();
    
        // Delayed attachment in order to prevent FOUC can result in an object being
        // programmatically focused before it has a render object.  If we have been focused
        // (i.e., if we are the focusNode) then go ahead and focus our corresponding native widget.
        // (Attach/detach can also happen as a result of display type changes, e.g., making a widget
        // block instead of inline, and focus should be restored in that case as well.)
        if (document()->focusNode() == this && renderer()->isWidget() && 
            static_cast<RenderWidget*>(renderer())->widget())
            static_cast<RenderWidget*>(renderer())->widget()->setFocus();
    }
}

void HTMLGenericFormElement::insertedIntoTree(bool deep)
{
    if (!m_form) {
        // This handles the case of a new form element being created by
        // JavaScript and inserted inside a form.  In the case of the parser
        // setting a form, we will already have a non-null value for m_form, 
        // and so we don't need to do anything.
        m_form = getForm();
        if (m_form)
            m_form->registerFormElement(this);
        else
            if (isRadioButton() && !name().isEmpty() && isChecked())
                document()->radioButtonChecked((HTMLInputElement*)this, m_form);
    }

    HTMLElement::insertedIntoTree(deep);
}

static inline Node* findRoot(Node* n)
{
    Node* root = n;
    for (; n; n = n->parentNode())
        root = n;
    return root;
}

void HTMLGenericFormElement::removedFromTree(bool deep)
{
    // If the form and element are both in the same tree, preserve the connection to the form.
    // Otherwise, null out our form and remove ourselves from the form's list of elements.
    if (m_form && !m_form->preserveAcrossRemove() && findRoot(this) != findRoot(m_form)) {
        m_form->removeFormElement(this);
        m_form = 0;
    }
    HTMLElement::removedFromTree(deep);
}

HTMLFormElement* HTMLGenericFormElement::getForm() const
{
    for (Node* p = parentNode(); p; p = p->parentNode())
        if (p->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(p);
    return 0;
}

const AtomicString& HTMLGenericFormElement::name() const
{
    const AtomicString& n = getAttribute(nameAttr);
    return n.isNull() ? emptyAtom : n;
}

void HTMLGenericFormElement::setName(const AtomicString &value)
{
    setAttribute(nameAttr, value);
}

void HTMLGenericFormElement::onSelect()
{
    // ### make this work with new form events architecture
    dispatchHTMLEvent(selectEvent,true,false);
}

void HTMLGenericFormElement::onChange()
{
    // ### make this work with new form events architecture
    dispatchHTMLEvent(changeEvent,true,false);
}

bool HTMLGenericFormElement::disabled() const
{
    return m_disabled;
}

void HTMLGenericFormElement::setDisabled(bool b)
{
    setAttribute(disabledAttr, b ? "" : 0);
}

void HTMLGenericFormElement::setReadOnly(bool b)
{
    setAttribute(readonlyAttr, b ? "" : 0);
}

void HTMLGenericFormElement::recalcStyle( StyleChange ch )
{
    //bool changed = changed();
    HTMLElement::recalcStyle( ch );

    if (renderer() /*&& changed*/)
        renderer()->updateFromElement();
}

bool HTMLGenericFormElement::isFocusable() const
{
    if (disabled() || !renderer() || 
        (renderer()->style() && renderer()->style()->visibility() != VISIBLE) || 
        renderer()->width() == 0 || renderer()->height() == 0)
        return false;
    return true;
}

bool HTMLGenericFormElement::isKeyboardFocusable() const
{
    if (isFocusable()) {
        if (renderer()->isWidget()) {
            return static_cast<RenderWidget*>(renderer())->widget() &&
                (static_cast<RenderWidget*>(renderer())->widget()->focusPolicy() & Widget::TabFocus);
        }
        if (document()->frame())
            return document()->frame()->tabsToAllControls();
    }
    return false;
}

bool HTMLGenericFormElement::isMouseFocusable() const
{
    if (isFocusable()) {
        if (renderer()->isWidget()) {
            return static_cast<RenderWidget*>(renderer())->widget() &&
                (static_cast<RenderWidget*>(renderer())->widget()->focusPolicy() & Widget::ClickFocus);
        } 
        // For <input type=image> and <button>, we will assume no mouse focusability.  This is
        // consistent with OS X behavior for buttons.
        return false;
    }
    return false;
}

// Special chars used to encode form state strings.
// We pick chars that are unlikely to be used in an HTML attr, so we rarely have to really encode.
const char stateSeparator = '&';
const char stateEscape = '<';
static const char stateSeparatorMarker[] = "<A";
static const char stateEscapeMarker[] = "<<";

// Encode an element name so we can put it in a state string without colliding
// with our separator char.
static DeprecatedString encodedElementName(DeprecatedString str)
{
    int sepLoc = str.find(stateSeparator);
    int escLoc = str.find(stateSeparator);
    if (sepLoc >= 0 || escLoc >= 0) {
        DeprecatedString newStr = str;
        //   replace "<" with "<<"
        while (escLoc >= 0) {
            newStr.replace(escLoc, 1, stateEscapeMarker);
            escLoc = str.find(stateSeparator, escLoc+1);
        }
        //   replace "&" with "<A"
        while (sepLoc >= 0) {
            newStr.replace(sepLoc, 1, stateSeparatorMarker);
            sepLoc = str.find(stateSeparator, sepLoc+1);
        }
        return newStr;
    } else {
        return str;
    }
}

DeprecatedString HTMLGenericFormElement::state( )
{
    // Build a string that contains ElementName&ElementType&
    return encodedElementName(name().deprecatedString()) + stateSeparator + type().deprecatedString() + stateSeparator;
}

DeprecatedString HTMLGenericFormElement::findMatchingState(DeprecatedStringList &states)
{
    DeprecatedString encName = encodedElementName(name().deprecatedString());
    DeprecatedString typeStr = type().deprecatedString();
    for (DeprecatedStringList::Iterator it = states.begin(); it != states.end(); ++it) {
        DeprecatedString state = *it;
        int sep1 = state.find(stateSeparator);
        int sep2 = state.find(stateSeparator, sep1+1);
        assert(sep1 >= 0);
        assert(sep2 >= 0);

        String nameAndType = state.left(sep2);
        if (encName.length() + typeStr.length() + 1 == (unsigned)sep2
            && nameAndType.startsWith(encName)
            && nameAndType.endsWith(typeStr)) {
            states.remove(it);
            return state.mid(sep2+1);
        }
    }
    return DeprecatedString::null;
}

int HTMLGenericFormElement::tabIndex() const
{
    return getAttribute(tabindexAttr).toInt();
}

void HTMLGenericFormElement::setTabIndex(int value)
{
    setAttribute(tabindexAttr, String::number(value));
}
    
} // namespace
