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
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "KWQView.h"
#import "WebCoreBridge.h"
#import "WebCoreScrollView.h"

#define MIN_LINES 4 /* ensures we have a scroll bar */

@interface KWQListBoxScrollView : WebCoreScrollView
@end

@interface KWQTableView : NSTableView <KWQWidgetHolder>
{
    QListBox *_box;
    NSArray *_items;
    BOOL processingMouseEvent;
    BOOL clickedDuringMouseEvent;
    BOOL inNextValidKeyView;
    NSWritingDirection _direction;
}
- initWithListBox:(QListBox *)b items:(NSArray *)items;
- (void)_KWQ_setKeyboardFocusRingNeedsDisplay;
- (QWidget *)widget;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
@end

static NSFont *itemFont()
{
    static NSFont *font = [[NSFont systemFontOfSize:[NSFont smallSystemFontSize]] retain];
    return font;
}

static NSFont *groupLabelFont()
{
    static NSFont *font = [[NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]] retain];
    return font;
}

static NSParagraphStyle *paragraphStyle(NSWritingDirection direction)
{
    static NSParagraphStyle *leftStyle;
    static NSParagraphStyle *rightStyle;
    NSParagraphStyle **style = direction == NSWritingDirectionRightToLeft ? &rightStyle : &leftStyle;
    if (*style == nil) {
        NSMutableParagraphStyle *mutableStyle = [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
        [mutableStyle setBaseWritingDirection:direction];
        *style = [mutableStyle copy];
        [mutableStyle release];
    }
    return *style;
}

static NSDictionary *stringAttributes(NSWritingDirection direction, bool isGroupLabel)
{
    static NSDictionary *attributeGlobals[4];
    NSDictionary **attributes = &attributeGlobals[(direction == NSWritingDirectionRightToLeft ? 0 : 1) + (isGroupLabel ? 0 : 2)];
    if (*attributes == nil) {
        *attributes = [[NSDictionary dictionaryWithObjectsAndKeys:
            isGroupLabel ? groupLabelFont() : itemFont(), NSFontAttributeName,
            paragraphStyle(direction), NSParagraphStyleAttributeName,
            nil] retain];
    }
    return *attributes;
}

QListBox::QListBox(QWidget *parent)
    : QScrollView(parent)
    , _insertingItems(false)
    , _changingSelection(false)
    , _enabled(true)
    , _widthGood(false)
    , _clicked(this, SIGNAL(clicked(QListBoxItem *)))
    , _selectionChanged(this, SIGNAL(selectionChanged()))
{
    KWQ_BLOCK_EXCEPTIONS;

    _items = [[NSMutableArray alloc] init];
    NSScrollView *scrollView = [[KWQListBoxScrollView alloc] init];
    setView(scrollView);
    [scrollView release];
    
    [scrollView setBorderType:NSBezelBorder];
    [scrollView setHasVerticalScroller:YES];
    [[scrollView verticalScroller] setControlSize:NSSmallControlSize];

    // In WebHTMLView, we set a clip. This is not typical to do in an
    // NSView, and while correct for any one invocation of drawRect:,
    // it causes some bad problems if that clip is cached between calls.
    // The cached graphics state, which clip views keep around, does
    // cache the clip in this undesirable way. Consequently, we want to 
    // release the GState for all clip views for all views contained in 
    // a WebHTMLView. Here we do it for list boxes used in forms.
    // See these bugs for more information:
    // <rdar://problem/3226083>: REGRESSION (Panther): white box overlaying select lists at nvidia.com drivers page
    [[scrollView contentView] releaseGState];
    
    KWQTableView *tableView = [[KWQTableView alloc] initWithListBox:this items:_items];
    [scrollView setDocumentView:tableView];
    [tableView release];
    [scrollView setVerticalLineScroll:[tableView rowHeight]];
    
    KWQ_UNBLOCK_EXCEPTIONS;
}

QListBox::~QListBox()
{
    NSScrollView *scrollView = getView();
    
    KWQ_BLOCK_EXCEPTIONS;
    NSTableView *tableView = [scrollView documentView];
    [tableView setDelegate:nil];
    [tableView setDataSource:nil];
    [_items release];
    KWQ_UNBLOCK_EXCEPTIONS;
}

uint QListBox::count() const
{
    KWQ_BLOCK_EXCEPTIONS;
    return [_items count];
    KWQ_UNBLOCK_EXCEPTIONS;

    return 0;
}

void QListBox::clear()
{
    KWQ_BLOCK_EXCEPTIONS;
    [_items removeAllObjects];
    if (!_insertingItems) {
        NSScrollView *scrollView = getView();
        NSTableView *tableView = [scrollView documentView];
        [tableView reloadData];
    }
    KWQ_UNBLOCK_EXCEPTIONS;
    _widthGood = NO;
}

void QListBox::setSelectionMode(SelectionMode mode)
{
    NSScrollView *scrollView = getView();

    KWQ_BLOCK_EXCEPTIONS;
    NSTableView *tableView = [scrollView documentView];
    [tableView setAllowsMultipleSelection:mode != Single];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QListBox::insertItem(const QString &text, int index, bool isLabel)
{
    ASSERT(index >= 0);

    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = getView();
    KWQTableView *tableView = [scrollView documentView];

    NSAttributedString *s = [[NSAttributedString alloc] initWithString:text.getNSString()
        attributes:stringAttributes([tableView baseWritingDirection], isLabel)];

    int c = count();
    if (index >= c) {
        [_items addObject:s];
    } else {
        [_items replaceObjectAtIndex:index withObject:s];
    }
 
    [s release];

    if (!_insertingItems) {
        [tableView reloadData];
    }

    KWQ_UNBLOCK_EXCEPTIONS;

    _widthGood = NO;
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

    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = getView();
    NSTableView *tableView = [scrollView documentView];
    [tableView reloadData];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QListBox::setSelected(int index, bool selectIt)
{
    ASSERT(index >= 0);
    ASSERT(!_insertingItems);

    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = getView();
    NSTableView *tableView = [scrollView documentView];
    _changingSelection = true;
    if (selectIt) {
        [tableView selectRow:index byExtendingSelection:[tableView allowsMultipleSelection]];
        [tableView scrollRowToVisible:index];
    } else {
        [tableView deselectRow:index];
    }

    KWQ_UNBLOCK_EXCEPTIONS;

    _changingSelection = false;
}

bool QListBox::isSelected(int index) const
{
    ASSERT(index >= 0);
    ASSERT(!_insertingItems);

    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = getView();
    NSTableView *tableView = [scrollView documentView];
    return [tableView isRowSelected:index]; 

    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QListBox::setEnabled(bool enabled)
{
    _enabled = enabled;
    // You would think this would work, but not until AK fixes 2177792
    //KWQ_BLOCK_EXCEPTIONS;
    //NSTableView *tableView = [(NSScrollView *)getView() documentView];
    //[tableView setEnabled:enabled];
    //KWQ_UNBLOCK_EXCEPTIONS;
}

bool QListBox::isEnabled()
{
    return _enabled;
}

QSize QListBox::sizeForNumberOfLines(int lines) const
{
    ASSERT(!_insertingItems);

    NSScrollView *scrollView = getView();

    NSSize size = {0,0};

    KWQ_BLOCK_EXCEPTIONS;
    NSTableView *tableView = [scrollView documentView];
    
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
    size = [NSScrollView frameSizeForContentSize:contentSize
        hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSBezelBorder];
    KWQ_UNBLOCK_EXCEPTIONS;

    return QSize(size);
}

QWidget::FocusPolicy QListBox::focusPolicy() const
{
    KWQ_BLOCK_EXCEPTIONS;
    
    // Add an additional check here.
    // For now, selects are only focused when full
    // keyboard access is turned on.
    unsigned keyboardUIMode = [KWQKHTMLPart::bridgeForWidget(this) keyboardUIMode];
    if ((keyboardUIMode & WebCoreKeyboardAccessFull) == 0)
        return NoFocus;
    
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return QScrollView::focusPolicy();
}

bool QListBox::checksDescendantsForFocus() const
{
    return true;
}

void QListBox::setWritingDirection(QPainter::TextDirection d)
{
    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = getView();
    KWQTableView *tableView = [scrollView documentView];
    NSWritingDirection direction = d == QPainter::RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    if ([tableView baseWritingDirection] != direction) {
        int n = count();
        for (int i = 0; i < n; i++) {
            NSAttributedString *o = [_items objectAtIndex:i];
            NSAttributedString *s = [[NSAttributedString alloc] initWithString:[o string]
                attributes:stringAttributes([tableView baseWritingDirection], itemIsGroupLabel(i))];
            [_items replaceObjectAtIndex:i withObject:s];
            [s release];
        }
        [tableView setBaseWritingDirection:direction];
        [tableView reloadData];
    }

    KWQ_UNBLOCK_EXCEPTIONS;
}

bool QListBox::itemIsGroupLabel(int index) const
{
    ASSERT(index >= 0);

    KWQ_BLOCK_EXCEPTIONS;

    NSAttributedString *s = [_items objectAtIndex:index];
    NSFont *f = [s attribute:NSFontAttributeName atIndex:0 effectiveRange:NULL];
    return f == groupLabelFont();

    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
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

- (BOOL)becomeFirstResponder
{
    KWQTableView *documentView = [self documentView];
    QWidget *widget = [documentView widget];
    [KWQKHTMLPart::bridgeForWidget(widget) makeFirstResponder:documentView];
    return YES;
}

@end

@implementation KWQTableView

- initWithListBox:(QListBox *)b items:(NSArray *)i
{
    [super init];
    _box = b;
    _items = i;

    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:nil];

    [column setEditable:NO];
    [[column dataCell] setFont:itemFont()];

    [self addTableColumn:column];

    [column release];
    
    [self setAllowsMultipleSelection:NO];
    [self setHeaderView:nil];
    [self setIntercellSpacing:NSMakeSize(0, 0)];
    [self setRowHeight:ceil([[column dataCell] cellSize].height)];
    
    [self setDataSource:self];
    [self setDelegate:self];

    return self;
}

-(void)mouseDown:(NSEvent *)event
{
    processingMouseEvent = TRUE;
    [super mouseDown:event];
    processingMouseEvent = FALSE;

    if (clickedDuringMouseEvent) {
	clickedDuringMouseEvent = false;
    } else {
	_box->sendConsumedMouseUp();
    }
}

- (void)keyDown:(NSEvent *)event
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
	[super keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event
{
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
	[super keyUp:event];
    }
}

- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];
    
    if (become) {
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(_box)) {
            [self _KWQ_scrollFrameToVisible];
        }        
	[self _KWQ_setKeyboardFocusRingNeedsDisplay];
	QFocusEvent event(QEvent::FocusIn);
	const_cast<QObject *>(_box->eventFilterObject())->eventFilter(_box, &event);
    }

    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        QFocusEvent event(QEvent::FocusOut);
        const_cast<QObject *>(_box->eventFilterObject())->eventFilter(_box, &event);
    }
    return resign;
}

-(NSView *)nextKeyView
{
    return _box && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(_box, KWQSelectingNext)
        : [super nextKeyView];
}

-(NSView *)previousKeyView
{
    return _box && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(_box, KWQSelectingPrevious)
        : [super previousKeyView];
}

-(NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

-(NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
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
	if (processingMouseEvent) {
	    clickedDuringMouseEvent = true;
	    _box->sendConsumedMouseUp();
	}
        _box->clicked();
    }
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(int)row
{
    return !_box->itemIsGroupLabel(row);
}

- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTableView
{
    return _box->isEnabled();
}

- (void)tableView:(NSTableView *)tableView willDisplayCell:(id)cell forTableColumn:(NSTableColumn *)tableColumn row:(int)row
{
    ASSERT([cell isKindOfClass:[NSCell class]]);
    [(NSCell *)cell setEnabled:_box->isEnabled()];
}

- (void)_KWQ_setKeyboardFocusRingNeedsDisplay
{
    [self setKeyboardFocusRingNeedsDisplayInRect:[self bounds]];
}

- (QWidget *)widget
{
    return _box;
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    _direction = direction;
}

- (NSWritingDirection)baseWritingDirection
{
    return _direction;
}

@end
