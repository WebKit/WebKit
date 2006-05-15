/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "RenderLineEdit.h"

#include "EventNames.h"
#include "HTMLNames.h"
#include "HTMLFormElement.h"
#include "HTMLInputElement.h"
#include "KWQLineEdit.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

RenderLineEdit::RenderLineEdit(HTMLInputElement* element)
    : RenderFormElement(element)
    , m_updating(false)
{
    QLineEdit::Type type;
    switch (element->inputType()) {
        case HTMLInputElement::PASSWORD:
            type = QLineEdit::Password;
            break;
        case HTMLInputElement::SEARCH:
            type = QLineEdit::Search;
            break;
        default:
            type = QLineEdit::Normal;
    }
    QLineEdit* edit = new QLineEdit(type);
    if (type == QLineEdit::Search)
        edit->setLiveSearch(false);
    setWidget(edit);
}

void RenderLineEdit::selectionChanged(Widget*)
{
    // We only want to call onselect if there actually is a selection
    QLineEdit* w = static_cast<QLineEdit*>(m_widget);
    if (w->hasSelectedText())
        static_cast<HTMLGenericFormElement*>(node())->onSelect();
}

void RenderLineEdit::returnPressed(Widget*)
{
    // Emit onChange if necessary
    // Works but might not be enough, dirk said he had another solution at
    // hand (can't remember which) - David
    if (isTextField() && isEdited()) {
        static_cast<HTMLGenericFormElement*>(node())->onChange();
        setEdited(false);
    }

    if (HTMLFormElement* fe = static_cast<HTMLGenericFormElement*>(node())->form())
        fe->submitClick();
}

void RenderLineEdit::performSearch(Widget*)
{
    // Fire the "search" DOM event.
    static_cast<EventTargetNode*>(node())->dispatchHTMLEvent(searchEvent, true, false);
}

void RenderLineEdit::addSearchResult()
{
    if (widget())
        static_cast<QLineEdit*>(widget())->addSearchResult();
}

void RenderLineEdit::calcMinMaxWidth()
{
    KHTMLAssert(!minMaxKnown());

    // Let the widget tell us how big it wants to be.
    m_updating = true;
    int size = static_cast<HTMLInputElement*>(node())->size();
    IntSize s(static_cast<QLineEdit*>(widget())->sizeForCharacterWidth(size > 0 ? size : 20));
    m_updating = false;

    setIntrinsicWidth(s.width());
    setIntrinsicHeight(s.height());

    RenderFormElement::calcMinMaxWidth();
}

void RenderLineEdit::setStyle(RenderStyle *s)
{
    RenderFormElement::setStyle(s);

    QLineEdit* w = static_cast<QLineEdit*>(widget());
    w->setAlignment(textAlignment());
    w->setWritingDirection(style()->direction() == RTL ? RTL : LTR);
}

void RenderLineEdit::updateFromElement()
{
    HTMLInputElement* e = static_cast<HTMLInputElement*>(node());
    QLineEdit* w = static_cast<QLineEdit*>(widget());
    
    int ml = e->maxLength();
    if (ml <= 0 || ml > 1024)
        ml = 1024;
    if (w->maxLength() != ml)
        w->setMaxLength(ml);

    if (!e->valueMatchesRenderer()) {
        String widgetText = w->text();
        String newText = e->value();
        newText.replace('\\', backslashAsCurrencySymbol());
        if (widgetText != newText) {
            int pos = w->cursorPosition();

            m_updating = true;
            w->setText(newText);
            m_updating = false;
            
            w->setEdited( false );

            w->setCursorPosition(pos);
        }
        e->setValueMatchesRenderer();
    }

    w->setReadOnly(e->isReadOnlyControl());
    
    // Handle updating the search attributes.
    w->setPlaceholderString(e->getAttribute(placeholderAttr).deprecatedString());
    if (w->type() == QLineEdit::Search) {
        w->setLiveSearch(!e->getAttribute(incrementalAttr).isNull());
        w->setAutoSaveName(e->getAttribute(autosaveAttr));
        w->setMaxResults(e->maxResults());
    }

    w->setColors(style()->backgroundColor(), style()->color());

    RenderFormElement::updateFromElement();
}

void RenderLineEdit::valueChanged(Widget*)
{
    if (m_updating) // Don't alter the value if we are in the middle of initing the control, since
        return;     // we are getting the value from the DOM and it's not user input.

    String newText = static_cast<QLineEdit*>(widget())->text();

    // A null string value is used to indicate that the form control has not altered the original
    // default value.  That means that we should never use the null string value when the user
    // empties a textfield, but should always force an empty textfield to use the empty string.
    if (newText.isNull())
        newText = "";

    newText.replace(backslashAsCurrencySymbol(), '\\');
    static_cast<HTMLInputElement*>(node())->setValueFromRenderer(newText);
}

int RenderLineEdit::selectionStart()
{
    QLineEdit *lineEdit = static_cast<QLineEdit *>(m_widget);
    int start = lineEdit->selectionStart();
    if (start == -1)
        start = lineEdit->cursorPosition();
    return start;
}

int RenderLineEdit::selectionEnd()
{
    QLineEdit *lineEdit = static_cast<QLineEdit *>(m_widget);
    int start = lineEdit->selectionStart();
    if (start == -1)
        return lineEdit->cursorPosition();
    return start + (int)lineEdit->selectedText().length();
}

void RenderLineEdit::setSelectionStart(int start)
{
    int realStart = max(start, 0);
    int length = max(selectionEnd() - realStart, 0);
    static_cast<QLineEdit *>(m_widget)->setSelection(realStart, length);
}

void RenderLineEdit::setSelectionEnd(int end)
{
    int start = selectionStart();
    int realEnd = max(end, 0);
    int length = realEnd - start;
    if (length < 0) {
        start = realEnd;
        length = 0;
    }
    static_cast<QLineEdit *>(m_widget)->setSelection(start, length);
}

void RenderLineEdit::select()
{
    static_cast<QLineEdit*>(m_widget)->selectAll();
}

bool RenderLineEdit::isEdited() const
{
    return static_cast<QLineEdit*>(m_widget)->edited();
}
void RenderLineEdit::setEdited(bool x)
{
    static_cast<QLineEdit*>(m_widget)->setEdited(x);
}

void RenderLineEdit::setSelectionRange(int start, int end)
{
    int realStart = max(start, 0);
    int length = max(end - realStart, 0);
    static_cast<QLineEdit *>(m_widget)->setSelection(realStart, length);
}

} // namespace WebCore
