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

#include <KWQListBox.h>


QListBox::QListBox()
{
}


QListBox::~QListBox()
{
}


uint QListBox::count() const
{
}


void QListBox::clear()
{
}


void QListBox::setSelectionMode(SelectionMode)
{
}


QListBoxItem *QListBox::firstItem() const
{
}


int QListBox::currentItem() const
{
}


void QListBox::insertItem(const QString &, int index=-1)
{
}


void QListBox::insertItem(const QListBoxItem *, int index=-1)
{
}


void QListBox::setSelected(int, bool)
{
}


bool QListBox::isSelected(int)
{
}


// class QListBoxItem ==========================================================

QListBoxItem::QListBoxItem()
{
}


void QListBoxItem::setSelectable(bool)
{
}


QListBox *QListBoxItem::listBox() const
{
}


int QListBoxItem::width(const QListBox *) const
{
}


int QListBoxItem::height(const QListBox *) const
{
}


QListBoxItem *QListBoxItem::next() const
{
}


QListBoxItem *QListBoxItem::prev() const
{
}



// class QListBoxText ==========================================================

QListBoxText::QListBoxText(const QString &text=QString::null)
{
}


QListBoxText::~QListBoxText()
{
}

