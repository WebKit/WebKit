/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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

#include "config.h"
#include "HTMLTextAreaElement.h"

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "FocusController.h"
#include "FormDataList.h"
#include "Frame.h"
#include "HTMLNames.h"
#include "Page.h"
#include "RenderStyle.h"
#include "RenderTextControl.h"
#include "Selection.h"
#include "Text.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

static const int defaultRows = 2;
static const int defaultCols = 20;

HTMLTextAreaElement::HTMLTextAreaElement(Document* doc, HTMLFormElement* f)
    : HTMLFormControlElementWithState(textareaTag, doc, f)
    , m_rows(defaultRows)
    , m_cols(defaultCols)
    , m_wrap(ta_Virtual)
    , cachedSelStart(-1)
    , cachedSelEnd(-1)
{
    setValueMatchesRenderer();
}

const AtomicString& HTMLTextAreaElement::type() const
{
    static const AtomicString textarea("textarea");
    return textarea;
}

bool HTMLTextAreaElement::saveState(String& result) const
{
    result = value();
    return true;
}

void HTMLTextAreaElement::restoreState(const String& state)
{
    setDefaultValue(state);
}

int HTMLTextAreaElement::selectionStart()
{
    if (renderer()) {
        if (document()->focusedNode() != this && cachedSelStart != -1)
            return cachedSelStart;
        return static_cast<RenderTextControl *>(renderer())->selectionStart();
    }
    return 0;
}

int HTMLTextAreaElement::selectionEnd()
{
    if (renderer()) {
        if (document()->focusedNode() != this && cachedSelEnd != -1)
            return cachedSelEnd;
        return static_cast<RenderTextControl *>(renderer())->selectionEnd();
    }
    return 0;
}

void HTMLTextAreaElement::setSelectionStart(int start)
{
    if (renderer())
        static_cast<RenderTextControl*>(renderer())->setSelectionStart(start);
}

void HTMLTextAreaElement::setSelectionEnd(int end)
{
    if (renderer())
        static_cast<RenderTextControl*>(renderer())->setSelectionEnd(end);
}

void HTMLTextAreaElement::select()
{
    if (renderer())
        static_cast<RenderTextControl *>(renderer())->select();
}

void HTMLTextAreaElement::setSelectionRange(int start, int end)
{
    if (renderer())
        static_cast<RenderTextControl*>(renderer())->setSelectionRange(start, end);
}

void HTMLTextAreaElement::childrenChanged(bool changedByParser)
{
    setValue(defaultValue());
    HTMLElement::childrenChanged(changedByParser);
}
    
void HTMLTextAreaElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == rowsAttr) {
        int rows = attr->value().toInt();
        if (rows <= 0)
            rows = defaultRows;
        if (m_rows != rows) {
            m_rows = rows;
            if (renderer())
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else if (attr->name() == colsAttr) {
        int cols = attr->value().toInt();
        if (cols <= 0)
            cols = defaultCols;
        if (m_cols != cols) {
            m_cols = cols;
            if (renderer())
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        }
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
            renderer()->setNeedsLayoutAndPrefWidthsRecalc();
    } else if (attr->name() == accesskeyAttr) {
        // ignore for the moment
    } else if (attr->name() == alignAttr) {
        // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=7075
    } else if (attr->name() == onfocusAttr)
        setHTMLEventListener(focusEvent, attr);
    else if (attr->name() == onblurAttr)
        setHTMLEventListener(blurEvent, attr);
    else if (attr->name() == onselectAttr)
        setHTMLEventListener(selectEvent, attr);
    else if (attr->name() == onchangeAttr)
        setHTMLEventListener(changeEvent, attr);
    else
        HTMLFormControlElementWithState::parseMappedAttribute(attr);
}

RenderObject* HTMLTextAreaElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return new (arena) RenderTextControl(this, true);
}

bool HTMLTextAreaElement::appendFormData(FormDataList& encoding, bool)
{
    if (name().isEmpty())
        return false;
        
    bool hardWrap = renderer() && wrap() == ta_Physical;
    String v = hardWrap ? static_cast<RenderTextControl*>(renderer())->textWithHardLineBreaks() : value();
    encoding.appendData(name(), v);
    return true;
}

void HTMLTextAreaElement::reset()
{
    setValue(defaultValue());
}

bool HTMLTextAreaElement::isKeyboardFocusable(KeyboardEvent*) const
{
    // If text areas can be focused, then they should always be keyboard focusable
    return HTMLFormControlElementWithState::isFocusable();
}

bool HTMLTextAreaElement::isMouseFocusable() const
{
    return HTMLFormControlElementWithState::isFocusable();
}

void HTMLTextAreaElement::updateFocusAppearance(bool restorePreviousSelection)
{
    ASSERT(renderer());
    
    if (!restorePreviousSelection || cachedSelStart == -1) {
        // If this is the first focus, set a caret at the beginning of the text.  
        // This matches some browsers' behavior; see Bugzilla Bug 11746 Comment #15.
        // http://bugs.webkit.org/show_bug.cgi?id=11746#c15
        setSelectionRange(0, 0);
    } else
        // Restore the cached selection.  This matches other browsers' behavior.
        setSelectionRange(cachedSelStart, cachedSelEnd); 

    if (document()->frame())
        document()->frame()->revealSelection();
}

void HTMLTextAreaElement::defaultEventHandler(Event *evt)
{
    if (renderer() && (evt->isMouseEvent() || evt->isDragEvent() || evt->isWheelEvent() || evt->type() == blurEvent))
        static_cast<RenderTextControl*>(renderer())->forwardEvent(evt);

    HTMLFormControlElementWithState::defaultEventHandler(evt);
}

void HTMLTextAreaElement::rendererWillBeDestroyed()
{
    updateValue();
}

void HTMLTextAreaElement::updateValue() const
{
    if (!valueMatchesRenderer()) {
        ASSERT(renderer());
        m_value = static_cast<RenderTextControl*>(renderer())->text();
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
    // Code elsewhere normalizes line endings added by the user via the keyboard or pasting.
    // We must normalize line endings coming from JS.
    DeprecatedString valueWithNormalizedLineEndings = value.deprecatedString();
    valueWithNormalizedLineEndings.replace("\r\n", "\n");
    valueWithNormalizedLineEndings.replace("\r", "\n");
    
    m_value = valueWithNormalizedLineEndings;
    setValueMatchesRenderer();
    if (inDocument())
        document()->updateRendering();
    if (renderer())
        renderer()->updateFromElement();
    
    // Set the caret to the end of the text value.
    if (document()->focusedNode() == this) {
        unsigned endOfString = m_value.length();
        setSelectionRange(endOfString, endOfString);
    }

    setChanged();
}

String HTMLTextAreaElement::defaultValue() const
{
    String val = "";

    // Since there may be comments, ignore nodes other than text nodes.
    for (Node* n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            val += static_cast<Text*>(n)->data();

    // FIXME: We should only drop the first carriage return for the default
    // value in the original source, not defaultValues set from JS. This code
    // will do both.
    if (val.length() >= 2 && val[0] == '\r' && val[1] == '\n')
        val.remove(0, 2);
    else if (val.length() >= 1 && (val[0] == '\r' || val[0] == '\n'))
        val.remove(0, 1);

    return val;
}

void HTMLTextAreaElement::setDefaultValue(const String& defaultValue)
{
    // To preserve comments, remove all the text nodes, then add a single one.
    Vector<RefPtr<Node> > textNodes;
    for (Node* n = firstChild(); n; n = n->nextSibling())
        if (n->isTextNode())
            textNodes.append(n);
    ExceptionCode ec = 0;
    size_t size = textNodes.size();
    for (size_t i = 0; i < size; ++i)
        removeChild(textNodes[i].get(), ec);
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

Selection HTMLTextAreaElement::selection() const
{
    if (!renderer() || cachedSelStart == -1 || cachedSelEnd == -1)
        return Selection();
    return static_cast<RenderTextControl*>(renderer())->selection(cachedSelStart, cachedSelEnd);
}

bool HTMLTextAreaElement::shouldUseInputMethod() const
{
    return true;
}

} // namespace
