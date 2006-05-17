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
#include "dom2_eventsimpl.h"
#include "HTMLTextAreaElement.h"
#include "Document.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "RenderTextArea.h"
#include "RenderTextField.h"
#include "render_style.h"
#include "Text.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLTextAreaElement::HTMLTextAreaElement(Document *doc, HTMLFormElement *f)
    : HTMLGenericFormElement(textareaTag, doc, f)
    , m_rows(2)
    , m_cols(20)
    , m_wrap(ta_Virtual)
{
    setValueMatchesRenderer();
    document()->registerFormElementWithState(this);
}

HTMLTextAreaElement::~HTMLTextAreaElement()
{
    document()->deregisterFormElementWithState(this);
}

const AtomicString& HTMLTextAreaElement::type() const
{
    static const AtomicString textarea("textarea");
    return textarea;
}

String HTMLTextAreaElement::stateValue() const
{
    return value();
}

void HTMLTextAreaElement::restoreState(const String& state)
{
    setDefaultValue(state);
}

int HTMLTextAreaElement::selectionStart()
{
    if (renderer()) {
        if (renderer()->style()->appearance() == TextAreaAppearance)
            return static_cast<RenderTextField *>(renderer())->selectionStart();
        return static_cast<RenderTextArea*>(renderer())->selectionStart();
    }
    return 0;
}

int HTMLTextAreaElement::selectionEnd()
{
    if (renderer()) {
        if (renderer()->style()->appearance() == TextAreaAppearance)
            return static_cast<RenderTextField *>(renderer())->selectionEnd();
        return static_cast<RenderTextArea*>(renderer())->selectionEnd();
    }
    return 0;
}

void HTMLTextAreaElement::setSelectionStart(int start)
{
    if (renderer()) {
        if (renderer()->style()->appearance() == TextAreaAppearance)
            static_cast<RenderTextField*>(renderer())->setSelectionStart(start);
        else
            static_cast<RenderTextArea*>(renderer())->setSelectionStart(start);
    }
}

void HTMLTextAreaElement::setSelectionEnd(int end)
{
    if (renderer()) {
        if (renderer()->style()->appearance() == TextAreaAppearance)
            static_cast<RenderTextField*>(renderer())->setSelectionEnd(end);
        else
            static_cast<RenderTextArea*>(renderer())->setSelectionEnd(end);
    }
}

void HTMLTextAreaElement::select()
{
    if (renderer()) {
        if (renderer()->style()->appearance() == TextAreaAppearance)
            static_cast<RenderTextField *>(renderer())->select();
        else
            static_cast<RenderTextArea *>(renderer())->select();
    }
}

void HTMLTextAreaElement::setSelectionRange(int start, int end)
{
    if (renderer()) {
        if (renderer()->style()->appearance() == TextAreaAppearance)
            static_cast<RenderTextField*>(renderer())->setSelectionRange(start, end);
        else
            static_cast<RenderTextArea*>(renderer())->setSelectionRange(start, end);    
    }
}

void HTMLTextAreaElement::childrenChanged()
{
    setValue(defaultValue());
}
    
void HTMLTextAreaElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == rowsAttr) {
        m_rows = !attr->isNull() ? attr->value().toInt() : 3;
        if (renderer())
            renderer()->setNeedsLayoutAndMinMaxRecalc();
    } else if (attr->name() == colsAttr) {
        m_cols = !attr->isNull() ? attr->value().toInt() : 60;
        if (renderer())
            renderer()->setNeedsLayoutAndMinMaxRecalc();
    } else if (attr->name() == wrapAttr) {
        // virtual / physical is Netscape extension of HTML 3.0, now deprecated
        // soft/ hard / off is recommendation for HTML 4 extension by IE and NS 4
        if (equalIgnoringCase(attr->value(), "virtual") || equalIgnoringCase(attr->value(), "soft"))
            m_wrap = ta_Virtual;
        else if (equalIgnoringCase(attr->value(), "physical") || equalIgnoringCase(attr->value(), "hard"))
            m_wrap = ta_Physical;
        else if (equalIgnoringCase(attr->value(), "on" ))
            m_wrap = ta_Physical;
        else if (equalIgnoringCase(attr->value(), "off"))
            m_wrap = ta_NoWrap;
        if (renderer())
            renderer()->setNeedsLayoutAndMinMaxRecalc();
    } else if (attr->name() == accesskeyAttr) {
        // ignore for the moment
    } else if (attr->name() == onfocusAttr)
        setHTMLEventListener(focusEvent, attr);
    else if (attr->name() == onblurAttr)
        setHTMLEventListener(blurEvent, attr);
    else if (attr->name() == onselectAttr)
        setHTMLEventListener(selectEvent, attr);
    else if (attr->name() == onchangeAttr)
        setHTMLEventListener(changeEvent, attr);
    else
        HTMLGenericFormElement::parseMappedAttribute(attr);
}

RenderObject* HTMLTextAreaElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (style->appearance() == TextAreaAppearance)
        return new (arena) RenderTextField(this, true);
    return new (arena) RenderTextArea(this);
}

bool HTMLTextAreaElement::appendFormData(FormDataList& encoding, bool)
{
    if (name().isEmpty())
        return false;
        
    bool hardWrap = renderer() && wrap() == ta_Physical;
    String v;
    if (renderer() && renderer()->style()->appearance() == TextAreaAppearance)
        v = hardWrap ? static_cast<RenderTextField*>(renderer())->textWithHardLineBreaks() : value();
    else
        v = hardWrap ? static_cast<RenderTextArea*>(renderer())->textWithHardLineBreaks() : value();
    encoding.appendData(name(), v);
    return true;
}

void HTMLTextAreaElement::reset()
{
    setValue(defaultValue());
}

bool HTMLTextAreaElement::isKeyboardFocusable() const
{
    // If text areas can be focused, then they should always be keyboard focusable
    if (renderer() && renderer()->style()->appearance() == TextAreaAppearance)
        return HTMLGenericFormElement::isFocusable();
    return HTMLGenericFormElement::isKeyboardFocusable();
}

bool HTMLTextAreaElement::isMouseFocusable() const
{
    if (renderer() && renderer()->style()->appearance() == TextAreaAppearance)
        return HTMLGenericFormElement::isFocusable();
    return HTMLGenericFormElement::isMouseFocusable();
}

void HTMLTextAreaElement::focus()
{
    if (renderer() && renderer()->style()->appearance() == TextAreaAppearance) {
        Document* doc = document();
        if (doc->focusNode() == this)
            return;
        doc->updateLayout();
        // FIXME: Should isFocusable do the updateLayout?
        if (!isFocusable())
            return;
        doc->setFocusNode(this);
        select();
        if (doc->frame())
            doc->frame()->revealSelection();
        return;
    }
    HTMLGenericFormElement::focus();
}

void HTMLTextAreaElement::defaultEventHandler(Event *evt)
{
    if (renderer() && renderer()->style()->appearance() == TextAreaAppearance && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent() || evt->type() == blurEvent))
        static_cast<RenderTextField*>(renderer())->forwardEvent(evt);

    HTMLGenericFormElement::defaultEventHandler(evt);
}

void HTMLTextAreaElement::rendererWillBeDestroyed()
{
    updateValue();
}

void HTMLTextAreaElement::updateValue() const
{
    if (!valueMatchesRenderer()) {
        ASSERT(renderer());
        if (renderer()->style()->appearance() == TextAreaAppearance)
            m_value = static_cast<RenderTextField*>(renderer())->text();
        else
            m_value = static_cast<RenderTextArea*>(renderer())->text();
        setValueMatchesRenderer();
    }
}

String HTMLTextAreaElement::value() const
{
    updateValue();
    return m_value;
}

void HTMLTextAreaElement::setValue(const String& value)
{
    DeprecatedString string = value.deprecatedString();
    // WebCoreTextArea normalizes line endings added by the user via the keyboard or pasting.
    // We must normalize line endings coming from JS.
    string.replace("\r\n", "\n");
    string.replace("\r", "\n");
    
    m_value = String(string);
    setValueMatchesRenderer();
    if (renderer())
        renderer()->updateFromElement();
    setChanged(true);
}

String HTMLTextAreaElement::defaultValue() const
{
    String val = "";

    // there may be comments - just grab the text nodes
    for (Node* n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            val += static_cast<Text*>(n)->data();

    // FIXME: We should only drop the first carriage return for the default
    // value in the original source, not defaultValues set from JS.
    if (val.length() >= 2 && val[0] == '\r' && val[1] == '\n')
        val.remove(0, 2);
    else if (val.length() >= 1 && (val[0] == '\r' || val[0] == '\n'))
        val.remove(0, 1);

    return val;
}

void HTMLTextAreaElement::setDefaultValue(const String& defaultValue)
{
    // there may be comments - remove all the text nodes and replace them with one
    DeprecatedPtrList<Node> toRemove;
    Node *n;
    for (n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            toRemove.append(n);
    DeprecatedPtrListIterator<Node> it(toRemove);
    ExceptionCode ec = 0;
    for (; it.current(); ++it) {
        RefPtr<Node> n = it.current();
        removeChild(n.get(), ec);
    }
    insertBefore(document()->createTextNode(defaultValue), firstChild(), ec);
    setValue(defaultValue);
}

void HTMLTextAreaElement::accessKeyAction(bool sendToAnyElement)
{
    focus();
}

String HTMLTextAreaElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLTextAreaElement::setAccessKey(const String& value)
{
    setAttribute(accesskeyAttr, value);
}

void HTMLTextAreaElement::setCols(int cols)
{
    setAttribute(colsAttr, String::number(cols));
}

void HTMLTextAreaElement::setRows(int rows)
{
    setAttribute(rowsAttr, String::number(rows));
}

} // namespace
