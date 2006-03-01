/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "KWQListBox.h"

#import "KWQExceptions.h"
#import "KWQView.h"
#import "MacFrame.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"
#import <kxmlcore/Assertions.h>

#import "render_form.h"

using namespace WebCore;

const int minLines = 4; /* ensures we have a scroll bar */
const float bottomMargin = 1;
const float leftMargin = 2;
const float rightMargin = 2;

@interface KWQListBoxScrollView : NSScrollView <KWQWidgetHolder>
@end

@interface KWQTableView : NSTableView <KWQWidgetHolder>
{
@public
    QListBox *_box;
    BOOL processingMouseEvent;
    BOOL clickedDuringMouseEvent;
    BOOL inNextValidKeyView;
    NSWritingDirection _direction;
    BOOL isSystemFont;
    UCTypeSelectRef typeSelectSelector;
}
- (id)initWithListBox:(QListBox *)b;
- (void)detach;
- (void)_KWQ_setKeyboardFocusRingNeedsDisplay;
- (Widget *)widget;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
- (void)fontChanged;
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

static id <WebCoreTextRenderer> itemTextRenderer()
{
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        if (itemScreenRenderer == nil) {
            WebCoreFont font;
            WebCoreInitializeFont(&font);
            font.font = itemFont();
            itemScreenRenderer = [[[WebCoreTextRendererFactory sharedFactory] rendererWithFont:font] retain];
        }
        return itemScreenRenderer;
    } else {
        if (itemPrinterRenderer == nil) {
            WebCoreFont font;
            WebCoreInitializeFont(&font);
            font.font = itemFont();
            font.forPrinter = YES;
            itemPrinterRenderer = [[[WebCoreTextRendererFactory sharedFactory] rendererWithFont:font] retain];
        }
        return itemPrinterRenderer;
    }
}

static id <WebCoreTextRenderer> groupLabelTextRenderer()
{
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        if (groupLabelScreenRenderer == nil) {
            WebCoreFont font;
            WebCoreInitializeFont(&font);
            font.font = [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]];
            groupLabelScreenRenderer = [[[WebCoreTextRendererFactory sharedFactory] rendererWithFont:font] retain];
        }
        return groupLabelScreenRenderer;
    } else {
        if (groupLabelPrinterRenderer == nil) {
            WebCoreFont font;
            WebCoreInitializeFont(&font);
            font.font = [NSFont boldSystemFontOfSize:[NSFont smallSystemFontSize]];
            font.forPrinter = YES;
            groupLabelPrinterRenderer = [[[WebCoreTextRendererFactory sharedFactory] rendererWithFont:font] retain];
        }
        return groupLabelPrinterRenderer;
    }
}

QListBox::QListBox()
    : _changingSelection(false)
    , _enabled(true)
    , _widthGood(false)
    , _clicked(this, SIGNAL(clicked(QListBoxItem *)))
    , _selectionChanged(this, SIGNAL(selectionChanged()))
{
    KWQ_BLOCK_EXCEPTIONS;

    NSScrollView *scrollView = [[KWQListBoxScrollView alloc] initWithFrame:NSZeroRect];
    setView(scrollView);
    [scrollView release];
    
    [scrollView setBorderType:NSBezelBorder];
    [scrollView setHasVerticalScroller:YES];
    [[scrollView verticalScroller] setControlSize:NSSmallControlSize];

    // Another element might overlap this one, so we have to do the slower-style scrolling.
    [[scrollView contentView] setCopiesOnScroll:NO];
    
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

void QListBox::appendItem(const QString &text, KWQListBoxItemType type, bool enabled)
{
    _items.append(KWQListBoxItem(text, type, enabled));
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
        // You would think this would work, but not until AppKit bug 2177792 is fixed.
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

IntSize QListBox::sizeForNumberOfLines(int lines) const
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
            
            id <WebCoreTextRenderer> renderer;
            id <WebCoreTextRenderer> groupLabelRenderer;
            
            if (tableView->isSystemFont) {        
                renderer = itemTextRenderer();
                groupLabelRenderer = groupLabelTextRenderer();
            } else {
                renderer = [[WebCoreTextRendererFactory sharedFactory] rendererWithFont:font().getWebCoreFont()];
                FontDescription boldDesc = font().fontDescription();
                boldDesc.setWeight(cBoldWeight);
                Font b = Font(boldDesc, font().letterSpacing(), font().wordSpacing());          
                groupLabelRenderer = [[WebCoreTextRendererFactory sharedFactory] rendererWithFont:b.getWebCoreFont()];
            }
            
            do {
                const QString &s = (*i).string;

                WebCoreTextRun run;
                int length = s.length();
                WebCoreInitializeTextRun(&run, reinterpret_cast<const UniChar *>(s.unicode()), length, 0, length);

                float textWidth = [(((*i).type == KWQListBoxGroupLabel) ? groupLabelRenderer : renderer) floatWidthForRun:&run style:&style];
                width = kMax(width, textWidth);
                
                ++i;
            
            } while (i != e);
        }
        _width = ceilf(width);
        _widthGood = true;
    }
    
    NSSize contentSize = { _width, [tableView rowHeight] * MAX(minLines, lines) };
    size = [NSScrollView frameSizeForContentSize:contentSize hasHorizontalScroller:NO hasVerticalScroller:YES borderType:NSBezelBorder];
    size.width += [NSScroller scrollerWidthForControlSize:NSSmallControlSize] - [NSScroller scrollerWidth] + leftMargin + rightMargin;

    return IntSize(size);
    KWQ_UNBLOCK_EXCEPTIONS;
    return IntSize(0, 0);
}

Widget::FocusPolicy QListBox::focusPolicy() const
{
    FocusPolicy policy = Widget::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
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

void QListBox::setFont(const Font& font)
{
    Widget::setFont(font);

    NSScrollView *scrollView = static_cast<NSScrollView *>(getView());
    KWQTableView *tableView = [scrollView documentView];
    [tableView fontChanged];
}

@implementation KWQListBoxScrollView

- (Widget *)widget
{
    KWQTableView *tableView = [self documentView];
    
    assert([tableView isKindOfClass:[KWQTableView class]]);
    
    return [tableView widget];
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
    Widget *widget = [documentView widget];
    [MacFrame::bridgeForWidget(widget) makeFirstResponder:documentView];
    return YES;
}

- (BOOL)autoforwardsScrollWheelEvents
{
    return YES;
}

@end

static Boolean KWQTableViewTypeSelectCallback(UInt32 index, void *listDataPtr, void *refcon, CFStringRef *outString, UCTypeSelectOptions *tsOptions)
{
    KWQTableView *self = static_cast<KWQTableView *>(refcon);   
    QListBox *box = static_cast<QListBox *>([self widget]);
    
    if (!box)
        return false;
    
    if (index > box->count())
        return false;
    
    if (outString)
        *outString = box->itemAtIndex(index).string.getCFString();
    
    if (tsOptions)
        *tsOptions = kUCTSOptionsNoneMask;
    
    return true;
}

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
    
    [self setDataSource:self];
    [self setDelegate:self];

    return self;
}

- (void)finalize
{
    if (typeSelectSelector)
        UCTypeSelectReleaseSelector(&typeSelectSelector);
    
    [super finalize];
}

- (void)dealloc
{
    if (typeSelectSelector)
        UCTypeSelectReleaseSelector(&typeSelectSelector);
    
    [super dealloc];
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
    Widget::beforeMouseDown(outerView);
    [super mouseDown:event];
    Widget::afterMouseDown(outerView);
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
    WebCoreFrameBridge *bridge = MacFrame::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
    [super keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event
{
    if (!_box)  {
        return;
    }
    
    WebCoreFrameBridge *bridge = MacFrame::bridgeForWidget(_box);
    if (![bridge interceptKeyEvent:event toView:self]) {
        [super keyUp:event];
        NSString *string = [event characters];
       
        if ([string length] == 0)
           return;
       
        // type select should work with any graphic character as defined in D13a of the unicode standard.
        const uint32_t graphicCharacterMask = U_GC_L_MASK | U_GC_M_MASK | U_GC_N_MASK | U_GC_P_MASK | U_GC_S_MASK | U_GC_ZS_MASK;
        unichar pressedCharacter = [string characterAtIndex:0];
        
        if (!(U_GET_GC_MASK(pressedCharacter) & graphicCharacterMask)) {
            if (typeSelectSelector)
                UCTypeSelectFlushSelectorData(typeSelectSelector);
            return;            
        }
        
        OSStatus err = noErr;
        if (!typeSelectSelector)
            err = UCTypeSelectCreateSelector(0, 0, kUCCollateStandardOptions, &typeSelectSelector);
        
        if (err || !typeSelectSelector)
            return;
        
        Boolean updateSelector = false;
        // the timestamp and what the AddKey function want for time are the same thing.
        err = UCTypeSelectAddKeyToSelector(typeSelectSelector, (CFStringRef)string, [event timestamp], &updateSelector);
        
        if (err || !updateSelector)
            return;
  
        UInt32 closestItem = 0;
        
        err = UCTypeSelectFindItem(typeSelectSelector, [self numberOfRowsInTableView:self], 0, self, KWQTableViewTypeSelectCallback, &closestItem); 

        if (err)
            return;
        
        [self selectRowIndexes:[NSIndexSet indexSetWithIndex:closestItem] byExtendingSelection:NO];
        [self scrollRowToVisible:closestItem];
        
    }
}

- (BOOL)becomeFirstResponder
{
    if (!_box) {
        return NO;
    }

    BOOL become = [super becomeFirstResponder];
    
    if (become) {
        if (_box && !MacFrame::currentEventIsMouseDownInWidget(_box)) {
            RenderWidget *widget = const_cast<RenderWidget *> (static_cast<const RenderWidget *>(_box->eventFilterObject()));
            RenderLayer *layer = widget->enclosingLayer();
            if (layer)
                layer->scrollRectToVisible(widget->absoluteBoundingBoxRect());
        }        
        [self _KWQ_setKeyboardFocusRingNeedsDisplay];

        if (_box && _box->eventFilterObject())
            _box->eventFilterObject()->eventFilterFocusIn();
    }

    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign && _box && _box->eventFilterObject()) {
        _box->eventFilterObject()->eventFilterFocusOut();
        if (_box)
            [MacFrame::bridgeForWidget(_box) formControlIsResigningFirstResponder:self];
    }
    return resign;
}

- (BOOL)canBecomeKeyView
{
    // Simplified method from NSView; overridden to replace NSView's way of checking
    // for full keyboard access with ours.
    return ([self window] != nil) && ![self isHiddenOrHasHiddenAncestor] && [self acceptsFirstResponder];
}

- (NSView *)nextKeyView
{
    return _box && inNextValidKeyView
        ? MacFrame::nextKeyViewForWidget(_box, KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    return _box && inNextValidKeyView
        ? MacFrame::nextKeyViewForWidget(_box, KWQSelectingPrevious)
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
    if (!_box)
        return NO;
    
    const KWQListBoxItem &item = _box->itemAtIndex(row);
    
    return item.type == KWQListBoxOption && item.enabled;
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
    if (_box->isEnabled() && item.enabled) {
        if ([self isRowSelected:row] && [[self window] firstResponder] == self && ([[self window] isKeyWindow] || ![[self window] canBecomeKeyWindow])) {
            color = [NSColor alternateSelectedControlTextColor];
        } else {
            color = [NSColor controlTextColor];
        }
    } else {
        color = [NSColor disabledControlTextColor];
    }

    bool rtl = _direction == NSWritingDirectionRightToLeft;

    id <WebCoreTextRenderer> renderer;
    if (isSystemFont) {
        renderer = (item.type == KWQListBoxGroupLabel) ? groupLabelTextRenderer() : itemTextRenderer();
    } else {
        if (item.type == KWQListBoxGroupLabel) {
            FontDescription boldDesc = _box->font().fontDescription();
            boldDesc.setWeight(cBoldWeight);
            Font b = Font(boldDesc, _box->font().letterSpacing(), _box->font().wordSpacing());          
            renderer = [[WebCoreTextRendererFactory sharedFactory] rendererWithFont:b.getWebCoreFont()];
        }
        else
            renderer = [[WebCoreTextRendererFactory sharedFactory] rendererWithFont:_box->font().getWebCoreFont()];
    }
   
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.rtl = rtl;
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;
    style.textColor = color;

    WebCoreTextRun run;
    int length = item.string.length();
    WebCoreInitializeTextRun(&run, reinterpret_cast<const UniChar *>(item.string.unicode()), length, 0, length);

    NSRect cellRect = [self frameOfCellAtColumn:0 row:row];
    NSPoint point;
    if (!rtl) {
        point.x = NSMinX(cellRect) + leftMargin;
    } else {
        point.x = NSMaxX(cellRect) - rightMargin - [renderer floatWidthForRun:&run style:&style];
    }
    point.y = NSMaxY(cellRect) - [renderer descent] - bottomMargin;

    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = point;
    
    [renderer drawRun:&run style:&style geometry:&geometry];
}

- (void)_KWQ_setKeyboardFocusRingNeedsDisplay
{
    [self setKeyboardFocusRingNeedsDisplayInRect:[self bounds]];
}

- (Widget *)widget
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

- (void)fontChanged
{
    NSFont *font = _box->font().getNSFont();
    isSystemFont = [[font fontName] isEqualToString:[itemFont() fontName]] && [font pointSize] == [itemFont() pointSize];
    [self setRowHeight:ceilf([font ascender] - [font descender] + bottomMargin)];
    [self setNeedsDisplay:YES];
}

@end
