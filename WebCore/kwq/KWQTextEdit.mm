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
#import "KWQExceptions.h"
#import "KWQTextArea.h"

QTextEdit::QTextEdit(QWidget *parent)
    : _clicked(this, SIGNAL(clicked()))
    , _textChanged(this, SIGNAL(textChanged()))
{
    KWQ_BLOCK_NS_EXCEPTIONS;
    KWQTextArea *textView = [[KWQTextArea alloc] initWithQTextEdit:this];
    setView(textView);
    [textView release];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QTextEdit::setText(const QString &string)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    KWQ_BLOCK_NS_EXCEPTIONS;
    [textView setText:string.getNSString()];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

QString QTextEdit::text() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    volatile NSString * volatile text = @"";

    KWQ_BLOCK_NS_EXCEPTIONS;
    text = [textView text];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return QString::fromNSString((NSString *)text);
}

QString QTextEdit::textWithHardLineBreaks() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    volatile NSString * volatile text = @"";

    KWQ_BLOCK_NS_EXCEPTIONS;
    text = [textView textWithHardLineBreaks];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
    
    return QString::fromNSString((NSString *)text);
}

void QTextEdit::getCursorPosition(int *paragraph, int *index) const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    if (index)
	*index = 0;
    if (paragraph)
	*paragraph = 0;

    KWQ_BLOCK_NS_EXCEPTIONS;
    [textView getCursorPositionAsIndex:index inParagraph:paragraph];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QTextEdit::setCursorPosition(int paragraph, int index)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    [textView setCursorPositionToIndex:index inParagraph:paragraph];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

QTextEdit::WrapStyle QTextEdit::wordWrap() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    volatile bool wrap = false;
    KWQ_BLOCK_NS_EXCEPTIONS;
    wrap = [textView wordWrap];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return wrap  ? WidgetWidth : NoWrap;
}

void QTextEdit::setWordWrap(WrapStyle style)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    KWQ_BLOCK_NS_EXCEPTIONS;
    [textView setWordWrap:style == WidgetWidth];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
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
    volatile bool result = false;

    KWQ_BLOCK_NS_EXCEPTIONS;
    result = ![textView isEditable];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return result;
}

void QTextEdit::setReadOnly(bool flag)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    [textView setEditable:!flag];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QTextEdit::selectAll()
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    [textView selectAll];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QTextEdit::setFont(const QFont &font)
{
    QWidget::setFont(font);
    KWQTextArea *textView = (KWQTextArea *)getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    [textView setFont:font.getNSFont()];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

void QTextEdit::clicked()
{
    _clicked.call();
}

void QTextEdit::setAlignment(AlignmentFlags alignment)
{
    ASSERT(alignment == AlignLeft || alignment == AlignRight);
    KWQTextArea *textArea = getView();

    KWQ_BLOCK_NS_EXCEPTIONS;
    [textArea setAlignment:(alignment == AlignRight ? NSRightTextAlignment : NSLeftTextAlignment)];
    KWQ_UNBLOCK_NS_EXCEPTIONS;
}

QSize QTextEdit::sizeWithColumnsAndRows(int numColumns, int numRows) const
{
    KWQTextArea *textArea = getView();
    NSSize size = {0,0};

    KWQ_BLOCK_NS_EXCEPTIONS;
    size = [textArea sizeWithColumns:numColumns rows:numRows];
    KWQ_UNBLOCK_NS_EXCEPTIONS;

    return QSize((int)ceil(size.width), (int)ceil(size.height));
}

bool QTextEdit::checksDescendantsForFocus() const
{
    return true;
}

