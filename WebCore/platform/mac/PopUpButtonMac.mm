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
#import "PopUpButton.h"

#import "BlockExceptions.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "TextField.h"
#import "WebCoreFrameBridge.h"
#import "FontData.h"
#import "RenderView.h"
#import "RenderWidget.h"
#import "WebCoreWidgetHolder.h"
#import "WidgetClient.h"
#import "Font.h"

using namespace WebCore;

@interface NSCell (WebCorePopUpButtonKnowsAppKitSecrets)
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

@interface WebCorePopUpButtonCell : NSPopUpButtonCell <WebCoreWidgetHolder>
{
    PopUpButton *box;
    NSWritingDirection baseWritingDirection;
}
- (id)initWithQComboBox:(PopUpButton *)b;
- (void)detachQComboBox;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
@end

@interface WebCorePopUpButton : NSPopUpButton <WebCoreWidgetHolder>
{
    BOOL inNextValidKeyView;
    BOOL populatingMenu;
}
- (void)setPopulatingMenu:(BOOL)populating;

@end

PopUpButton::PopUpButton()
    : _widthGood(false)
    , _currentItem(0)
    , _menuPopulated(true)
    , _labelFont(nil)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCorePopUpButton *button = [[WebCorePopUpButton alloc] init];
    setView(button);
    [button release];
    
    WebCorePopUpButtonCell *cell = [[WebCorePopUpButtonCell alloc] initWithQComboBox:this];
    // Work around problem where the pop-up menu gets a "..." in it
    // by turning off the default "ellipsizing" behavior.
    [cell setLineBreakMode:NSLineBreakByClipping];
    [button setCell:cell];
    [cell release];

    [button setTarget:button];
    [button setAction:@selector(action:)];

    [[button cell] setControlSize:NSSmallControlSize];
    [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:NSSmallControlSize]]];
    [button setAutoenablesItems:NO];
    
    END_BLOCK_OBJC_EXCEPTIONS;
}

PopUpButton::~PopUpButton()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCorePopUpButton *button = (WebCorePopUpButton *)getView();
    [button setTarget:nil];
    [[button cell] detachQComboBox];
    HardRelease(_labelFont);

    END_BLOCK_OBJC_EXCEPTIONS;
}

void PopUpButton::setTitle(NSMenuItem *menuItem, const ListBoxItem &title)
{
    if (title.type == ListBoxGroupLabel) {
        NSDictionary *attributes = [[NSDictionary alloc] initWithObjectsAndKeys:labelFont(), NSFontAttributeName, nil];
        NSAttributedString *string = [[NSAttributedString alloc] initWithString:title.string.getNSString() attributes:attributes];
        [menuItem setAttributedTitle:string];
        [string release];
        [attributes release];
        [menuItem setEnabled:NO];
    } else {
        [menuItem setTitle:title.string.getNSString()];
        [menuItem setEnabled:title.enabled];
    }    
}

void PopUpButton::appendItem(const DeprecatedString &text, ListBoxItemType type, bool enabled)
{
    const ListBoxItem listItem(text, type, enabled);
    _items.append(listItem);
    if (_menuPopulated) {
        WebCorePopUpButton *button = (WebCorePopUpButton *)getView();
        if (![[button cell] isHighlighted]) {
            _menuPopulated = false;
        } else {
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            if (type == ListBoxSeparator) {
                NSMenuItem *separator = [NSMenuItem separatorItem];
                [[button menu] addItem:separator];
            } else {
                // We must add the item with no title and then set the title because
                // addItemWithTitle does not allow duplicate titles.
                [button addItemWithTitle:@""];
                NSMenuItem *menuItem = [button lastItem];
                setTitle(menuItem, listItem);
            }
            END_BLOCK_OBJC_EXCEPTIONS;
        }
    }
    _widthGood = false;
}

IntSize PopUpButton::sizeHint() const 
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCorePopUpButton *button = (WebCorePopUpButton *)getView();
    
    if (!_widthGood) {
        float width = 0;
        DeprecatedValueListConstIterator<ListBoxItem> i = const_cast<const DeprecatedValueList<ListBoxItem> &>(_items).begin();
        DeprecatedValueListConstIterator<ListBoxItem> e = const_cast<const DeprecatedValueList<ListBoxItem> &>(_items).end();
        if (i != e) {
            RenderWidget *client = static_cast<RenderWidget *>(Widget::client());
            bool isPrinting = client->view()->printingMode();
            FontPlatformData itemFont([button font], isPrinting);
            FontPlatformData labelFont(this->labelFont(), isPrinting);
            Font itemRenderer(itemFont);
            Font labelRenderer(labelFont);
            do {
                const DeprecatedString &s = (*i).string;
                bool isGroupLabel = ((*i).type == ListBoxGroupLabel);
                ++i;

                TextRun run(reinterpret_cast<const UniChar *>(s.unicode()), s.length());
                TextStyle style;
                style.disableRoundingHacks();
                Font* renderer = isGroupLabel ? &labelRenderer : &itemRenderer;
                float textWidth = renderer->floatWidth(run, style);
                width = max(width, textWidth);
            } while (i != e);
        }
        _width = max(static_cast<int>(ceilf(width)), dimensions()[minimumTextWidth]);
        _widthGood = true;
    }
    
    return IntSize(_width + dimensions()[widthNotIncludingText],
        static_cast<int>([[button cell] cellSize].height) - (dimensions()[topMargin] + dimensions()[bottomMargin]));

    END_BLOCK_OBJC_EXCEPTIONS;

    return IntSize(0, 0);
}

IntRect PopUpButton::frameGeometry() const
{
    IntRect r = Widget::frameGeometry();
    return IntRect(r.x() + dimensions()[leftMargin], r.y() + dimensions()[topMargin],
        r.width() - (dimensions()[leftMargin] + dimensions()[rightMargin]),
        r.height() - (dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void PopUpButton::setFrameGeometry(const IntRect& r)
{
    Widget::setFrameGeometry(IntRect(-dimensions()[leftMargin] + r.x(), -dimensions()[topMargin] + r.y(),
        dimensions()[leftMargin] + r.width() + dimensions()[rightMargin],
        dimensions()[topMargin] + r.height() + dimensions()[bottomMargin]));
}

int PopUpButton::baselinePosition(int height) const
{
    // Menu text is at the top.
    WebCorePopUpButton *button = (WebCorePopUpButton *)getView();
    return static_cast<int>(ceilf(-dimensions()[topMargin] + dimensions()[baselineFudgeFactor] + [[button font] ascender]));
}

void PopUpButton::clear()
{
    WebCorePopUpButton *button = (WebCorePopUpButton *)getView();
    [button removeAllItems];
    _widthGood = false;
    _currentItem = 0;
    _items.clear();
    _menuPopulated = true;
}

void PopUpButton::setCurrentItem(int index)
{
    ASSERT(index < (int)_items.count());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCorePopUpButton *button = (WebCorePopUpButton *)getView();
    if (_menuPopulated) {
        [button selectItemAtIndex:index];
    } else {
        [button removeAllItems];
        [button addItemWithTitle:@""];
        NSMenuItem *menuItem = [button itemAtIndex:0];
        setTitle(menuItem, _items[index]);
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    _currentItem = index;
}

void PopUpButton::itemSelected()
{
    ASSERT(_menuPopulated);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCorePopUpButton *button = (WebCorePopUpButton *)getView();
    int i = [button indexOfSelectedItem];
    if (_currentItem == i) {
        return;
    }
    _currentItem = i;

    END_BLOCK_OBJC_EXCEPTIONS;

    if (client())
        client()->valueChanged(this);
}

void PopUpButton::setFont(const Font& f)
{
    Widget::setFont(f);

    const NSControlSize size = ControlSizeForFont(f);
    NSControl * const button = static_cast<NSControl *>(getView());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (size != [[button cell] controlSize]) {
        [[button cell] setControlSize:size];
        [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
        HardRelease(_labelFont);
        _labelFont = nil;
        _widthGood = false;
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

NSFont *PopUpButton::labelFont() const
{
    if (_labelFont == nil) {
        NSControl * const button = static_cast<NSControl *>(getView());
        _labelFont = HardRetain([NSFont boldSystemFontOfSize:[[button font] pointSize]]);
    }
    return _labelFont;
}

const int *PopUpButton::dimensions() const
{
    // We empirically determined these dimensions.
    // It would be better to get this info from AppKit somehow.
    static const int w[3][7] = {
        { 2, 3, 3, 3, 4, 34, 9 },
        { 1, 3, 3, 3, 3, 31, 5 },
        { 0, 0, 1, 1, 2, 32, 0 }
    };
    NSControl * const button = static_cast<NSControl *>(getView());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return  w[[[button cell] controlSize]];
    END_BLOCK_OBJC_EXCEPTIONS;

    return w[NSSmallControlSize];
}

Widget::FocusPolicy PopUpButton::focusPolicy() const
{
    FocusPolicy policy = Widget::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

void PopUpButton::setWritingDirection(TextDirection direction)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCorePopUpButton *button = static_cast<WebCorePopUpButton *>(getView());
    WebCorePopUpButtonCell *cell = [button cell];
    NSWritingDirection d = direction == RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    if ([cell baseWritingDirection] != d) {
        [cell setBaseWritingDirection:d];
        [button setNeedsDisplay:YES];
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void PopUpButton::populateMenu()
{
    if (!_menuPopulated) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;

        WebCorePopUpButton *button = static_cast<WebCorePopUpButton *>(getView());
        [button setPopulatingMenu:YES];
        [button removeAllItems];
        DeprecatedValueListConstIterator<ListBoxItem> i = const_cast<const DeprecatedValueList<ListBoxItem> &>(_items).begin();
        DeprecatedValueListConstIterator<ListBoxItem> e = const_cast<const DeprecatedValueList<ListBoxItem> &>(_items).end();
        for (; i != e; ++i) {
            if ((*i).type == ListBoxSeparator) {
                NSMenuItem *separator = [NSMenuItem separatorItem];
                [[button menu] addItem:separator];
            } else {
                // We must add the item with no title and then set the title because
                // addItemWithTitle does not allow duplicate titles.
                [button addItemWithTitle:@""];
                NSMenuItem *menuItem = [button lastItem];
                setTitle(menuItem, *i);
            }
        }
        [button selectItemAtIndex:_currentItem];
        [button setPopulatingMenu:NO];

        END_BLOCK_OBJC_EXCEPTIONS;

        _menuPopulated = true;
    }
}

void PopUpButton::populate()
{
    populateMenu();
}

@implementation WebCorePopUpButtonCell

- (id)initWithQComboBox:(PopUpButton *)b
{
    box = b;
    return [super init];
}

- (void)detachQComboBox
{
    box = 0;
}

- (BOOL)trackMouse:(NSEvent *)event inRect:(NSRect)rect ofView:(NSView *)view untilMouseUp:(BOOL)flag
{
    WebCoreFrameBridge *bridge = box ? [FrameMac::bridgeForWidget(box) retain] : nil;

    // we need to retain the event because it is the [NSApp currentEvent], which can change
    // and therefore be released during [super trackMouse:...]
    [event retain];
    BOOL result = [super trackMouse:event inRect:rect ofView:view untilMouseUp:flag];
    if (result && bridge) {
        // Give KHTML a chance to fix up its event state, since the popup eats all the
        // events during tracking.  [NSApp currentEvent] is still the original mouseDown
        // at this point!
        [bridge impl]->sendFakeEventsAfterWidgetTracking(event);
    }
    [event release];
    [bridge release];
    return result;
}

- (Widget *)widget
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
    if (highlighted && box) {
        box->populateMenu();
    }
    [super setHighlighted:highlighted];
}

@end

@implementation WebCorePopUpButton

- (void)action:(id)sender
{
    PopUpButton *box = static_cast<PopUpButton *>([self widget]);
    if (box) {
        box->itemSelected();
    }
}

- (Widget *)widget
{
    return [[self cell] widget];
}

- (void)mouseDown:(NSEvent *)event
{
    Widget::beforeMouseDown(self);
    [super mouseDown:event];
    Widget::afterMouseDown(self);
}

- (BOOL)becomeFirstResponder
{
    BOOL become = [super becomeFirstResponder];
    if (become) {
        Widget* widget = [self widget];
        if (widget && widget->client() && !FrameMac::currentEventIsMouseDownInWidget(widget))
            widget->client()->scrollToVisible(widget);
        if (widget && widget->client()) {
            widget->client()->focusIn(widget);
            [FrameMac::bridgeForWidget(widget) formControlIsBecomingFirstResponder:self];
        }
    }
    return become;
}

- (BOOL)resignFirstResponder
{
    BOOL resign = [super resignFirstResponder];
    if (resign) {
        Widget* widget = [self widget];
        if (widget && widget->client()) {
            widget->client()->focusOut(widget);
            [FrameMac::bridgeForWidget(widget) formControlIsResigningFirstResponder:self];
        }
    }
    return resign;
}

- (BOOL)needsPanelToBecomeKey
{
    // override this NSView method so that <select> elements gain focus when clicked - 4011544
    return YES;
}

- (BOOL)canBecomeKeyView
{
    // Simplified method from NSView; overridden to replace NSView's way of checking
    // for full keyboard access with ours.
    return ([self window] != nil) && ![self isHiddenOrHasHiddenAncestor] && [self acceptsFirstResponder];
}

- (NSView *)nextKeyView
{
    Widget *widget = [self widget];
    return widget && inNextValidKeyView
        ? FrameMac::nextKeyViewForWidget(widget, SelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    Widget *widget = [self widget];
    return widget && inNextValidKeyView
        ? FrameMac::nextKeyViewForWidget(widget, SelectingPrevious)
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

- (void)setPopulatingMenu:(BOOL)populating
{
    populatingMenu = populating;
}

- (void)setNeedsDisplayInRect:(NSRect)rect
{
    if (!populatingMenu) {
        [super setNeedsDisplayInRect:rect];
    }
}

@end
