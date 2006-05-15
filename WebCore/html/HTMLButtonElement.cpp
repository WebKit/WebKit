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
#include "HTMLButtonElement.h"
#include "HTMLFormElement.h"
#include "dom2_eventsimpl.h"
#include "FormDataList.h"

#include "RenderButton.h"

#include "EventNames.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLButtonElement::HTMLButtonElement(Document* doc, HTMLFormElement* f)
    : HTMLGenericFormElement(buttonTag, doc, f)
    , m_type(SUBMIT)
    , m_dirty(true)
    , m_activeSubmit(false)
{
}

HTMLButtonElement::~HTMLButtonElement()
{
}

RenderObject* HTMLButtonElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderButton(this);
}

const AtomicString& HTMLButtonElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLButtonElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() ==  typeAttr) {
        if (equalIgnoringCase(attr->value(), "submit"))
            m_type = SUBMIT;
        else if (equalIgnoringCase(attr->value(), "reset"))
            m_type = RESET;
        else if (equalIgnoringCase(attr->value(), "button"))
            m_type = BUTTON;
    } else if (attr->name() == valueAttr) {
        m_value = attr->value();
        m_currValue = m_value;
    } else if (attr->name() == accesskeyAttr) {
        // Do nothing.
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else
        HTMLGenericFormElement::parseMappedAttribute(attr);
}

void HTMLButtonElement::defaultEventHandler(Event *evt)
{
    if (m_type != BUTTON && (evt->type() == DOMActivateEvent)) {
        if (form() && m_type == SUBMIT) {
            m_activeSubmit = true;
            form()->prepareSubmit();
            m_activeSubmit = false; // in case we were canceled
        }
        if (form() && m_type == RESET)
            form()->reset();
    }
    HTMLGenericFormElement::defaultEventHandler(evt);
}

bool HTMLButtonElement::isSuccessfulSubmitButton() const
{
    // HTML spec says that buttons must have names
    // to be considered successful. However, other browsers
    // do not impose this constraint. Therefore, we behave
    // differently and can use different buttons than the 
    // author intended. 
    // Remove the name constraint for now.
    // Was: m_type == SUBMIT && !disabled() && !name().isEmpty()
    return m_type == SUBMIT && !disabled();
}

bool HTMLButtonElement::isActivatedSubmit() const
{
    return m_activeSubmit;
}

void HTMLButtonElement::setActivatedSubmit(bool flag)
{
    m_activeSubmit = flag;
}

bool HTMLButtonElement::appendFormData(FormDataList& encoding, bool /*multipart*/)
{
    if (m_type != SUBMIT || name().isEmpty() || !m_activeSubmit)
        return false;
    encoding.appendData(name(), m_currValue);
    return true;
}

void HTMLButtonElement::accessKeyAction(bool sendToAnyElement)
{   
    // send the mouse button events iff the
    // caller specified sendToAnyElement
    click(sendToAnyElement);
}

String HTMLButtonElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLButtonElement::setAccessKey(const String &value)
{
    setAttribute(accesskeyAttr, value);
}

String HTMLButtonElement::value() const
{
    return getAttribute(valueAttr);
}

void HTMLButtonElement::setValue(const String &value)
{
    setAttribute(valueAttr, value);
}
    
} // namespace
