/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 *               2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES LOSS OF USE, DATA, OR
 * PROFITS OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#include <QLineEdit>

#include "GraphicsTypes.h"
#include "TextDirection.h"
#include "Widget.h"
#include "PlatformLineEdit.h"
#include "Color.h"
#include "IntSize.h"

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

PlatformLineEdit::PlatformLineEdit(PlatformLineEdit::Type type)
    : m_lineEdit(0)
    , m_type(type)
{
}

PlatformLineEdit::~PlatformLineEdit()
{
}

void PlatformLineEdit::setParentWidget(QWidget* parent)
{
    Widget::setParentWidget(parent);

    Q_ASSERT(m_lineEdit == 0);
    m_lineEdit = new QLineEdit(parent);

    if (m_type == Password)
        m_lineEdit->setEchoMode(QLineEdit::Password);

    setQWidget(m_lineEdit);
}

void PlatformLineEdit::setColors(const Color& background, const Color& foreground)
{
    notImplemented();
}

void PlatformLineEdit::setAlignment(HorizontalAlignment align)
{
    Qt::Alignment qtAlign;

    switch (align)
    {
        case AlignLeft:
            qtAlign = Qt::AlignLeft;
            break;
        case AlignRight:
            qtAlign = Qt::AlignRight;
            break;
        case AlignHCenter:
            qtAlign = Qt::AlignHCenter;
            break;
    }

    m_lineEdit->setAlignment(qtAlign);
}

void PlatformLineEdit::setCursorPosition(int pos)
{
    m_lineEdit->setCursorPosition(pos);
}

int PlatformLineEdit::cursorPosition() const
{
    return m_lineEdit->cursorPosition();
}

void PlatformLineEdit::setEdited(bool)
{
    notImplemented();
}

bool PlatformLineEdit::edited() const
{
    notImplemented();
    return false;
}

void PlatformLineEdit::setFont(const Font& font)
{
    m_lineEdit->setFont(font);
}

void PlatformLineEdit::setMaxLength(int length)
{
    m_lineEdit->setMaxLength(length);
}

int PlatformLineEdit::maxLength() const
{
    return m_lineEdit->maxLength();
}

void PlatformLineEdit::setReadOnly(bool value)
{
    m_lineEdit->setReadOnly(value);
}

bool PlatformLineEdit::isReadOnly() const
{
    return m_lineEdit->isReadOnly();
}

void PlatformLineEdit::setText(const String& str)
{
    m_lineEdit->setText(str);
}

String PlatformLineEdit::text() const
{
    return m_lineEdit->text();
}

void PlatformLineEdit::setWritingDirection(TextDirection dir)
{
    Qt::LayoutDirection qDir;

    switch(dir)
    {
        case LTR:
            qDir = Qt::LeftToRight;
            break;
        case RTL:
            qDir = Qt::RightToLeft;
            break;
    }

    m_lineEdit->setLayoutDirection(qDir);
}

void PlatformLineEdit::selectAll()
{
    m_lineEdit->selectAll();
}

bool PlatformLineEdit::hasSelectedText() const
{
    return m_lineEdit->hasSelectedText();
}

int PlatformLineEdit::selectionStart() const
{
    return m_lineEdit->selectionStart();
}

String PlatformLineEdit::selectedText() const
{
    return m_lineEdit->selectedText();
}

void PlatformLineEdit::setSelection(int start, int length)
{
    m_lineEdit->setSelection(start, length);
}

IntSize PlatformLineEdit::sizeForCharacterWidth(int numCharacters) const
{
    notImplemented();
    return IntSize();
}

int PlatformLineEdit::baselinePosition(int height) const
{
    notImplemented();
    return 0;
}

Widget::FocusPolicy PlatformLineEdit::focusPolicy() const
{
    return Widget::focusPolicy();
}

bool PlatformLineEdit::checksDescendantsForFocus() const
{
    return true;
}

void PlatformLineEdit::setLiveSearch(bool liveSearch)
{
    notImplemented();
}

void PlatformLineEdit::setAutoSaveName(const String& name)
{
    notImplemented();
}

void PlatformLineEdit::setMaxResults(int maxResults)
{
    notImplemented();
}

void PlatformLineEdit::setPlaceholderString(const String& placeholder)
{
    notImplemented();
}

void PlatformLineEdit::addSearchResult()
{
    notImplemented();
}

// vim: ts=4 sw=4 et
