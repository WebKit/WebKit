/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

using namespace HTMLNames;

static const int defaultRows = 2;
static const int defaultCols = 20;

HTMLTextAreaElement::HTMLTextAreaElement(Document* document, HTMLFormElement* form)
    : HTMLFormControlElementWithState(textareaTag, document, form)
    , m_rows(defaultRows)
    , m_cols(defaultCols)
    , m_wrap(SoftWrap)
    , m_cachedSelectionStart(-1)
    , m_cachedSelectionEnd(-1)
{
    setValueMatchesRenderer();
}

const AtomicString& HTMLTextAreaElement::type() const
{
    static const AtomicString& textarea = *new AtomicString("textarea");
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
    if (!renderer())
        return 0;
    if (document()->focusedNode() != this && m_cachedSelectionStart >= 0)
        return m_cachedSelectionStart;
    return static_cast<RenderTextControl*>(renderer())->selectionStart();
}

int HTMLTextAreaElement::selectionEnd()
{
    if (!renderer())
        return 0;
    if (document()->focusedNode() != this && m_cachedSelectionEnd >= 0)
        return m_cachedSelectionEnd;
    return static_cast<RenderTextControl*>(renderer())->selectionEnd();
}

void HTMLTextAreaElement::setSelectionStart(int start)
{
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionStart(start);
}

void HTMLTextAreaElement::setSelectionEnd(int end)
{
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionEnd(end);
}

void HTMLTextAreaElement::select()
{
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->select();
}

void HTMLTextAreaElement::setSelectionRange(int start, int end)
{
    if (!renderer())
        return;
    static_cast<RenderTextControl*>(renderer())->setSelectionRange(start, end);
}

void HTMLTextAreaElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    setValue(defaultValue());
    HTMLElement::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
}
    
void HTMLTextAreaElement::parseMappedAttribute(MappedAttribute* attr)
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
        // The virtual/physical values were a Netscape extension of HTML 3.0, now deprecated.
        // The soft/hard /off values are a recommendation for HTML 4 extension by IE and NS 4.
        WrapMethod wrap;
        if (equalIgnoringCase(attr->value(), "physical") || equalIgnoringCase(attr->value(), "hard") || equalIgnoringCase(attr->value(), "on"))
            wrap = HardWrap;
        else if (equalIgnoringCase(attr->value(), "off"))
            wrap = NoWrap;
        else
            wrap = SoftWrap;
        if (wrap != m_wrap) {
            m_wrap = wrap;
            if (renderer())
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
        }
    } else if (attr->name() == accesskeyAttr) {
        // ignore for the moment
    } else if (attr->name() == alignAttr) {
        // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
        // See http://bugs.webkit.org/show_bug.cgi?id=7075
    } else if (attr->name() == onfocusAttr)
        setInlineEventListenerForTypeAndAttribute(eventNames().focusEvent, attr);
    else if (attr->name() == onblurAttr)
        setInlineEventListenerForTypeAndAttribute(eventNames().blurEvent, attr);
    else if (attr->name() == onselectAttr)
        setInlineEventListenerForTypeAndAttribute(eventNames().selectEvent, attr);
    else if (attr->name() == onchangeAttr)
        setInlineEventListenerForTypeAndAttribute(eventNames().changeEvent, attr);
    else
        HTMLFormControlElementWithState::parseMappedAttribute(attr);
}

RenderObject* HTMLTextAreaElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderTextControl(this, true);
}

bool HTMLTextAreaElement::appendFormData(FormDataList& encoding, bool)
{
    if (name().isEmpty())
        return false;

    // FIXME: It's not acceptable to ignore the HardWrap setting when there is no renderer.
    // While we have no evidence this has ever been a practical problem, it would be best to fix it some day.
    RenderTextControl* control = static_cast<RenderTextControl*>(renderer());
    const String& text = (m_wrap == HardWrap && control) ? control->textWithHardLineBreaks() : value();
    encoding.appendData(name(), text);
    return true;
}

void HTMLTextAreaElement::reset()
{
    setValue(defaultValue());
}

bool HTMLTextAreaElement::isKeyboardFocusable(KeyboardEvent*) const
{
    // If a given text area can be focused at all, then it will always be keyboard focusable.
    return isFocusable();
}

bool HTMLTextAreaElement::isMouseFocusable() const
{
    return isFocusable();
}

void HTMLTextAreaElement::updateFocusAppearance(bool restorePreviousSelection)
{
    ASSERT(renderer());
    
    if (!restorePreviousSelection || m_cachedSelectionStart < 0) {
        // If this is the first focus, set a caret at the beginning of the text.  
        // This matches some browsers' behavior; see bug 11746 Comment #15.
        // http://bugs.webkit.org/show_bug.cgi?id=11746#c15
        setSelectionRange(0, 0);
    } else {
        // Restore the cached selection.  This matches other browsers' behavior.
        setSelectionRange(m_cachedSelectionStart, m_cachedSelectionEnd);
    }

    if (document()->frame())
        document()->frame()->revealSelection();
}

void HTMLTextAreaElement::defaultEventHandler(Event* event)
{
    if (renderer() && (event->isMouseEvent() || event->isDragEvent() || event->isWheelEvent() || event->type() == eventNames().blurEvent))
        static_cast<RenderTextControl*>(renderer())->forwardEvent(event);

    HTMLFormControlElementWithState::defaultEventHandler(event);
}

void HTMLTextAreaElement::rendererWillBeDestroyed()
{
    updateValue();
}

void HTMLTextAreaElement::updateValue() const
{
    if (valueMatchesRenderer())
        return;

    ASSERT(renderer());
    m_value = static_cast<RenderTextControl*>(renderer())->text();
    setValueMatchesRenderer();
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
    m_value = value;
    m_value.replace("\r\n", "\n");
    m_value.replace('\r', '\n');

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
    String value = "";

    // Since there may be comments, ignore nodes other than text nodes.
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            value += static_cast<Text*>(n)->data();
    }

    UChar firstCharacter = value[0];
    if (firstCharacter == '\r' && value[1] == '\n')
        value.remove(0, 2);
    else if (firstCharacter == '\r' || firstCharacter == '\n')
        value.remove(0, 1);

    return value;
}

void HTMLTextAreaElement::setDefaultValue(const String& defaultValue)
{
    // To preserve comments, remove only the text nodes, then add a single text node.

    Vector<RefPtr<Node> > textNodes;
    for (Node* n = firstChild(); n; n = n->nextSibling()) {
        if (n->isTextNode())
            textNodes.append(n);
    }
    ExceptionCode ec;
    size_t size = textNodes.size();
    for (size_t i = 0; i < size; ++i)
        removeChild(textNodes[i].get(), ec);

    // Normalize line endings.
    // Add an extra line break if the string starts with one, since
    // the code to read default values from the DOM strips the leading one.
    String value = defaultValue;
    value.replace("\r\n", "\n");
    value.replace('\r', '\n');
    if (value[0] == '\n')
        value = "\n" + value;

    insertBefore(document()->createTextNode(value), firstChild(), ec);

    setValue(value);
}

void HTMLTextAreaElement::accessKeyAction(bool)
{
    focus();
}

const AtomicString& HTMLTextAreaElement::accessKey() const
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
    if (!renderer() || m_cachedSelectionStart < 0 || m_cachedSelectionEnd < 0)
        return Selection();
    return static_cast<RenderTextControl*>(renderer())->selection(m_cachedSelectionStart, m_cachedSelectionEnd);
}

bool HTMLTextAreaElement::shouldUseInputMethod() const
{
    return true;
}

} // namespace
