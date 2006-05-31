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
#include "RenderTextArea.h"

#include "KWQTextEdit.h"
#include "HTMLTextAreaElement.h"
#include "FrameView.h"

namespace WebCore {

RenderTextArea::RenderTextArea(HTMLTextAreaElement* element)
    : RenderFormElement(element)
    , m_dirty(false)
    , m_updating(false)
{
    QTextEdit* edit = new QTextEdit(m_view);

    if (element->wrap() != HTMLTextAreaElement::ta_NoWrap)
        edit->setWordWrap(QTextEdit::WidgetWidth);
    else
        edit->setWordWrap(QTextEdit::NoWrap);

    setWidget(edit);
}

void RenderTextArea::destroy()
{
    static_cast<HTMLTextAreaElement*>(node())->rendererWillBeDestroyed();
    RenderFormElement::destroy();
}

void RenderTextArea::calcMinMaxWidth()
{
    ASSERT(!minMaxKnown());

    QTextEdit* w = static_cast<QTextEdit*>(m_widget);
    IntSize size(w->sizeWithColumnsAndRows(
        max(static_cast<HTMLTextAreaElement*>(node())->cols(), 1),
        max(static_cast<HTMLTextAreaElement*>(node())->rows(), 1)));

    setIntrinsicWidth(size.width());
    setIntrinsicHeight(size.height());

    RenderFormElement::calcMinMaxWidth();
}

void RenderTextArea::setStyle(RenderStyle* s)
{
    RenderFormElement::setStyle(s);

    QTextEdit* w = static_cast<QTextEdit*>(m_widget);
    w->setAlignment(textAlignment());
    w->setLineHeight(RenderObject::lineHeight(true));

    w->setWritingDirection(style()->direction() == RTL ? RTL : LTR);

    ScrollBarMode scrollMode = ScrollBarAuto;
    switch (style()->overflow()) {
        case OAUTO:
        case OMARQUEE: // makes no sense, map to auto
        case OOVERLAY: // not implemented for text, map to auto
        case OVISIBLE:
            break;
        case OHIDDEN:
            scrollMode = ScrollBarAlwaysOff;
            break;
        case OSCROLL:
            scrollMode = ScrollBarAlwaysOn;
            break;
    }
    ScrollBarMode horizontalScrollMode = scrollMode;
    if (static_cast<HTMLTextAreaElement*>(node())->wrap() != HTMLTextAreaElement::ta_NoWrap)
        horizontalScrollMode = ScrollBarAlwaysOff;

    w->setScrollBarModes(horizontalScrollMode, scrollMode);
}

void RenderTextArea::setEdited(bool x)
{
    m_dirty = x;
}

void RenderTextArea::updateFromElement()
{
    HTMLTextAreaElement* e = static_cast<HTMLTextAreaElement*>(node());
    QTextEdit* w = static_cast<QTextEdit*>(m_widget);

    w->setReadOnly(e->isReadOnlyControl());
    w->setDisabled(e->disabled());

    String widgetText = text();
    String text = e->value();
    text.replace('\\', backslashAsCurrencySymbol());
    if (widgetText != text) {
        int line, col;
        w->getCursorPosition(&line, &col);
        m_updating = true;
        w->setText(text);
        m_updating = false;
        w->setCursorPosition(line, col);
    }
    m_dirty = false;

    w->setColors(style()->backgroundColor(), style()->color());

    RenderFormElement::updateFromElement();
}

String RenderTextArea::text()
{
    String txt = static_cast<QTextEdit*>(m_widget)->text();
    return txt.replace(backslashAsCurrencySymbol(), '\\');
}

String RenderTextArea::textWithHardLineBreaks()
{
    String txt = static_cast<QTextEdit*>(m_widget)->textWithHardLineBreaks();
    return txt.replace(backslashAsCurrencySymbol(), '\\');
}

void RenderTextArea::valueChanged(Widget*)
{
    if (m_updating)
        return;
    static_cast<HTMLTextAreaElement*>(node())->setValueMatchesRenderer(false);
    m_dirty = true;
}

int RenderTextArea::selectionStart()
{
    return static_cast<QTextEdit*>(m_widget)->selectionStart();
}

int RenderTextArea::selectionEnd()
{
    return static_cast<QTextEdit*>(m_widget)->selectionEnd();
}

void RenderTextArea::setSelectionStart(int start)
{
    static_cast<QTextEdit*>(m_widget)->setSelectionStart(start);
}

void RenderTextArea::setSelectionEnd(int end)
{
    static_cast<QTextEdit*>(m_widget)->setSelectionEnd(end);
}

void RenderTextArea::select()
{
    static_cast<QTextEdit*>(m_widget)->selectAll();
}

void RenderTextArea::setSelectionRange(int start, int end)
{
    QTextEdit* textEdit = static_cast<QTextEdit*>(m_widget);
    textEdit->setSelectionRange(start, end-start);
}

void RenderTextArea::selectionChanged(Widget*)
{
    QTextEdit* w = static_cast<QTextEdit*>(m_widget);

    // We only want to call onselect if there actually is a selection
    if (!w->hasSelectedText())
        return;
    
    static_cast<HTMLTextAreaElement*>(node())->onSelect();
}

} // namespace WebCore
