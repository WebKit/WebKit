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

#include <keditcl.h>

#import <KWQTextArea.h>


// RenderTextArea actually uses a TextAreaWidget->KEdit->QMultiLineEdit->QTableView

QTableView::QTableView()
{
    // Nothing needed.
    _logNotYetImplemented();
}


QTableView::~QTableView()
{
    // Nothing needed.
    _logNotYetImplemented();
}


QScrollBar *QTableView::verticalScrollBar() const
{
    // Nothing needed.
    _logNeverImplemented();
}


QScrollBar *QTableView::horizontalScrollBar() const
{
    // Nothing needed.
    _logNeverImplemented();
}


void QTableView::setTableFlags(uint)
{
    // Nothing needed.
    _logNeverImplemented();
}


void QTableView::clearTableFlags(uint f = ~0)
{
    // Nothing needed.
    _logNeverImplemented();
}


QMultiLineEdit::QMultiLineEdit()
{
    // Nothing needed.
    _logNotYetImplemented();
}


QMultiLineEdit::~QMultiLineEdit()
{
    // Nothing needed.
    _logNotYetImplemented();
}


void QMultiLineEdit::setWordWrap(WordWrap f)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    if (f == QMultiLineEdit::WidgetWidth)
        [textView setWordWrap: TRUE];
    else
        [textView setWordWrap: FALSE];
}


QMultiLineEdit::WordWrap QMultiLineEdit::wordWrap() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    if ([textView wordWrap])
        return QMultiLineEdit::WidgetWidth;
    return QMultiLineEdit::NoWrap;
}


bool QMultiLineEdit::hasMarkedText() const
{
    // We can safely ignore this method.
    _logNeverImplemented();
    return false;
}


bool QMultiLineEdit::isReadOnly() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    return [textView isEditable];
}


void QMultiLineEdit::setReadOnly(bool flag)
{
    KWQTextArea *textView = (KWQTextArea *)getView();

    [textView setEditable: (BOOL)flag];
}


void QMultiLineEdit::setCursorPosition(int line, int col, bool mark = FALSE)
{
    // We can safely ignore this method.
    _logNeverImplemented();
}


void QMultiLineEdit::getCursorPosition(int *line, int *col) const
{
    // We can safely ignore this method.
    _logNeverImplemented();
}


void QMultiLineEdit::setText(const QString &string)
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    [textView setText: QSTRING_TO_NSSTRING (string)];
}


QString QMultiLineEdit::text()
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    return NSSTRING_TO_QSTRING ([textView text]);
}


QString QMultiLineEdit::textLine(int line) const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    return NSSTRING_TO_QSTRING([textView textForLine: line]);
}


int QMultiLineEdit::numLines() const
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    return [textView numLines];
}


void QMultiLineEdit::selectAll()
{
    KWQTextArea *textView = (KWQTextArea *)getView();
    
    [textView selectAll];
}


KEdit::KEdit()
{
    _logNeverImplemented();
}


KEdit::KEdit(QWidget *w)
{
    KWQTextArea *textView;
    
    textView = [[KWQTextArea alloc] initWithFrame: NSMakeRect (0,0,0,0) widget: this];
    setView (textView);
    [textView release];
}


KEdit::~KEdit()
{
}


