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
#include <qcombobox.h>

#include <KWQView.h>

#include <kwqdebug.h>


QComboBox::QComboBox(QWidget *parent, const char *name)
{
    init(FALSE);
}


QComboBox::QComboBox(bool rw, QWidget *parent, const char *name)
{
    init(rw);
}


void QComboBox::init(bool isEditable)
{
    KWQNSComboBox *comboBox;
    
    comboBox = [[[KWQNSComboBox alloc] initWithFrame: NSMakeRect (0,0,0,0) widget: this] autorelease];
    //if (isEditable == FALSE)
    //    [comboBox setEditable: NO];
    setView (comboBox);
    
    items = [[NSMutableArray alloc] init];
}


QComboBox::~QComboBox()
{
    [items release];
}


int QComboBox::count() const
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    
    return [comboBox numberOfItems];
}


QListBox *QComboBox::listBox() const
{
    _logNotYetImplemented();
    return 0L;
}


void QComboBox::popup()
{
    _logNotYetImplemented();
}


bool QComboBox::eventFilter(QObject *object, QEvent *event)
{
    _logNotYetImplemented();
    return FALSE;
}


void QComboBox::insertItem(const QString &text, int index)
{
    int numItems = [items count];
    if (index < 0 || index == numItems)
        [items addObject:QSTRING_TO_NSSTRING(text)];
    else {
        while (index >= numItems) {
            [items addObject: @""];
            ++numItems;
        }
        [items replaceObjectAtIndex:index withObject:QSTRING_TO_NSSTRING(text)];
    }
}


QSize QComboBox::sizeHint() const 
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    
    [comboBox sizeToFit];
    
    NSRect vFrame = [comboBox frame];
    return QSize((int)vFrame.size.width,(int)vFrame.size.height);
}


void QComboBox::clear()
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    
    [comboBox removeAllItems];
}

int QComboBox::indexOfCurrentItem()
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    return [comboBox indexOfSelectedItem];
}

void QComboBox::setCurrentItem(int index)
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    int num = [comboBox numberOfItems];
    if (num != 0 && index < num)
        [comboBox selectItemAtIndex: index];
    else
        KWQDEBUG2 ("Error, index = %d, numberOfItems = %d", index, num);
}

int QComboBox::currentItem() const
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();

    return [comboBox indexOfSelectedItem];
}


