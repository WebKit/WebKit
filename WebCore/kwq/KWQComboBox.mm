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

#import "KWQComboBox.h"

#import "KWQAssertions.h"
#import "KWQButton.h"
#import "KWQExceptions.h"
#import "KWQKHTMLPart.h"
#import "KWQNSViewExtras.h"
#import "KWQView.h"
#import "WebCoreBridge.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

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

@interface KWQPopUpButtonCell : NSPopUpButtonCell <KWQWidgetHolder>
{
    QComboBox *box;
    NSWritingDirection baseWritingDirection;
}
- (id)initWithQComboBox:(QComboBox *)b;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
@end

@interface KWQPopUpButton : NSPopUpButton <KWQWidgetHolder>
{
    BOOL inNextValidKeyView;
}
@end

QComboBox::QComboBox()
    : _widthGood(false)
    , _currentItem(0)
    , _menuPopulated(true)
    , _activated(this, SIGNAL(activated(int)))
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = [[KWQPopUpButton alloc] init];
    setView(button);
    [button release];
    
    KWQPopUpButtonCell *cell = [[KWQPopUpButtonCell alloc] initWithQComboBox:this];
    [button setCell:cell];
    [cell release];

    [button setTarget:button];
    [button setAction:@selector(action:)];

    [[button cell] setControlSize:NSSmallControlSize];
    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]]];

    KWQ_UNBLOCK_EXCEPTIONS;
}

QComboBox::~QComboBox()
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button setTarget:nil];

    KWQ_UNBLOCK_EXCEPTIONS;
}

void QComboBox::appendItem(const QString &text)
{
    KWQ_BLOCK_EXCEPTIONS;

    _items.append(text);
    if (_menuPopulated) {
        KWQPopUpButton *button = (KWQPopUpButton *)getView();
        if (![[button cell] isHighlighted]) {
            _menuPopulated = false;
        } else {
            // We must add the item with no title and then set the title because
            // addItemWithTitle does not allow duplicate titles.
            [button addItemWithTitle:@""];
            [[button lastItem] setTitle:text.getNSString()];
        }
    }
    _widthGood = false;

    KWQ_UNBLOCK_EXCEPTIONS;
}

QSize QComboBox::sizeHint() const 
{
    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    
    if (!_widthGood) {
        float width = 0;
        QValueListConstIterator<QString> i = const_cast<const QStringList &>(_items).begin();
        QValueListConstIterator<QString> e = const_cast<const QStringList &>(_items).end();
        if (i != e) {
            id <WebCoreTextRenderer> renderer = [[WebCoreTextRendererFactory sharedFactory]
                rendererWithFont:[button font] usingPrinterFont:![NSGraphicsContext currentContextDrawingToScreen]];
            WebCoreTextStyle style;
            WebCoreInitializeEmptyTextStyle(&style);
            style.applyRunRounding = NO;
            style.applyWordRounding = NO;
            do {
                const QString &s = *i;
                ++i;

                WebCoreTextRun run;
                int length = s.length();
                WebCoreInitializeTextRun(&run, reinterpret_cast<const UniChar *>(s.unicode()), length, 0, length);

                float textWidth = [renderer floatWidthForRun:&run style:&style widths:0];
                width = kMax(width, textWidth);
            } while (i != e);
        }
        _width = kMax(static_cast<int>(ceilf(width)), dimensions()[minimumTextWidth]);
        _widthGood = true;
    }
    
    return QSize(_width + dimensions()[widthNotIncludingText],
        static_cast<int>([[button cell] cellSize].height) - (dimensions()[topMargin] + dimensions()[bottomMargin]));

    KWQ_UNBLOCK_EXCEPTIONS;

    return QSize(0, 0);
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
    return static_cast<int>(ceilf(-dimensions()[topMargin] + dimensions()[baselineFudgeFactor] + [[button font] ascender]));
}

void QComboBox::clear()
{
    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button removeAllItems];
    _widthGood = false;
    _currentItem = 0;
    _items.clear();
    _menuPopulated = true;
}

void QComboBox::setCurrentItem(int index)
{
    ASSERT(index < (int)_items.count());

    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    if (_menuPopulated) {
        [button selectItemAtIndex:index];
    } else {
        [button removeAllItems];
        [button addItemWithTitle:@""];
        [[button itemAtIndex:0] setTitle:_items[index].getNSString()];
    }

    KWQ_UNBLOCK_EXCEPTIONS;

    _currentItem = index;
}

void QComboBox::itemSelected()
{
    ASSERT(_menuPopulated);

    KWQ_BLOCK_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    int i = [button indexOfSelectedItem];
    if (_currentItem == i) {
        return;
    }
    _currentItem = i;

    KWQ_UNBLOCK_EXCEPTIONS;

    _activated.call(_currentItem);
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
    
    // Menus are only focused when full keyboard access is turned on.
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

void QComboBox::populateMenu()
{
    if (!_menuPopulated) {
        KWQ_BLOCK_EXCEPTIONS;

        KWQPopUpButton *button = getView();
        [button removeAllItems];
        QValueListConstIterator<QString> i = const_cast<const QStringList &>(_items).begin();
        QValueListConstIterator<QString> e = const_cast<const QStringList &>(_items).end();
        for (; i != e; ++i) {
            // We must add the item with no title and then set the title because
            // addItemWithTitle does not allow duplicate titles.
            [button addItemWithTitle:@""];
            [[button lastItem] setTitle:(*i).getNSString()];
        }
        [button selectItemAtIndex:_currentItem];

        KWQ_UNBLOCK_EXCEPTIONS;

        _menuPopulated = true;
    }
}

@implementation KWQPopUpButtonCell

- (id)initWithQComboBox:(QComboBox *)b
{
    box = b;
    return [super init];
}

- (BOOL)trackMouse:(NSEvent *)event inRect:(NSRect)rect ofView:(NSView *)view untilMouseUp:(BOOL)flag
{
    WebCoreBridge *bridge = [KWQKHTMLPart::bridgeForWidget(box) retain];
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
    return box;
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

- (void)setHighlighted:(BOOL)highlighted
{
    if (highlighted) {
        box->populateMenu();
    }
    [super setHighlighted:highlighted];
}

@end

@implementation KWQPopUpButton

- (void)action:(id)sender
{
    static_cast<QComboBox *>([self widget])->itemSelected();
}

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

- (BOOL)canBecomeKeyView {
    // Simplified method from NSView; overridden to replace NSView's way of checking
    // for full keyboard access with ours.
    if (!KWQKHTMLPart::partForWidget([self widget])->tabsToAllControls()) {
        return NO;
    }
    
    return ([self window] != nil) && ![self isHiddenOrHasHiddenAncestor] && [self acceptsFirstResponder];
}

- (NSView *)nextKeyView
{
    QWidget *widget = [self widget];
    return widget && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    QWidget *widget = [self widget];
    return widget && inNextValidKeyView
        ? KWQKHTMLPart::nextKeyViewForWidget(widget, KWQSelectingPrevious)
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

@end
