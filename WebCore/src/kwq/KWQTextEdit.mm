/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#include <kwqdebug.h>

#include <qtextedit.h>

#import <KWQTextArea.h>


// class QTextEdit

QTextEdit::QTextEdit(QWidget *parent)
{
    KWQTextArea *textView;
    
    textView = [[KWQTextArea alloc] initWithFrame:NSMakeRect(0,0,0,0) widget: this];
    setView (textView);
    [textView release];
}

void QTextEdit::setText(const QString &string)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    [textView setText:QSTRING_TO_NSSTRING (string)];
}

QString QTextEdit::text()
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return NSSTRING_TO_QSTRING ([textView text]);
}

int QTextEdit::paragraphs() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return [textView paragraphs];
}

int QTextEdit::paragraphLength(int paragraph) const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return [textView paragraphLength:paragraph];
}

QString QTextEdit::text(int paragraph)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return NSSTRING_TO_QSTRING([textView textForParagraph:paragraph]);
}

int QTextEdit::lineOfChar(int paragraph, int index)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return [textView lineOfCharAtIndex:index inParagraph:paragraph];
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
    
    if ([textView wordWrap]) {
        return QTextEdit::WidgetWidth;
    } else {
	return QTextEdit::NoWrap;
    }
}

void QTextEdit::setWordWrap(WrapStyle f)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    if (f == QTextEdit::WidgetWidth) {
        [textView setWordWrap: TRUE];
    } else {
        [textView setWordWrap: FALSE];
    }
}

void QTextEdit::setTextFormat(TextFormat f)
{
    if (f != PlainText) {
	_logNotYetImplemented();
    }
}

bool QTextEdit::isReadOnly () const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return [textView isEditable] ? NO : YES;
}

void QTextEdit::setReadOnly (bool flag)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView setEditable: (BOOL)flag ? NO : YES];
}

void QTextEdit::selectAll()
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    [textView selectAll];
}

int QTextEdit::verticalScrollBarWidth() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return (int) [[textView verticalScroller] frame].size.width;
}

int QTextEdit::horizontalScrollBarHeight() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    return (int) [[textView horizontalScroller] frame].size.height;
}

