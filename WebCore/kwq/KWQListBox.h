/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#ifndef KWQLISTBOX_H_
#define KWQLISTBOX_H_

#include <qscrollview.h>
#include <qstring.h>
#include <KWQSignal.h>

class QListBoxItem;

class QListBox : public QScrollView {
friend class QListBoxItem;
public:
    enum SelectionMode { Single, Extended };

    QListBox(QWidget *parent);
    ~QListBox();

    QSize sizeForNumberOfLines(int numLines) const;
    
    uint count() const;
    void clear();
    virtual void setSelectionMode(SelectionMode);

    QListBoxItem *firstItem() const { return _head; }

    void beginBatchInsert();
    void insertItem(const QString &, unsigned index);
    void insertItem(QListBoxItem *, unsigned index);
    void endBatchInsert();
    void setSelected(int, bool);
    bool isSelected(int) const;
    
    void clicked() { _clicked.call(); }
    void selectionChanged() { _selectionChanged.call(); }

private:
    QListBoxItem *_head;
    bool _insertingItems;
    
    KWQSignal _clicked;
    KWQSignal _selectionChanged;
};

class QListBoxItem {
friend class QListBox;
public:
    QListBoxItem(const QString &text);

    void setSelectable(bool) { }
    QListBoxItem *next() const { return _next; }
    QString text() const { return _text; }

private:
    QString _text;
    QListBoxItem *_next;

    QListBoxItem(const QListBoxItem &);
    QListBoxItem &operator=(const QListBoxItem &);
};

class QListBoxText : public QListBoxItem {
public:
    QListBoxText(const QString &text = QString::null) : QListBoxItem(text) { }
};

#endif
