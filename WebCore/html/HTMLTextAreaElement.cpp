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
#include "HTMLTextAreaElement.h"
#include "rendering/render_form.h"
#include "Text.h"
#include "FormDataList.h"
#include "Document.h"

#include "EventNames.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLTextAreaElement::HTMLTextAreaElement(Document *doc, HTMLFormElement *f)
    : HTMLGenericFormElement(textareaTag, doc, f)
    , m_valueMatchesRenderer(true)
{
    // DTD requires rows & cols be specified, but we will provide reasonable defaults
    m_rows = 2;
    m_cols = 20;
    m_wrap = ta_Virtual;
}

HTMLTextAreaElement::~HTMLTextAreaElement()
{
    getDocument()->deregisterMaintainsState(this);
}

String HTMLTextAreaElement::type() const
{
    return "textarea";
}

DeprecatedString HTMLTextAreaElement::state( )
{
    // Make sure the string is not empty!
    return HTMLGenericFormElement::state() + value().deprecatedString()+'.';
}

void HTMLTextAreaElement::restoreState(DeprecatedStringList &states)
{
    DeprecatedString state = HTMLGenericFormElement::findMatchingState(states);
    if (state.isNull()) return;
    setDefaultValue(state.left(state.length()-1));
    // the close() in the rendertree will take care of transferring defaultvalue to 'value'
}

int HTMLTextAreaElement::selectionStart()
{
    if (renderer())
        return static_cast<RenderTextArea *>(renderer())->selectionStart();
    return 0;
}

int HTMLTextAreaElement::selectionEnd()
{
    if (renderer())
        return static_cast<RenderTextArea *>(renderer())->selectionEnd();
    return 0;
}

void HTMLTextAreaElement::setSelectionStart(int start)
{
    if (renderer())
        static_cast<RenderTextArea *>(renderer())->setSelectionStart(start);
}

void HTMLTextAreaElement::setSelectionEnd(int end)
{
    if (renderer())
        static_cast<RenderTextArea *>(renderer())->setSelectionEnd(end);
}

void HTMLTextAreaElement::select()
{
    if (renderer())
        static_cast<RenderTextArea*>(renderer())->select();
}

void HTMLTextAreaElement::setSelectionRange(int start, int end)
{
    if (renderer())
        static_cast<RenderTextArea *>(renderer())->setSelectionRange(start, end);
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
    } else if (attr->name() == onfocusAttr) {
        setHTMLEventListener(focusEvent, attr);
    } else if (attr->name() == onblurAttr) {
        setHTMLEventListener(blurEvent, attr);
    } else if (attr->name() == onselectAttr) {
        setHTMLEventListener(selectEvent, attr);
    } else if (attr->name() == onchangeAttr) {
        setHTMLEventListener(changeEvent, attr);
    } else
        HTMLGenericFormElement::parseMappedAttribute(attr);
}

RenderObject *HTMLTextAreaElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderTextArea(this);
}

bool HTMLTextAreaElement::appendFormData(FormDataList& encoding, bool)
{
    if (name().isEmpty())
        return false;
        
    bool hardWrap = renderer() && wrap() == ta_Physical;
    String v = hardWrap ? static_cast<RenderTextArea*>(renderer())->textWithHardLineBreaks() : value();
    encoding.appendData(name(), v);
    return true;
}

void HTMLTextAreaElement::reset()
{
    setValue(defaultValue());
}

void HTMLTextAreaElement::rendererWillBeDestroyed()
{
    updateValue();
}

void HTMLTextAreaElement::updateValue()
{
    if (!m_valueMatchesRenderer) {
        ASSERT(renderer());
        m_value = static_cast<RenderTextArea*>(renderer())->text();
        m_valueMatchesRenderer = true;
    }
}

String HTMLTextAreaElement::value()
{
    updateValue();
    return m_value;
}

void HTMLTextAreaElement::setValue(const String &value)
{
    DeprecatedString string = value.deprecatedString();
    // WebCoreTextArea normalizes line endings added by the user via the keyboard or pasting.
    // We must normalize line endings coming from JS.
    string.replace("\r\n", "\n");
    string.replace("\r", "\n");
    
    m_value = String(string);
    m_valueMatchesRenderer = true;
    if (renderer())
        renderer()->updateFromElement();
    setChanged(true);
}

String HTMLTextAreaElement::defaultValue()
{
    String val = "";
    // there may be comments - just grab the text nodes
    Node *n;
    for (n = firstChild(); n; n = n->nextSibling())
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

void HTMLTextAreaElement::setDefaultValue(const String &defaultValue)
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
    insertBefore(getDocument()->createTextNode(defaultValue), firstChild(), ec);
    setValue(defaultValue);
}

bool HTMLTextAreaElement::isEditable()
{
    return true;
}

void HTMLTextAreaElement::accessKeyAction(bool sendToAnyElement)
{
    focus();
}

String HTMLTextAreaElement::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLTextAreaElement::setAccessKey(const String &value)
{
    setAttribute(accesskeyAttr, value);
}

void HTMLTextAreaElement::setCols(int cols)
{
    setAttribute(colsAttr, DeprecatedString::number(cols));
}

void HTMLTextAreaElement::setRows(int rows)
{
    setAttribute(rowsAttr, DeprecatedString::number(rows));
}

} // namespace
