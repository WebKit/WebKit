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

#import "KWQListBox.h"

#import "KWQAssertions.h"

// FIXME: We have to switch to NSTableView instead of NSBrowser.

#define BORDER_TOTAL_WIDTH 3
#define BORDER_TOTAL_HEIGHT 2

@interface NSBrowser (KWQNSBrowserSecrets)
- (void)_setScrollerSize:(NSControlSize)scrollerSize;
@end

@interface KWQBrowserDelegate : NSObject
{
    QListBox *box;
}
- initWithListBox:(QListBox *)b;
@end

@implementation KWQBrowserDelegate

- initWithListBox:(QListBox *)b
{
    [super init];
    box = b;
    return self;
}

- (int)browser:(NSBrowser *)sender numberOfRowsInColumn:(int)column
{
    return box->count();
}

- (void)browser:(NSBrowser *)sender willDisplayCell:(id)cell atRow:(int)row column:(int)column
{
    int count = 0;
    for (QListBoxItem *item = box->firstItem(); item; item = item->next()) {
        if (count == row) {
            [cell setEnabled:!item->text().isEmpty()];
            [cell setLeaf:YES];
            [cell setStringValue:item->text().getNSString()];
            break;
        }
        count++;
    }
}

- (IBAction)browserSingleClick:(id)browser
{
    box->selectionChanged();
    box->clicked();
}

@end

QListBox::QListBox(QWidget *parent)
    : QScrollView(parent), _head(0), _insertingItems(false)
    , _clicked(this, SIGNAL(clicked(QListBoxItem *)))
    , _selectionChanged(this, SIGNAL(selectionChanged()))
{
    NSBrowser *browser = [[NSBrowser alloc] init];
    KWQBrowserDelegate *delegate = [[KWQBrowserDelegate alloc] initWithListBox:this];

    // FIXME: Need to configure browser's scroll view to use a scroller that
    // has no up/down buttons. Typically the browser is very small and won't
    // have enough space to draw the up/down buttons and the scroll knob.
    [browser setTitled:NO];
    [browser setDelegate:delegate];
    [browser setTarget:delegate];
    [browser setAction:@selector(browserSingleClick:)];
    [browser addColumn];
    [browser setMaxVisibleColumns:1];
    [browser _setScrollerSize:NSSmallControlSize];
    
    setView(browser);
    
    [browser release];
}

QListBox::~QListBox()
{
    NSBrowser *browser = (NSBrowser *)getView();
    KWQBrowserDelegate *delegate = [browser delegate];
    [browser setDelegate:nil];
    [delegate release];
}

uint QListBox::count() const
{
    int count = 0;
    for (QListBoxItem *item = _head; item; item = item->next()) {
        count++;
    }
    return count;
}

void QListBox::clear()
{
    QListBoxItem *next;
    for (QListBoxItem *item = _head; item; item = next) {
        next = item->next();
        delete item;
    }
    _head = 0;
    
    NSBrowser *browser = (NSBrowser *)getView();
    [browser loadColumnZero];
}

void QListBox::setSelectionMode(SelectionMode mode)
{
    NSBrowser *browser = (NSBrowser *)getView();
    [browser setAllowsMultipleSelection:mode != Single];
}

void QListBox::insertItem(const QString &text, unsigned index)
{
    insertItem(new QListBoxItem(text), index);
}

void QListBox::insertItem(QListBoxItem *item, unsigned index)
{
    if (!item)
        return;

    if (index > count())
        index = count();

    if (!_head || index == 0) {
        item->_next = _head;
        _head = item;
    } else {
        QListBoxItem *i = _head;
        while (i->_next && index > 1) {
            i = i->_next;
            index--;
        }
        item->_next = i->_next;
        i->_next = item;
    }

    if (!_insertingItems) {
        NSBrowser *browser = (NSBrowser *)getView();
        [browser loadColumnZero];
        [browser tile];
    }
}

void QListBox::beginBatchInsert()
{
    ASSERT(!_insertingItems);
    _insertingItems = true;
}

void QListBox::endBatchInsert()
{
    ASSERT(_insertingItems);
    _insertingItems = false;
    NSBrowser *browser = (NSBrowser *)getView();
    [browser loadColumnZero];
    [browser tile];
}

void QListBox::setSelected(int index, bool selectIt)
{
    ASSERT(!_insertingItems);
    if (selectIt) {
        NSBrowser *browser = (NSBrowser *)getView();
        [browser selectRow:index inColumn:0];
    } else {
        // FIXME: What can we do here?
    }
}

bool QListBox::isSelected(int index) const
{
    ASSERT(!_insertingItems);
    NSBrowser *browser = (NSBrowser *)getView();
    return [[browser loadedCellAtRow:index column:0] state] != NSOffState; 
}

QSize QListBox::sizeForNumberOfLines(int lines) const
{
    NSBrowser *browser = (NSBrowser *)getView();
    int rows = [[browser delegate] browser:browser numberOfRowsInColumn:0];
    float rowHeight = 0;
    float width = 0;
    for (int row = 0; row < rows; row++) {
        NSSize size = [[browser loadedCellAtRow:row column:0] cellSize];
        width = MAX(width, size.width);
        rowHeight = MAX(rowHeight, size.height);
    }
    // FIXME: Add scroll bar width.
    return QSize((int)ceil(width + BORDER_TOTAL_WIDTH
        + [NSScroller scrollerWidthForControlSize:NSSmallControlSize]),
        (int)ceil(rowHeight * lines + BORDER_TOTAL_HEIGHT));
}

QListBoxItem::QListBoxItem(const QString &text)
    : _text(text), _next(0)
{
}
