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

#import "KWQComboBox.h"

#import "KWQKHTMLPart.h"
#import "WebCoreBridge.h"

// We empirically determined that combo boxes have these extra pixels on all
// sides. It would be better to get this info from AppKit somehow.
#define TOP_MARGIN 1
#define BOTTOM_MARGIN 3
#define LEFT_MARGIN 3
#define RIGHT_MARGIN 3

#define TEXT_VERTICAL_NUDGE 1

// This is the 2-pixel CELLOFFSET for bordered cells from NSCell.
#define VERTICAL_FUDGE_FACTOR 2

@interface KWQComboBoxAdapter : NSObject
{
    QComboBox *box;
}
- initWithQComboBox:(QComboBox *)b;
- (void)action:(id)sender;
@end

@interface KWQPopUpButtonCell : NSPopUpButtonCell
{
    QWidget *widget;
}
- initWithWidget:(QWidget *)widget;
@end

QComboBox::QComboBox()
    : m_activated(this, SIGNAL(activated(int)))
    , m_adapter([[KWQComboBoxAdapter alloc] initWithQComboBox:this])
{
    NSPopUpButton *button = [[NSPopUpButton alloc] init];
    
    KWQPopUpButtonCell *cell = [[KWQPopUpButtonCell alloc] initWithWidget:this];
    [button setCell:cell];
    [cell release];

    [button setTarget:m_adapter];
    [button setAction:@selector(action:)];

    [[button cell] setControlSize:NSSmallControlSize];
    [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];

    setView(button);

    [button release];
}

QComboBox::~QComboBox()
{
    NSPopUpButton *button = (NSPopUpButton *)getView();
    [button setTarget:nil];
    [m_adapter release];
}

void QComboBox::insertItem(const QString &text, int index)
{
    NSPopUpButton *button = (NSPopUpButton *)getView();
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
}

QSize QComboBox::sizeHint() const 
{
    NSPopUpButton *button = (NSPopUpButton *)getView();
    return QSize((int)[[button cell] cellSize].width - (LEFT_MARGIN + RIGHT_MARGIN),
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
    NSPopUpButton *button = (NSButton *)getView();
    return (int)ceil(-TOP_MARGIN + VERTICAL_FUDGE_FACTOR
        + TEXT_VERTICAL_NUDGE + [[button font] ascender]);
}

void QComboBox::clear()
{
    NSPopUpButton *button = (NSPopUpButton *)getView();
    [button removeAllItems];
}

void QComboBox::setCurrentItem(int index)
{
    NSPopUpButton *button = (NSPopUpButton *)getView();
    [button selectItemAtIndex:index];
}

int QComboBox::currentItem() const
{
    NSPopUpButton *button = (NSPopUpButton *)getView();
    return [button indexOfSelectedItem];
}

@implementation KWQComboBoxAdapter

- initWithQComboBox:(QComboBox *)b
{
    box = b;
    return [super init];
}

- (void)action:(id)sender
{
    box->activated();
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
    // We need to "defer loading" while we are tracking the menu.
    // That's because we don't want the new page to load while the user is holding the mouse down.
    // Normally, this is not a problem because we use a different run loop mode, but pop-up menus
    // use a Carbon implementation, and it uses the default run loop mode.
    // See bug 3021018 for more information.
    
    WebCoreBridge *bridge = KWQKHTMLPart::bridgeForWidget(widget);
    BOOL wasDeferringLoading = [bridge defersLoading];
    if (!wasDeferringLoading) {
        [bridge setDefersLoading:YES];
    }
    BOOL result = [super trackMouse:event inRect:rect ofView:view untilMouseUp:flag];
    if (!wasDeferringLoading) {
        [bridge setDefersLoading:NO];
    }
    return result;
}

- (void)drawInteriorWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    cellFrame.origin.y += TEXT_VERTICAL_NUDGE;
    cellFrame.size.height -= TEXT_VERTICAL_NUDGE;
    [super drawInteriorWithFrame:cellFrame inView:controlView];
}

@end
