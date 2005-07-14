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

#import "KWQListBox.h"

#import "KWQAssertions.h"
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "KWQView.h"
#import "WebCoreBridge.h"
#import "WebCoreScrollView.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

@interface NSTableView (KWQListBoxKnowsAppKitSecrets)
- (NSCell *)_accessibilityTableCell:(int)row tableColumn:(NSTableColumn *)tableColumn;
@end

const int minLines = 4; /* ensures we have a scroll bar */
const float bottomMargin = 1;
const float leftMargin = 2;
const float rightMargin = 2;

@interface KWQListBoxScrollView : WebCoreScrollView <KWQWidgetHolder>
{
    QListBox *_box;
}
@end

@interface KWQTableView : NSTableView <KWQWidgetHolder>
{
    QListBox *_box;
    BOOL processingMouseEvent;
    BOOL clickedDuringMouseEvent;
    BOOL inNextValidKeyView;
    NSWritingDirection _direction;
}
- (id)initWithListBox:(QListBox *)b;
- (void)detach;
- (void)_KWQ_setKeyboardFocusRingNeedsDisplay;
- (QWidget *)widget;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
@end

static id <WebCoreTextRenderer> itemScreenRenderer;
static id <WebCoreTextRenderer> itemPrinterRenderer;
static id <WebCoreTextRenderer> groupLabelScreenRenderer;
static id <WebCoreTextRenderer> groupLabelPrinterRenderer;

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

static id <WebCoreTextRenderer> itemTextRenderer()
{
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        if (itemScreenRenderer == nil) {
            itemScreenRenderer = [[[WebCoreTextRendererFactory sharedFactory]
                rendererWithFont:itemFont() usingPrinterFont:NO] retain];
        }
        return itemScreenRenderer;
    } else {
        if (itemPrinterRenderer == nil) {
            itemPrinterRenderer = [[[WebCoreTextRendererFactory sharedFactory]
                rendererWithFont:itemFont() usingPrinterFont:YES] retain];
        }
        return itemPrinterRenderer;
    }
}

static id <WebCoreTextRenderer> groupLabelTextRenderer()
{
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        if (groupLabelScreenRenderer == nil) {
            groupLabelScreenRenderer = [[[WebCoreTextRendererFactory sharedFactory]
                rendererWithFont:groupLabelFont() usingPrinterFont:NO] retain];
        }
        return groupLabelScreenRenderer;
    } else {
        if (groupLabelPrinterRenderer == nil) {
            groupLabelPrinterRenderer = [[[WebCoreTextRendererFactory sharedFactory]
                rendererWithFont:groupLabelFont() usingPrinterFont:YES] retain];
        }
        return groupLabelPrinterRenderer;
    }
}

QListBox::QListBox(QWidget *parent)
    : QScrollView(parent)
    , _changingSelection(false)
    , _enabled(true)
    , _widthGood(false)
    , _clicked(this, SIGNAL(clicked(QListBoxItem *)))
    , _selectionChanged(this, SIGNAL(selectionChanged()))
{
    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = [[KWQListBoxScrollView alloc] initWithListBox:this];
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
    
    KWQTableView *tableView = [[KWQTableView alloc] initWithListBox:this];
    [scrollView setDocumentView:tableView];
    [tableView release];
    [scrollView setVerticalLineScroll:[tableView rowHeight]];
    
    KWQ_UNBLOCK_EXCEPTIONS;
}

QListBox::~QListBox()
{
    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    
    KWQ_BLOCK_EXCEPTIONS;
    KWQTableView *tableView = [scrollView documentView];
    [tableView detach];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QListBox::clear()
{
    _items.clear();
    _widthGood = false;
}

void QListBox::setSelectionMode(SelectionMode mode)
{
    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());

    KWQ_BLOCK_EXCEPTIONS;
    NSTableView *tableView = [scrollView documentView];
    [tableView setAllowsMultipleSelection:mode != Single];
    KWQ_UNBLOCK_EXCEPTIONS;
}

void QListBox::appendItem(const QString &text, bool isLabel)
{
    _items.append(KWQListBoxItem(text, isLabel));
    _widthGood = false;
}

void QListBox::doneAppendingItems()
{
    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    NSTableView *tableView = [scrollView documentView];
    [tableView reloadData];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QListBox::setSelected(int index, bool selectIt)
{
    ASSERT(index >= 0);

    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
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

    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    NSTableView *tableView = [scrollView documentView];
    return [tableView isRowSelected:index]; 

    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}

void QListBox::setEnabled(bool enabled)
{
    if (enabled != _enabled) {
        // You would think this would work, but not until AppKit bug 2177792 if fixed.
        //KWQ_BLOCK_EXCEPTIONS;
        //NSTableView *tableView = [(NSScrollView *)getView() documentView];
        //[tableView setEnabled:enabled];
        //KWQ_UNBLOCK_EXCEPTIONS;

        _enabled = enabled;

        NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
        NSTableView *tableView = [scrollView documentView];
        [tableView reloadData];
    }
}

bool QListBox::isEnabled()
{
    return _enabled;
}

QSize QListBox::sizeForNumberOfLines(int lines) const
{
    NSSize size = {0,0};

    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    KWQTableView *tableView = [scrollView documentView];
    
    if (!_widthGood) {
        float width = 0;
        QValueListConstIterator<KWQListBoxItem> i = const_cast<const QValueList<KWQListBoxItem> &>(_items).begin();
        QValueListConstIterator<KWQListBoxItem> e = const_cast<const QValueList<KWQListBoxItem> &>(_items).end();
        if (i != e) {
            WebCoreTextStyle style;
            WebCoreInitializeEmptyTextStyle(&style);
            style.rtl = [tableView baseWritingDirection] == NSWritingDirectionRightToLeft;
            style.applyRunRounding = NO;
            style.applyWordRounding = NO;
            do {
                const QString &s = (*i).string;
                id <WebCoreTextRenderer> renderer = (*i).isGroupLabel ? groupLabelTextRenderer() : itemTextRenderer();
                ++i;

                WebCoreTextRun run;
                int length = s.length();
                WebCoreInitializeTextRun(&run, reinterpret_cast<const UniChar *>(s.unicode()), length, 0, length);

                float textWidth = [renderer floatWidthForRun:&run style:&style widths:0];
                width = kMax(width, textWidth);
            } while (i != e);
        }
        _width = ceilf(width);
        _widthGood = true;
    }
    
    size = [NSScrollView frameSizeForContentSize:NSMakeSize(_width, [tableView rowHeight] * MAX(minLines, lines))
        hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSBezelBorder];
    size.width += [NSScroller scrollerWidthForControlSize:NSSmallControlSize] - [NSScroller scrollerWidth] + leftMargin + rightMargin;

    KWQ_UNBLOCK_EXCEPTIONS;

    return QSize(size);
}

QWidget::FocusPolicy QListBox::focusPolicy() const
{
    KWQ_BLOCK_EXCEPTIONS;
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(this);
    if (!bridge || ![bridge part] || ![bridge part]->tabsToAllControls()) {
        return NoFocus;
    }
    
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

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    KWQTableView *tableView = [scrollView documentView];
    NSWritingDirection direction = d == QPainter::RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    if ([tableView baseWritingDirection] != direction) {
        [tableView setBaseWritingDirection:direction];
        [tableView reloadData];
    }

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QListBox::clearCachedTextRenderers()
{
    [itemScreenRenderer release];
    itemScreenRenderer = nil;

    [itemPrinterRenderer release];
    itemPrinterRenderer = nil;

    [groupLabelScreenRenderer release];
    groupLabelScreenRenderer = nil;

    [groupLabelPrinterRenderer release];
    groupLabelPrinterRenderer = nil;
}

@implementation KWQListBoxScrollView

- (id)initWithListBox:(QListBox *)b
{
    if (!(self = [super init]))
        return nil;

    _box = b;
    return self;
}

- (QWidget *)widget
{
    return _box;
}

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

- (id)initWithListBox:(QListBox *)b
{
    [super init];

    _box = b;

    NSTableColumn *column = [[NSTableColumn alloc] initWithIdentifier:nil];

    [column setEditable:NO];

    [self addTableColumn:column];

    [column release];
    
    [self setAllowsMultipleSelection:NO];
    [self setHeaderView:nil];
    [self setIntercellSpacing:NSMakeSize(0, 0)];
    [self setRowHeight:ceilf([itemFont() ascender] - [itemFont() descender] + bottomMargin)];
    
    [self setDataSource:self];
    [self setDelegate:self];

    return self;
}

- (void)detach
{
    _box = 0;
    [self setDelegate:nil];
    [self setDataSource:nil];
}

- (void)mouseDown:(NSEvent *)event
{
    if (!_box) {
        [super mouseDown:event];
        return;
    }

    processingMouseEvent = YES;
    NSView *outerView = [_box->getOuterView() retain];
    QWidget::beforeMouseDown(outerView);
    [super mouseDown:event];
    QWidget::afterMouseDown(outerView);
    [outerView release];
    processingMouseEvent = NO;

    if (clickedDuringMouseEvent) {
	clickedDuringMouseEvent = false;
    } else if (_box) {
	_box->sendConsumedMouseUp();
    }
}

- (void)keyDown:(NSEvent *)event
{
    if (!_box)  {
        return;
    }
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
	[super keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event
{
    if (!_box)  {
        return;
    }
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
	[super keyUp:event];
    }
}

- (BOOL)becomeFirstResponder
{
    if (!_box) {
        return NO;
    }

    BOOL become = [super becomeFirstResponder];
    
    if (become) {
        if (_box && !KWQKHTMLPart::currentEventIsMouseDownInWidget(_box)) {
            [self _KWQ_scrollFrameToVisible];
        }        
	[self _KWQ_setKeyboardFocusRingNeedsDisplay];

        if (_box) {
            QFocusEvent event(QEvent::FocusIn);
            const_cast<QObject *>(_box->eventFilterObject())->eventFilter(_box, &event);
        }
    }

    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign && _box) {
        QFocusEvent event(QEvent::FocusOut);
        const_cast<QObject *>(_box->eventFilterObject())->eventFilter(_box, &event);
        [KWQKHTMLPart::bridgeForWidget(_box) formControlIsResigningFirstResponder:self];
    }
    return resign;
}

- (BOOL)canBecomeKeyView
{
    // Simplified method from NSView; overridden to replace NSView's way of checking
    // for full keyboard access with ours.
    if (!_box || !KWQKHTMLPart::partForWidget(_box)->tabsToAllControls()) {
        return NO;
    }
    return ([self window] != nil) && ![self isHiddenOrHasHiddenAncestor] && [self acceptsFirstResponder];
}

- (NSView *)nextKeyView
{
    return _box && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(_box, KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    return _box && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(_box, KWQSelectingPrevious)
        : [super previousKeyView];
}

- (NSView *)nextValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super nextValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (NSView *)previousValidKeyView
{
    inNextValidKeyView = YES;
    NSView *view = [super previousValidKeyView];
    inNextValidKeyView = NO;
    return view;
}

- (int)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _box ? _box->count() : 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(int)row
{
    return nil;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    if (_box) {
        _box->selectionChanged();
    }
    if (_box && !_box->changingSelection()) {
	if (processingMouseEvent) {
	    clickedDuringMouseEvent = true;
	    _box->sendConsumedMouseUp();
	}
        if (_box) {
            _box->clicked();
        }
    }
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(int)row
{
    return _box && !_box->itemAtIndex(row).isGroupLabel;
}

- (BOOL)selectionShouldChangeInTableView:(NSTableView *)aTableView
{
    return _box && _box->isEnabled();
}

- (void)drawRow:(int)row clipRect:(NSRect)clipRect
{
    if (!_box) {
        return;
    }

    const KWQListBoxItem &item = _box->itemAtIndex(row);

    NSColor *color;
    if (_box->isEnabled()) {
        if ([self isRowSelected:row] && [[self window] firstResponder] == self && ([[self window] isKeyWindow] || ![[self window] canBecomeKeyWindow])) {
            color = [NSColor alternateSelectedControlTextColor];
        } else {
            color = [NSColor controlTextColor];
        }
    } else {
        color = [NSColor disabledControlTextColor];
    }

    bool RTL = _direction == NSWritingDirectionRightToLeft;

    id <WebCoreTextRenderer> renderer = item.isGroupLabel ? groupLabelTextRenderer() : itemTextRenderer();

    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.rtl = RTL;
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;
    style.textColor = color;

    WebCoreTextRun run;
    int length = item.string.length();
    WebCoreInitializeTextRun(&run, reinterpret_cast<const UniChar *>(item.string.unicode()), length, 0, length);

    NSRect cellRect = [self frameOfCellAtColumn:0 row:row];
    NSPoint point;
    if (!RTL) {
        point.x = NSMinX(cellRect) + leftMargin;
    } else {
        point.x = NSMaxX(cellRect) - rightMargin - [renderer floatWidthForRun:&run style:&style widths:0];
    }
    point.y = NSMaxY(cellRect) + [itemFont() descender] - bottomMargin;

    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = point;
    
    [renderer drawRun:&run style:&style geometry:&geometry];
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

- (NSCell *)_accessibilityTableCell:(int)row tableColumn:(NSTableColumn *)tableColumn
{
    NSCell *cell = [super _accessibilityTableCell:row tableColumn:tableColumn];
    if (_box) {
        [cell setStringValue:_box->itemAtIndex(row).string.getNSString()];
    }
    return cell;
}

@end
