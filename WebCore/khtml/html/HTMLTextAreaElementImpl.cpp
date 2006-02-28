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
#include "HTMLTextAreaElementImpl.h"
#include "rendering/render_form.h"
#include "TextImpl.h"
#include "FormDataList.h"
#include "DocumentImpl.h"

#include "EventNames.h"
#include "htmlnames.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLTextAreaElementImpl::HTMLTextAreaElementImpl(DocumentImpl *doc, HTMLFormElementImpl *f)
    : HTMLGenericFormElementImpl(textareaTag, doc, f)
    , m_valueMatchesRenderer(true)
{
    // DTD requires rows & cols be specified, but we will provide reasonable defaults
    m_rows = 2;
    m_cols = 20;
    m_wrap = ta_Virtual;
}

HTMLTextAreaElementImpl::~HTMLTextAreaElementImpl()
{
    if (getDocument()) getDocument()->deregisterMaintainsState(this);
}

DOMString HTMLTextAreaElementImpl::type() const
{
    return "textarea";
}

QString HTMLTextAreaElementImpl::state( )
{
    // Make sure the string is not empty!
    return HTMLGenericFormElementImpl::state() + value().qstring()+'.';
}

void HTMLTextAreaElementImpl::restoreState(QStringList &states)
{
    QString state = HTMLGenericFormElementImpl::findMatchingState(states);
    if (state.isNull()) return;
    setDefaultValue(state.left(state.length()-1));
    // the close() in the rendertree will take care of transferring defaultvalue to 'value'
}

int HTMLTextAreaElementImpl::selectionStart()
{
    if (renderer())
        return static_cast<RenderTextArea *>(renderer())->selectionStart();
    return 0;
}

int HTMLTextAreaElementImpl::selectionEnd()
{
    if (renderer())
        return static_cast<RenderTextArea *>(renderer())->selectionEnd();
    return 0;
}

void HTMLTextAreaElementImpl::setSelectionStart(int start)
{
    if (renderer())
        static_cast<RenderTextArea *>(renderer())->setSelectionStart(start);
}

void HTMLTextAreaElementImpl::setSelectionEnd(int end)
{
    if (renderer())
        static_cast<RenderTextArea *>(renderer())->setSelectionEnd(end);
}

void HTMLTextAreaElementImpl::select()
{
    if (renderer())
        static_cast<RenderTextArea*>(renderer())->select();
}

void HTMLTextAreaElementImpl::setSelectionRange(int start, int end)
{
    if (renderer())
        static_cast<RenderTextArea *>(renderer())->setSelectionRange(start, end);
}

void HTMLTextAreaElementImpl::childrenChanged()
{
    setValue(defaultValue());
}
    
void HTMLTextAreaElementImpl::parseMappedAttribute(MappedAttributeImpl *attr)
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
        HTMLGenericFormElementImpl::parseMappedAttribute(attr);
}

RenderObject *HTMLTextAreaElementImpl::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderTextArea(this);
}

bool HTMLTextAreaElementImpl::appendFormData(FormDataList& encoding, bool)
{
    if (name().isEmpty())
        return false;
        
    bool hardWrap = renderer() && wrap() == ta_Physical;
    String v = hardWrap ? static_cast<RenderTextArea*>(renderer())->textWithHardLineBreaks() : value();
    encoding.appendData(name(), v);
    return true;
}

void HTMLTextAreaElementImpl::reset()
{
    setValue(defaultValue());
}

void HTMLTextAreaElementImpl::rendererWillBeDestroyed()
{
    updateValue();
}

void HTMLTextAreaElementImpl::updateValue()
{
    if (!m_valueMatchesRenderer) {
        ASSERT(renderer());
        m_value = static_cast<RenderTextArea*>(renderer())->text();
        m_valueMatchesRenderer = true;
    }
}

DOMString HTMLTextAreaElementImpl::value()
{
    updateValue();
    return m_value;
}

void HTMLTextAreaElementImpl::setValue(const DOMString &value)
{
    QString string = value.qstring();
    // KWQTextArea normalizes line endings added by the user via the keyboard or pasting.
    // We must normalize line endings coming from JS.
    string.replace("\r\n", "\n");
    string.replace("\r", "\n");
    
    m_value = String(string);
    m_valueMatchesRenderer = true;
    if (renderer())
        renderer()->updateFromElement();
    setChanged(true);
}

DOMString HTMLTextAreaElementImpl::defaultValue()
{
    DOMString val = "";
    // there may be comments - just grab the text nodes
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            val += static_cast<TextImpl*>(n)->data();
    
    // FIXME: We should only drop the first carriage return for the default
    // value in the original source, not defaultValues set from JS.
    if (val.length() >= 2 && val[0] == '\r' && val[1] == '\n')
        val.remove(0, 2);
    else if (val.length() >= 1 && (val[0] == '\r' || val[0] == '\n'))
        val.remove(0, 1);

    return val;
}

void HTMLTextAreaElementImpl::setDefaultValue(const DOMString &defaultValue)
{
    // there may be comments - remove all the text nodes and replace them with one
    QPtrList<NodeImpl> toRemove;
    NodeImpl *n;
    for (n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            toRemove.append(n);
    QPtrListIterator<NodeImpl> it(toRemove);
    int exceptioncode = 0;
    for (; it.current(); ++it) {
        RefPtr<NodeImpl> n = it.current();
        removeChild(n.get(), exceptioncode);
    }
    insertBefore(getDocument()->createTextNode(defaultValue), firstChild(), exceptioncode);
    setValue(defaultValue);
}

bool HTMLTextAreaElementImpl::isEditable()
{
    return true;
}

void HTMLTextAreaElementImpl::accessKeyAction(bool sendToAnyElement)
{
    focus();
}

DOMString HTMLTextAreaElementImpl::accessKey() const
{
    return getAttribute(accesskeyAttr);
}

void HTMLTextAreaElementImpl::setAccessKey(const DOMString &value)
{
    setAttribute(accesskeyAttr, value);
}

void HTMLTextAreaElementImpl::setCols(int cols)
{
    setAttribute(colsAttr, QString::number(cols));
}

void HTMLTextAreaElementImpl::setRows(int rows)
{
    setAttribute(rowsAttr, QString::number(rows));
}

} // namespace
