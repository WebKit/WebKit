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
#import <kwqdebug.h>

#import <KWQView.h>
#import <KWQListBox.h>


@interface KWQBrowserDelegate : NSObject
{
    QListBox *box;
}

@end

@implementation KWQBrowserDelegate

- initWithListBox: (QListBox *)b
{
    [super init];
    box = b;
    return self;
}

// ==========================================================
// Browser Delegate Methods.
// ==========================================================

// Use lazy initialization, since we don't want to touch the file system too much.
- (int)browser:(NSBrowser *)sender numberOfRowsInColumn:(int)column
{
    return box->count();
}

- (void)browser:(NSBrowser *)sender willDisplayCell:(id)cell atRow:(int)row column:(int)column
{
    QListBoxItem *item = box->firstItem();
    int count = 0;
    
    while (item != 0){
        if (count == row){
            if (item->text.isEmpty())
                [cell setEnabled: NO];
            else
                [cell setEnabled: YES];
            [cell setLeaf: YES];
            [cell setStringValue: item->text.getNSString()];
            return;
        }
        item = item->nextItem;
        count++;
    }
}

// ==========================================================
// Browser Target / Action Methods.
// ==========================================================

- (IBAction)browserSingleClick:(id)browser
{
    box->emitAction(QObject::ACTION_LISTBOX_CLICKED);
}

- (IBAction)browserDoubleClick:(id)browser
{
}

@end


QListBox::QListBox(QWidget *parent)
    : QScrollView(parent), m_insertingItems(false)
{
    NSBrowser *browser =  [[NSBrowser alloc] initWithFrame: NSMakeRect (0,0,1,1)];
    KWQBrowserDelegate *delegate = [[KWQBrowserDelegate alloc] initWithListBox: this];

    head = 0L;

    // FIXME:  Need to configure browser's scroll view to use a scroller that
    // has no up/down buttons.  Typically the browser is very small and won't
    // have enough space to draw the up/down buttons and the scroll knob.
    [browser setTitled: NO];
    [browser setDelegate: delegate];
    [browser setTarget: delegate];
    [browser setAction: @selector(browserSingleClick:)];
    [browser addColumn];
    
    setView (browser);
    
    [browser release];
}


QListBox::~QListBox()
{
    NSBrowser *browser = (NSBrowser *)getView();
    KWQBrowserDelegate *delegate = [browser delegate];
    [browser setDelegate: nil];
    [delegate release];
}


uint QListBox::count() const
{
    QListBoxItem *item = (QListBoxItem *)head;
    int count = 0;
    
    while (item != 0){
        item = item->nextItem;
        count++;
    }
    return count;
}


int QListBox::scrollBarWidth() const
{
    return (int)[NSScroller scrollerWidth];
}


void QListBox::clear()
{
    NSBrowser *browser = (NSBrowser *)getView();

    // Do we need to delete previous head?
    head = 0;
    
    [browser loadColumnZero];
}


void QListBox::setSelectionMode(SelectionMode mode)
{
    NSBrowser *browser = (NSBrowser *)getView();

    if (mode == QListBox::Extended){
        [browser setAllowsMultipleSelection: YES];
    }
    else {
        [browser setAllowsMultipleSelection: NO];
    }
}


QListBoxItem *QListBox::firstItem() const
{
    return head;
}


int QListBox::currentItem() const
{
    NSBrowser *browser = (NSBrowser *)getView();
    return [browser selectedRowInColumn:0];
}


void QListBox::insertItem(const QString &t, int index)
{
    insertItem( new QListBoxText(t), index );
}


void QListBox::insertItem(const QListBoxItem *newItem, int _index)
{
    if ( !newItem )
	    return;

    if ( _index < 0 || _index >= (int)count())
	    _index = count();

    int index = _index;
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

    if (!m_insertingItems) {
        NSBrowser *browser = (NSBrowser *)getView();
        [browser loadColumnZero];
        [browser tile];
    }
}

void QListBox::beginBatchInsert()
{
    m_insertingItems = true;
}

void QListBox::endBatchInsert()
{
    m_insertingItems = false;
    NSBrowser *browser = (NSBrowser *)getView();
    [browser loadColumnZero];
    [browser tile];
}

void QListBox::setSelected(int index, bool selectIt)
{
    if (selectIt) {
        NSBrowser *browser = (NSBrowser *)getView();
        [browser selectRow: index inColumn: 0];
    }
}


bool QListBox::isSelected(int index)
{
    NSBrowser *browser = (NSBrowser *)getView();
    return [[browser loadedCellAtRow: index column:0] state] == NSOnState; 
}


// class QListBoxItem ==========================================================

QListBoxItem::QListBoxItem()
{
}

QListBoxItem::~QListBoxItem()
{
}


void QListBoxItem::setSelectable(bool flag)
{
    // Not implemented, RenderSelect calls this when an item element is ""
    // We handle that case directly in our browser delegate.
}


QListBox *QListBoxItem::listBox() const
{
    return box;
}


int QListBoxItem::width(const QListBox *) const
{
    // Is this right?
    NSBrowser *browser = (NSBrowser *)box->getView();
    NSSize cellSize = [[browser loadedCellAtRow: 0 column: 0] cellSizeForBounds: NSMakeRect (0,0,10000,10000)];
    NSSize frameSize = [NSScrollView frameSizeForContentSize:cellSize hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSLineBorder];
    return (int)frameSize.width;
}


int QListBoxItem::height(const QListBox *) const
{
    // Is this right?
    NSBrowser *browser = (NSBrowser *)box->getView();
    NSSize size = [[browser loadedCellAtRow: 0 column: 0] cellSizeForBounds: NSMakeRect (0,0,10000,10000)];
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

