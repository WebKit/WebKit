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

#include <KWQView.h>
#include <KWQListBox.h>


// Emulated with a NSScrollView that contains a NSMatrix.  Use a prototype cell.
QListBox::QListBox()
{
    KWQNSScrollView *scrollview = [[NSScrollView alloc] initWithFrame: NSMakeRect (0,0,0,0) widget: this];
    NSButtonCell *cell = [[[NSButtonCell alloc] init] autorelease];
    
    [cell setBordered: NO];
    [cell setAlignment: NSLeftTextAlignment];
       
    matrix  = [[NSMatrix alloc] initWithFrame: NSMakeRect (0,0,0,0) mode:NSHighlightModeMatrix prototype:cell numberOfRows:0 numberOfColumns:0];
    
    [scrollview setHasVerticalScroller: NO];
    [scrollview setHasHorizontalScroller: YES];
    [scrollview setDocumentView: matrix];

    head = 0L;
    
    setView (scrollview);
}


QListBox::~QListBox()
{
    KWQNSScrollView *scrollview = (KWQNSScrollView *)getView();

    [scrollview setDocumentView: nil];
    [matrix release];
}


uint QListBox::count() const
{
    (uint)[matrix numberOfRows];
}


void QListBox::clear()
{
    [matrix renewRows: 0 columns: 0];
    [matrix sizeToCells];
}


void QListBox::setSelectionMode(SelectionMode mode)
{
    if (mode == QListBox::Extended){
        [matrix setMode: NSListModeMatrix];
    }
    else {
        [matrix setMode: NSHighlightModeMatrix];
    }
}


QListBoxItem *QListBox::firstItem() const
{
    return head;
}


int QListBox::currentItem() const
{
    return [matrix selectedRow];
}


void QListBox::insertItem(const QString &t, int index)
{
    insertItem( new QListBoxText(t), index );
}


void QListBox::insertItem(const QListBoxItem *newItem, int index)
{
    if ( !newItem )
	return;

    if ( index < 0 || index >= count())
	index = count();

    QListBoxItem *item = (QListBoxItem *)newItem;

    item->box = this;
    if ( !head || index == 0 ) {
	item->nextItem = head;
	item->previousItem = 0;
	head = item;
	if ( item->nextItem )
	    item->nextItem->previousItem = item;
    } else {
	QListBoxItem * i = head;
	while ( i->nextItem && index > 1 ) {
	    i = i->nextItem;
	    index--;
	}
	if ( i->nextItem ) {
	    item->nextItem = i->nextItem;
	    item->previousItem = i;
	    item->nextItem->previousItem = item;
	    item->previousItem->nextItem = item;
	} else {
	    i->nextItem = item;
	    item->previousItem = i;
	    item->nextItem = 0;
	}
    }
    [matrix insertRow: index];
    
    NSButtonCell *cell = [matrix cellAtRow: index column: 0];
    [cell setTitle:  QSTRING_TO_NSSTRING(item->text)];
    
    item->cell = [cell retain];
}

void QListBox::setSelected(int index, bool selectIt)
{
    if (selectIt)
        [matrix selectCellAtRow: index column: 0];
    else
        [matrix deselectSelectedCell];
}


bool QListBox::isSelected(int index)
{
    return [matrix selectedRow] == index;
}


// class QListBoxItem ==========================================================

QListBoxItem::QListBoxItem()
{
    cell = 0L;
}

QListBoxItem::~QListBoxItem()
{
    [cell release];
}


void QListBoxItem::setSelectable(bool flag)
{
    [cell setSelectable: flag];
}


QListBox *QListBoxItem::listBox() const
{
    return box;
}


int QListBoxItem::width(const QListBox *) const
{
    // Is this right?
    NSSize size = [cell cellSizeForBounds: NSMakeRect (0,0,10000,10000)];
    return (int)size.width;
}


int QListBoxItem::height(const QListBox *) const
{
    // Is this right?
    NSSize size = [cell cellSizeForBounds: NSMakeRect (0,0,10000,10000)];
    return (int)size.height;
}


QListBoxItem *QListBoxItem::next() const
{
    return nextItem;
}


QListBoxItem *QListBoxItem::prev() const
{
    return previousItem;
}



// class QListBoxText ==========================================================

QListBoxText::QListBoxText(const QString &t)
{
    text = t;
}


QListBoxText::~QListBoxText()
{
}

