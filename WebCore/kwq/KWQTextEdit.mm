/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQTextEdit.h"

#import "KWQAssertions.h"
#import "KWQTextArea.h"

QTextEdit::QTextEdit(QWidget *parent)
    : _clicked(this, SIGNAL(clicked()))
    , _textChanged(this, SIGNAL(textChanged()))
{
    KWQTextArea *textView = [[KWQTextArea alloc] initWithQTextEdit:this];
    setView(textView);
    [textView release];
}

void QTextEdit::setText(const QString &string)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView setText:string.getNSString()];
}

QString QTextEdit::text() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return QString::fromNSString([textView text]);
}

QString QTextEdit::textWithHardLineBreaks() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return QString::fromNSString([textView textWithHardLineBreaks]);
}

void QTextEdit::getCursorPosition(int *paragraph, int *index) const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView getCursorPositionAsIndex:index inParagraph:paragraph];
}

void QTextEdit::setCursorPosition(int paragraph, int index)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView setCursorPositionToIndex:index inParagraph:paragraph];
}

QTextEdit::WrapStyle QTextEdit::wordWrap() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return [textView wordWrap] ? WidgetWidth : NoWrap;
}

void QTextEdit::setWordWrap(WrapStyle style)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView setWordWrap:style == WidgetWidth];
}

void QTextEdit::setTextFormat(TextFormat)
{
}

void QTextEdit::setTabStopWidth(int)
{
}

bool QTextEdit::isReadOnly() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return ![textView isEditable];
}

void QTextEdit::setReadOnly(bool flag)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView setEditable:!flag];
}

void QTextEdit::selectAll()
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView selectAll];
}

int QTextEdit::verticalScrollBarWidth() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return (int)[[textView verticalScroller] frame].size.width;
}

int QTextEdit::horizontalScrollBarHeight() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return (int)[[textView horizontalScroller] frame].size.height;
}

void QTextEdit::setFont(const QFont &font)
{
    QWidget::setFont(font);
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView setFont:font.getNSFont()];
}

QWidget::FocusPolicy QTextEdit::focusPolicy() const
{
    return TabFocus;
}

void QTextEdit::clicked()
{
    _clicked.call();
}

void QTextEdit::setAlignment(AlignmentFlags alignment)
{
    ASSERT(alignment == AlignLeft || alignment == AlignRight);
    [(KWQTextArea *)getView() setAlignment:(alignment == AlignRight ? NSRightTextAlignment : NSLeftTextAlignment)];
}
