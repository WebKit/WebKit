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

#import "KWQComboBox.h"

#import "KWQButton.h"
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQView.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "WebCoreBridge.h"
#import "khtmlview.h"
#import "render_replaced.h"

using khtml::RenderWidget;

@interface NSCell (KWQComboBoxKnowsAppKitSecrets)
- (NSMutableDictionary *)_textAttributes;
@end

enum {
    topMargin,
    bottomMargin,
    leftMargin,
    rightMargin,
    baselineFudgeFactor,
    widthNotIncludingText,
    minimumTextWidth
};

@interface KWQComboBoxAdapter : NSObject
{
    QComboBox *box;
}
- initWithQComboBox:(QComboBox *)b;
- (void)action:(id)sender;
@end

@interface KWQPopUpButtonCell : NSPopUpButtonCell <KWQWidgetHolder>
{
    QWidget *widget;
    NSWritingDirection baseWritingDirection;
}

- (id)initWithWidget:(QWidget *)widget;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;

@end

@interface KWQPopUpButton : NSPopUpButton <KWQWidgetHolder>
{
    BOOL inNextValidKeyView;
}
@end

QComboBox::QComboBox()
    : _adapter(0)
    , _widthGood(false)
    , _activated(this, SIGNAL(activated(int)))
{
    KWQ_BLOCK_EXCEPTIONS;

    _adapter = [[KWQComboBoxAdapter alloc] initWithQComboBox:this];
    KWQPopUpButton *button = [[KWQPopUpButton alloc] init];
    setView(button);
    [button release];
    
    KWQPopUpButtonCell *cell = [[KWQPopUpButtonCell alloc] initWithWidget:this];
    [button setCell:cell];
    [cell release];

    [button setTarget:_adapter];
    [button setAction:@selector(action:)];

    [[button cell] setControlSize:NSSmallControlSize];
    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]]];

    updateCurrentItem();

    KWQ_UNBLOCK_EXCEPTIONS;
}

QComboBox::~QComboBox()
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button setTarget:nil];
    [_adapter release];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QComboBox::insertItem(const QString &text, int i)
{
    KWQ_BLOCK_EXCEPTIONS;

    int index = i;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    int numItems = [button numberOfItems];
    if (index < 0) {
        index = numItems;
    }
    while (index >= numItems) {
        [button addItemWithTitle:@""];
        ++numItems;
    }
    // It's convenient that we added the item with an empty title,
    // because addItemWithTitle will not allow multiple items with the
    // same title. But this way, we can have such duplicate items.
    [[button itemAtIndex:index] setTitle:text.getNSString()];
    _widthGood = false;

    updateCurrentItem();

    KWQ_UNBLOCK_EXCEPTIONS;
}

QSize QComboBox::sizeHint() const 
{
    NSSize size = {0,0};

    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    
    float width;
    if (_widthGood) {
        width = _width;
    } else {
        width = 0;
        NSDictionary *attributes = [NSDictionary dictionaryWithObject:[button font] forKey:NSFontAttributeName];
        NSEnumerator *e = [[button itemTitles] objectEnumerator];
        NSString *text;
        while ((text = [e nextObject])) {
            NSSize size = [text sizeWithAttributes:attributes];
            width = MAX(width, size.width);
        }
        _width = ceil(width);
        if (_width < dimensions()[minimumTextWidth]) {
            _width = dimensions()[minimumTextWidth];
        }
        _widthGood = true;
    }
    
    size = [[button cell] cellSize];

    KWQ_UNBLOCK_EXCEPTIONS;

    return QSize((int)_width + dimensions()[widthNotIncludingText],
        (int)size.height - (dimensions()[topMargin] + dimensions()[bottomMargin]));
}

QRect QComboBox::frameGeometry() const
{
    QRect r = QWidget::frameGeometry();
    return QRect(r.x() + dimensions()[leftMargin], r.y() + dimensions()[topMargin],
        r.width() - (dimensions()[leftMargin] + dimensions()[rightMargin]),
        r.height() - (dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void QComboBox::setFrameGeometry(const QRect &r)
{
    QWidget::setFrameGeometry(QRect(-dimensions()[leftMargin] + r.x(), -dimensions()[topMargin] + r.y(),
        dimensions()[leftMargin] + r.width() + dimensions()[rightMargin],
        dimensions()[topMargin] + r.height() + dimensions()[bottomMargin]));
}

int QComboBox::baselinePosition(int height) const
{
    // Menu text is at the top.
    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    return (int)ceil(-dimensions()[topMargin] + dimensions()[baselineFudgeFactor] + [[button font] ascender]);
}

void QComboBox::clear()
{
    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button removeAllItems];
    _widthGood = false;
    updateCurrentItem();
}

void QComboBox::setCurrentItem(int index)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button selectItemAtIndex:index];

    KWQ_UNBLOCK_EXCEPTIONS;

    updateCurrentItem();
}

bool QComboBox::updateCurrentItem() const
{
    KWQPopUpButton *button = (KWQPopUpButton *)getView();

    KWQ_BLOCK_EXCEPTIONS;
    int i = [button indexOfSelectedItem];

    if (_currentItem == i) {
        return false;
    }
    _currentItem = i;
    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

void QComboBox::itemSelected()
{
    if (updateCurrentItem()) {
        _activated.call(_currentItem);
    }
}

void QComboBox::setFont(const QFont &f)
{
    QWidget::setFont(f);

    const NSControlSize size = KWQNSControlSizeForFont(f);
    NSControl * const button = static_cast<NSControl *>(getView());

    KWQ_BLOCK_EXCEPTIONS;

    if (size != [[button cell] controlSize]) {
        [[button cell] setControlSize:size];
        [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
        _widthGood = false;
    }

    KWQ_UNBLOCK_EXCEPTIONS;
}

const int *QComboBox::dimensions() const
{
    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow.
    static const int w[3][7] = {
        { 2, 3, 3, 3, 4, 34, 9 },
        { 1, 3, 3, 3, 3, 31, 5 },
        { 0, 0, 1, 1, 2, 32, 0 }
    };
    NSControl * const button = static_cast<NSControl *>(getView());

    KWQ_BLOCK_EXCEPTIONS;
    return  w[[[button cell] controlSize]];
    KWQ_UNBLOCK_EXCEPTIONS;

    return w[NSSmallControlSize];
}

QWidget::FocusPolicy QComboBox::focusPolicy() const
{
    KWQ_BLOCK_EXCEPTIONS;
    
    // Add an additional check here.
    // For now, selects are only focused when full
    // keyboard access is turned on.
    unsigned keyboardUIMode = [KWQKHTMLPart::bridgeForWidget(this) keyboardUIMode];
    if ((keyboardUIMode & WebCoreKeyboardAccessFull) == 0)
        return NoFocus;
    
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return QWidget::focusPolicy();
}

void QComboBox::setWritingDirection(QPainter::TextDirection direction)
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = getView();
    KWQPopUpButtonCell *cell = [button cell];
    NSWritingDirection d = direction == QPainter::RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    if ([cell baseWritingDirection] != d) {
        [cell setBaseWritingDirection:d];
        [button setNeedsDisplay:YES];
    }

    KWQ_UNBLOCK_EXCEPTIONS;
}

@implementation KWQComboBoxAdapter

- initWithQComboBox:(QComboBox *)b
{
    box = b;
    return [super init];
}

- (void)action:(id)sender
{
    box->itemSelected();
}

@end

@implementation KWQPopUpButtonCell

- initWithWidget:(QWidget *)w
{
    [super init];
    widget = w;
    return self;
}

- (BOOL)trackMouse:(NSEvent *)event inRect:(NSRect)rect ofView:(NSView *)view untilMouseUp:(BOOL)flag
{
    WebCoreBridge *bridge = [KWQKHTMLPart::bridgeForWidget(widget) retain];
    BOOL result = [super trackMouse:event inRect:rect ofView:view untilMouseUp:flag];
    if (result) {
        // Give KHTML a chance to fix up its event state, since the popup eats all the
        // events during tracking.  [NSApp currentEvent] is still the original mouseDown
        // at this point!
        [bridge part]->sendFakeEventsAfterWidgetTracking(event);
    }
    [bridge release];
    return result;
}

- (QWidget *)widget
{
    return widget;
}

- (void)setBaseWritingDirection:(NSWritingDirection)direction
{
    baseWritingDirection = direction;
}

- (NSWritingDirection)baseWritingDirection
{
    return baseWritingDirection;
}

- (NSMutableDictionary *)_textAttributes
{
    NSMutableDictionary *attributes = [super _textAttributes];
    NSParagraphStyle *style = [attributes objectForKey:NSParagraphStyleAttributeName];
    ASSERT(style != nil);
    if ([style baseWritingDirection] != baseWritingDirection) {
        NSMutableParagraphStyle *mutableStyle = [style mutableCopy];
        [mutableStyle setBaseWritingDirection:baseWritingDirection];
        [attributes setObject:mutableStyle forKey:NSParagraphStyleAttributeName];
        [mutableStyle release];
    }
    return attributes;
}

@end

@implementation KWQPopUpButton

- (QWidget *)widget
{
    return [(KWQPopUpButtonCell *)[self cell] widget];
}

- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];
    if (become) {
        QWidget *widget = [self widget];
        if (!KWQKHTMLPart::currentEventIsMouseDownInWidget(widget)) {
            [self _KWQ_scrollFrameToVisible];
        }
        QFocusEvent event(QEvent::FocusIn);
        const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
    }
    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        QWidget *widget = [self widget];
        QFocusEvent event(QEvent::FocusOut);
        const_cast<QObject *>(widget->eventFilterObject())->eventFilter(widget, &event);
    }
    return resign;
}

-(NSView *)nextKeyView
{
    QWidget *widget = [self widget];
    return widget && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext)
        : [super nextKeyView];
}

-(NSView *)previousKeyView
{
    QWidget *widget = [self widget];
    return widget && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious)
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

@end
