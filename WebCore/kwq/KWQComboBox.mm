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

#import "KWQView.h"
#import "KWQKHTMLPart.h"
#import "WebCoreBridge.h"

#import "khtmlview.h"
#import "render_replaced.h"
using khtml::RenderWidget;

// We empirically determined that combo boxes have these extra pixels on all
// sides. It would be better to get this info from AppKit somehow.
#define TOP_MARGIN 1
#define BOTTOM_MARGIN 3
#define LEFT_MARGIN 3
#define RIGHT_MARGIN 3

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_2
#define TEXT_VERTICAL_NUDGE 1
#else
#define TEXT_VERTICAL_NUDGE 0
#endif

// This is the 2-pixel CELLOFFSET for bordered cells from NSCell.
#define VERTICAL_FUDGE_FACTOR 2

// When we discovered we needed to measure text widths ourselves, I empirically
// determined these widths. I don't know what exactly they correspond to in the
// NSPopUpButtonCell code.
#define WIDTH_NOT_INCLUDING_TEXT 31
#define MINIMUM_WIDTH 36

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
}
- initWithWidget:(QWidget *)widget;
@end

@interface KWQPopUpButton : NSPopUpButton <KWQWidgetHolder>
@end

QComboBox::QComboBox()
    : _adapter([[KWQComboBoxAdapter alloc] initWithQComboBox:this])
    , _widthGood(false)
    , _activated(this, SIGNAL(activated(int)))
{
    KWQPopUpButton *button = [[KWQPopUpButton alloc] init];
    
    KWQPopUpButtonCell *cell = [[KWQPopUpButtonCell alloc] initWithWidget:this];
    [button setCell:cell];
    [cell release];

    [button setTarget:_adapter];
    [button setAction:@selector(action:)];

    [[button cell] setControlSize:NSSmallControlSize];
    [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    setView(button);

    [button release];

    updateCurrentItem();
}

QComboBox::~QComboBox()
{
    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button setTarget:nil];
    [_adapter release];
}

void QComboBox::insertItem(const QString &text, int index)
{
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
}

QSize QComboBox::sizeHint() const 
{
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
        if (_width < MINIMUM_WIDTH - WIDTH_NOT_INCLUDING_TEXT) {
            _width = MINIMUM_WIDTH - WIDTH_NOT_INCLUDING_TEXT;
        }
        _widthGood = true;
    }
    
    return QSize((int)_width + WIDTH_NOT_INCLUDING_TEXT,
        (int)[[button cell] cellSize].height - (TOP_MARGIN + BOTTOM_MARGIN));
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
    QWidget::setFrameGeometry(QRect(-LEFT_MARGIN + r.x(), -TOP_MARGIN + r.y(),
        LEFT_MARGIN + r.width() + RIGHT_MARGIN,
        TOP_MARGIN + r.height() + BOTTOM_MARGIN));
}

int QComboBox::baselinePosition() const
{
    // Menu text is at the top.
    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    return (int)ceil(-TOP_MARGIN + VERTICAL_FUDGE_FACTOR
        + TEXT_VERTICAL_NUDGE + [[button font] ascender]);
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
    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button selectItemAtIndex:index];
    updateCurrentItem();
}

bool QComboBox::updateCurrentItem() const
{
    int i = [(KWQPopUpButton *)getView() indexOfSelectedItem];
    if (_currentItem == i) {
        return false;
    }
    _currentItem = i;
    return true;
}

void QComboBox::itemSelected()
{
    if (updateCurrentItem()) {
        _activated.call(_currentItem);
    }
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
    // We need to "defer loading" and defer timers while we are tracking the menu.
    // That's because we don't want the new page to load while the user is holding the mouse down.
    // Normally, this is not a problem because we use a different run loop mode, but pop-up menus
    // use a Carbon implementation, and it uses the default run loop mode.
    // See bugs 3021018 and 3242460 for some more information.
    
    WebCoreBridge *bridge = [KWQKHTMLPart::bridgeForWidget(widget) retain];
    BOOL wasDeferringLoading = [bridge defersLoading];
    if (!wasDeferringLoading) {
        [bridge setDefersLoading:YES];
    }
    BOOL wasDeferringTimers = QObject::defersTimers();
    if (!wasDeferringTimers) {
        QObject::setDefersTimers(true);
    }
    BOOL result = [super trackMouse:event inRect:rect ofView:view untilMouseUp:flag];
    if (!wasDeferringTimers) {
        QObject::setDefersTimers(false);
    }
    if (!wasDeferringLoading) {
        [bridge setDefersLoading:NO];
    }
    if (result) {
        // Give khtml a chance to fix up its event state, since the popup eats all the
        // events during tracking.  [NSApp currentEvent] is still the original mouseDown
        // at this point!
        [bridge part]->doFakeMouseUpAfterWidgetTracking(event);
    }
    [bridge release];
    return result;
}

#if TEXT_VERTICAL_NUDGE

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    cellFrame.origin.y += TEXT_VERTICAL_NUDGE;
    cellFrame.size.height -= TEXT_VERTICAL_NUDGE;
    [super drawInteriorWithFrame:cellFrame inView:controlView];
}

#endif

- (QWidget *)widget
{
    return widget;
}

@end

@implementation KWQPopUpButton

- (QWidget *)widget
{
    return [(KWQPopUpButtonCell *)[self cell] widget];
}

@end
