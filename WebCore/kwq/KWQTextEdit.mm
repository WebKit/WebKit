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

#include <keditcl.h>


QTableView::QTableView()
{
}


QTableView::~QTableView()
{
}


QScrollBar *QTableView::verticalScrollBar() const
{
}


QScrollBar *QTableView::horizontalScrollBar() const
{
}


void QTableView::setTableFlags(uint)
{
}


void QTableView::clearTableFlags(uint f = ~0)
{
}


QMultiLineEdit::QMultiLineEdit()
{
}


QMultiLineEdit::~QMultiLineEdit()
{
}


void QMultiLineEdit::setWordWrap(WordWrap)
{
}


QMultiLineEdit::WordWrap QMultiLineEdit::wordWrap() const
{
}


bool QMultiLineEdit::hasMarkedText() const
{
}


bool QMultiLineEdit::isReadOnly() const
{
}


void QMultiLineEdit::setReadOnly(bool)
{
}


void QMultiLineEdit::setCursorPosition(int line, int col, bool mark = FALSE)
{
}


void QMultiLineEdit::getCursorPosition(int *line, int *col) const
{
}


void QMultiLineEdit::setText(const QString &)
{
}


QString QMultiLineEdit::text()
{
}


QString QMultiLineEdit::textLine(int line) const
{
}


int QMultiLineEdit::numLines() const
{
}


void QMultiLineEdit::selectAll()
{
}


KEdit::KEdit()
{
}


KEdit::KEdit(QWidget *)
{
}


KEdit::~KEdit()
{
}


