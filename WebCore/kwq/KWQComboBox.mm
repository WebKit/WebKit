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

#import <qcombobox.h>

#import <KWQView.h>

#import <kwqdebug.h>

// We empirically determined that combo boxes have these extra pixels on all
// sides. It would be better to get this info from AppKit somehow.
#define TOP_MARGIN 1
#define BOTTOM_MARGIN 3
#define LEFT_MARGIN 3
#define RIGHT_MARGIN 3

QComboBox::QComboBox(QWidget *parent, const char *name)
    : m_activated(this, SIGNAL(activated(int)))
{
    init(false);
}

QComboBox::QComboBox(bool rw, QWidget *parent, const char *name)
    : m_activated(this, SIGNAL(activated(int)))
{
    init(rw);
}

void QComboBox::init(bool isEditable)
{
    KWQNSComboBox *comboBox = [[KWQNSComboBox alloc] initWithWidget:this];
    setView(comboBox);
    [comboBox release];
    
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
    return 0;
}

void QComboBox::popup()
{
}

bool QComboBox::eventFilter(QObject *object, QEvent *event)
{
    _logNotYetImplemented();
    return FALSE;
}

void QComboBox::insertItem(const QString &text, int index)
{
    NSString *string;
    int numItems = [items count];
    string = [text.getNSString() copy];
    if (index < 0 || index == numItems) {
        [items addObject:string];
    } else {
        while (index >= numItems) {
            [items addObject: @""];
            ++numItems;
        }
        [items replaceObjectAtIndex:index withObject:string];
    }
    [string release];
}

QSize QComboBox::sizeHint() const 
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    return QSize((int)[[comboBox cell] cellSize].width - (LEFT_MARGIN + RIGHT_MARGIN),
        (int)[[comboBox cell] cellSize].height - (TOP_MARGIN + BOTTOM_MARGIN));
}

QRect QComboBox::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + LEFT_MARGIN, r.y() + TOP_MARGIN,
        r.width() - (LEFT_MARGIN + RIGHT_MARGIN),
        r.height() - (TOP_MARGIN + BOTTOM_MARGIN));
}

void QComboBox::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(r.x() - LEFT_MARGIN, r.y() - TOP_MARGIN,
        r.width() + LEFT_MARGIN + RIGHT_MARGIN,
        r.height() + TOP_MARGIN + BOTTOM_MARGIN));
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
        [comboBox selectItemAtIndex:index];
    else
        KWQDEBUG("Error, index = %d, numberOfItems = %d", index, num);
}

int QComboBox::currentItem() const
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    return [comboBox indexOfSelectedItem];
}
