/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <QDebug>
#include <QTextEdit>

#include <config.h>
#include "GraphicsTypes.h"
#include "ScrollView.h"
#include "TextDirection.h"
#include "PlatformTextEdit.h"
#include "PlatformString.h"
#include "IntSize.h"

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

PlatformTextEdit::PlatformTextEdit(Widget* parent)
    : ScrollView()
{
    qDebug("PlatformTextEdit::PlatformTextEdit(), this=%p", this);
}

PlatformTextEdit::~PlatformTextEdit()
{
    qDebug("PlatformTextEdit::~PlatformTextEdit()");
}

void PlatformTextEdit::setParentWidget(QWidget* parent)
{
    qDebug("PlatformTextEdit::setParentWidget(), parent=%p", parent);
    Widget::setParentWidget(parent);

    QTextEdit *widget = new QTextEdit(parent, "");
    setQWidget(widget);
}

void PlatformTextEdit::setColors(const Color& background, const Color& foreground)
{
    notImplemented();
}

void PlatformTextEdit::setAlignment(HorizontalAlignment)
{
    notImplemented();
}

void PlatformTextEdit::setLineHeight(int lineHeight)
{
    notImplemented();
}

void PlatformTextEdit::setCursorPosition(int, int)
{
    notImplemented();
}

void PlatformTextEdit::getCursorPosition(int *, int *) const
{
    notImplemented();
}

void PlatformTextEdit::setFont(const Font&)
{
    notImplemented();
}

void PlatformTextEdit::setReadOnly(bool)
{
    notImplemented();
}

bool PlatformTextEdit::isReadOnly() const
{
    notImplemented();
    return false;
}

void PlatformTextEdit::setDisabled(bool)
{
    notImplemented();
}

bool PlatformTextEdit::isDisabled() const
{
    notImplemented();
    return false;
}

bool PlatformTextEdit::hasSelectedText() const
{
    notImplemented();
    return false;
}

void PlatformTextEdit::setText(const String&)
{
    notImplemented();
}

String PlatformTextEdit::text() const
{
    notImplemented();
}

String PlatformTextEdit::textWithHardLineBreaks() const
{
    notImplemented();
}

Widget::FocusPolicy PlatformTextEdit::focusPolicy() const
{
    return Widget::focusPolicy();
}

void PlatformTextEdit::setWordWrap(PlatformTextEdit::WrapStyle)
{
    notImplemented();
}

PlatformTextEdit::WrapStyle PlatformTextEdit::wordWrap() const
{
    notImplemented();
}

void PlatformTextEdit::setScrollBarModes(ScrollBarMode hMode, ScrollBarMode vMode)
{
    notImplemented();
}

void PlatformTextEdit::setWritingDirection(TextDirection)
{
    notImplemented();
}

int PlatformTextEdit::selectionStart()
{
    notImplemented();
    return 0;
}

int PlatformTextEdit::selectionEnd()
{
    notImplemented();
    return 0;
}

void PlatformTextEdit::setSelectionStart(int)
{
    notImplemented();
}

void PlatformTextEdit::setSelectionEnd(int)
{
    notImplemented();
}

void PlatformTextEdit::selectAll()
{
    notImplemented();
}

void PlatformTextEdit::setSelectionRange(int, int)
{
    notImplemented();
}

IntSize PlatformTextEdit::sizeWithColumnsAndRows(int numColumns, int numRows) const
{
    notImplemented();
}

bool PlatformTextEdit::checksDescendantsForFocus() const
{
    notImplemented();
    return false;
}

}

// vim: ts=4 sw=4 et
