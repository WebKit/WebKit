/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#define MIN_LINES 4 /* ensures we have a scroll bar */

@interface KWQListBoxScrollView : NSScrollView
{
}
@end

@interface KWQListBoxTableViewDelegate : NSObject
{
    QListBox *_box;
    NSArray *_items;
}
- initWithListBox:(QListBox *)b items:(NSArray *)items;
@end

QListBox::QListBox(QWidget *parent)
    : QScrollView(parent)
    , _items([[NSMutableArray alloc] init])
    , _insertingItems(false)
    , _changingSelection(false)
    , _widthGood(false)
    , _clicked(this, SIGNAL(clicked(QListBoxItem *)))
    , _selectionChanged(this, SIGNAL(selectionChanged()))
{
    NSScrollView *scrollView = [[KWQListBoxScrollView alloc] init];
    
    [scrollView setBorderType:NSBezelBorder];
    [scrollView setHasVerticalScroller:YES];
    [[scrollView verticalScroller] setControlSize:NSSmallControlSize];
    
    NSTableView *tableView = [[NSTableView alloc] init];
    KWQListBoxTableViewDelegate *delegate = [[KWQListBoxTableViewDelegate alloc] initWithListBox:this items:_items];

    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:nil];

    [column setEditable:NO];
    [[column dataCell] setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    [tableView addTableColumn:column];

    [column release];
    
    [tableView setAllowsMultipleSelection:NO];
    [tableView setHeaderView:nil];
    [tableView setIntercellSpacing:NSMakeSize(0, 0)];
    [tableView setRowHeight:ceil([[column dataCell] cellSize].height)];
    
    [tableView setDataSource:delegate];
    [tableView setDelegate:delegate];

    [scrollView setDocumentView:tableView];
    [scrollView setVerticalLineScroll:[tableView rowHeight]];
    
    [tableView release];
    
    setView(scrollView);
    
    [scrollView release];
}

QListBox::~QListBox()
{
    NSTableView *tableView = [(NSScrollView *)getView() documentView];
    KWQListBoxTableViewDelegate *delegate = [tableView delegate];
    [tableView setDelegate:nil];
    [tableView setDataSource:nil];
    [delegate release];
    [_items release];
}

uint QListBox::count() const
{
    return [_items count];
}

void QListBox::clear()
{
    [_items removeAllObjects];
    if (!_insertingItems) {
        NSTableView *tableView = [(NSScrollView *)getView() documentView];
        [tableView reloadData];
    }
    _widthGood = NO;
}

void QListBox::setSelectionMode(SelectionMode mode)
{
    NSTableView *tableView = [(NSScrollView *)getView() documentView];
    [tableView setAllowsMultipleSelection:mode != Single];
}

void QListBox::insertItem(NSObject *o, unsigned index)
{
    unsigned c = count();
    if (index >= c) {
        [_items addObject:o];
    } else {
        [_items replaceObjectAtIndex:index withObject:o];
    }

    if (!_insertingItems) {
        NSTableView *tableView = [(NSScrollView *)getView() documentView];
        [tableView reloadData];
    }
    _widthGood = NO;
}

void QListBox::insertItem(const QString &text, unsigned index)
{
    insertItem(text.getNSString(), index);
}

void QListBox::insertGroupLabel(const QString &text, unsigned index)
{
    static NSDictionary *groupLabelAttributes;
    if (groupLabelAttributes == nil) {
        groupLabelAttributes = [[NSDictionary dictionaryWithObject:
            [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]] forKey:NSFontAttributeName] retain];
    }

    NSAttributedString *s = [[NSAttributedString alloc]
        initWithString:text.getNSString() attributes:groupLabelAttributes];
    insertItem(s, index);
    [s release];
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
    NSTableView *tableView = [(NSScrollView *)getView() documentView];
    [tableView reloadData];
}

void QListBox::setSelected(int index, bool selectIt)
{
    ASSERT(!_insertingItems);
    NSTableView *tableView = [(NSScrollView *)getView() documentView];
    _changingSelection = true;
    if (selectIt) {
        [tableView selectRow:index byExtendingSelection:[tableView allowsMultipleSelection]];
    } else {
        [tableView deselectRow:index];
    }
    _changingSelection = false;
}

bool QListBox::isSelected(int index) const
{
    ASSERT(!_insertingItems);
    NSTableView *tableView = [(NSScrollView *)getView() documentView];
    return [tableView isRowSelected:index]; 
}

QSize QListBox::sizeForNumberOfLines(int lines) const
{
    ASSERT(!_insertingItems);

    NSTableView *tableView = [(NSScrollView *)getView() documentView];
    
    float width;
    if (_widthGood) {
        width = _width;
    } else {
        width = 0;
        NSCell *cell = [[[tableView tableColumns] objectAtIndex:0] dataCell];
        NSEnumerator *e = [_items objectEnumerator];
        NSString *text;
        while ((text = [e nextObject])) {
            [cell setStringValue:text];
            NSSize size = [cell cellSize];
            width = MAX(width, size.width);
        }
        _width = width;
        _widthGood = YES;
    }
    
    NSSize contentSize;
    contentSize.width = ceil(width);
    contentSize.height = ceil(([tableView rowHeight] + [tableView intercellSpacing].height) * MAX(MIN_LINES, lines));
    NSSize size = [NSScrollView frameSizeForContentSize:contentSize
        hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSBezelBorder];

    return QSize(size);
}

@implementation KWQListBoxScrollView

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    NSTableColumn *column = [[[self documentView] tableColumns] objectAtIndex:0];
    [column setWidth:[self contentSize].width];
    [column setMinWidth:[self contentSize].width];
    [column setMaxWidth:[self contentSize].width];
}

@end

@implementation KWQListBoxTableViewDelegate

- initWithListBox:(QListBox *)b items:(NSArray *)i
{
    [super init];
    _box = b;
    _items = i;
    return self;
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return [_items count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(int)row
{
    return [_items objectAtIndex:row];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    _box->selectionChanged();
    if (!_box->changingSelection()) {
        _box->clicked();
    }
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(int)row
{
    return [[_items objectAtIndex:row] isKindOfClass:[NSString class]];
}

@end
