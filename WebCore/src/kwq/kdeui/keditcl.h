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

#ifndef KEDITCL_H_
#define KEDITCL_H_

#include <qwidget.h>
#include <qframe.h>
#include <qscrollbar.h>
#include <qstring.h>

class QTableView : public QFrame {
public:
    QScrollBar *verticalScrollBar() const;
    QScrollBar *horizontalScrollBar() const;

    virtual void setTableFlags(uint);
    void clearTableFlags(uint f = ~0);
};

const uint Tbl_vScrollBar       = 0x00000001;
const uint Tbl_hScrollBar       = 0x00000002;
const uint Tbl_autoVScrollBar   = 0x00000004;
const uint Tbl_autoHScrollBar   = 0x00000008;
const uint Tbl_autoScrollBars   = 0x0000000C;


class QMultiLineEdit : public QTableView {
public:

    enum WordWrap {
        NoWrap,
        WidgetWidth,
        FixedPixelWidth,
        FixedColumnWidth
    };    

    void setWordWrap(WordWrap);
    WordWrap wordWrap() const;
    bool hasMarkedText() const;
    bool isReadOnly() const;
    virtual void setReadOnly(bool);
    virtual void setCursorPosition(int line, int col, bool mark = FALSE);
    void getCursorPosition(int *line, int *col) const;
    virtual void setText(const QString &);
    QString text();
    QString textLine(int line) const;
    int numLines() const;
    void selectAll();
};


class KEdit : public QMultiLineEdit {
public:
    KEdit();
    KEdit(QWidget *);
};

#endif
