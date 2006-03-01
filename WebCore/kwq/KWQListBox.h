/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "KWQPainter.h"
#include "ScrollView.h"

enum KWQListBoxItemType {
    KWQListBoxOption,
    KWQListBoxGroupLabel,
    KWQListBoxSeparator
};

struct KWQListBoxItem
{
    QString string;
    KWQListBoxItemType type;
    bool enabled;
    
    KWQListBoxItem(const QString &s, KWQListBoxItemType t, bool e) : string(s), type(t), enabled(e) { }
};

class QListBox : public WebCore::ScrollView {
public:
    enum SelectionMode { Single, Extended };

    QListBox();
    ~QListBox();

    IntSize sizeForNumberOfLines(int numLines) const;
    
    uint count() const { return _items.count(); }

    void setSelectionMode(SelectionMode);

    void clear();
    void appendItem(const QString &s, bool enabled) { appendItem(s, KWQListBoxOption, enabled); }
    void appendGroupLabel(const QString &s, bool enabled) { appendItem(s, KWQListBoxGroupLabel, enabled); }
    void doneAppendingItems();

    void setSelected(int, bool);
    bool isSelected(int) const;

    void setEnabled(bool enabled);
    bool isEnabled();
    
    const KWQListBoxItem &itemAtIndex(int index) const { return _items[index]; }
    
    void setWritingDirection(QPainter::TextDirection);
    
    bool changingSelection() { return _changingSelection; }
    void clicked() { _clicked.call(); }
    void selectionChanged() { _selectionChanged.call(); }

    virtual FocusPolicy focusPolicy() const;
    virtual bool checksDescendantsForFocus() const;
    
    static void clearCachedTextRenderers();
    void setFont(const WebCore::Font&);

private:
    void appendItem(const QString &, KWQListBoxItemType, bool);

    // A Vector<KWQListBoxItem> might be more efficient for large lists.
    QValueList<KWQListBoxItem> _items;

    bool _changingSelection;
    bool _enabled;

    mutable float _width;
    mutable bool _widthGood;
    
    KWQSignal _clicked;
    KWQSignal _selectionChanged;
};

#endif
