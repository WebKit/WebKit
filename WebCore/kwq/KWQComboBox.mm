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
#import "KWQComboBox.h"

#import "BlockExceptions.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreTextRenderer.h"
#import "WebCoreWidgetHolder.h"
#import "render_form.h"

using namespace WebCore;

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

@interface KWQPopUpButtonCell : NSPopUpButtonCell <WebCoreWidgetHolder>
{
    QComboBox *box;
    NSWritingDirection baseWritingDirection;
}
- (id)initWithQComboBox:(QComboBox *)b;
- (void)detachQComboBox;
- (void)setBaseWritingDirection:(NSWritingDirection)direction;
- (NSWritingDirection)baseWritingDirection;
@end

@interface KWQPopUpButton : NSPopUpButton <WebCoreWidgetHolder>
{
    BOOL inNextValidKeyView;
    BOOL populatingMenu;
}
- (void)setPopulatingMenu:(BOOL)populating;

@end

QComboBox::QComboBox()
    : _widthGood(false)
    , _currentItem(0)
    , _menuPopulated(true)
    , _labelFont(nil)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    KWQPopUpButton *button = [[KWQPopUpButton alloc] init];
    setView(button);
    [button release];
    
    KWQPopUpButtonCell *cell = [[KWQPopUpButtonCell alloc] initWithQComboBox:this];
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

QComboBox::~QComboBox()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    [button setTarget:nil];
    [[button cell] detachQComboBox];
    KWQRelease(_labelFont);

    END_BLOCK_OBJC_EXCEPTIONS;
}

void QComboBox::setTitle(NSMenuItem *menuItem, const KWQListBoxItem &title)
{
    if (title.type == KWQListBoxGroupLabel) {
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

void QComboBox::appendItem(const DeprecatedString &text, KWQListBoxItemType type, bool enabled)
{
    const KWQListBoxItem listItem(text, type, enabled);
    _items.append(listItem);
    if (_menuPopulated) {
        KWQPopUpButton *button = (KWQPopUpButton *)getView();
        if (![[button cell] isHighlighted]) {
            _menuPopulated = false;
        } else {
            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            if (type == KWQListBoxSeparator) {
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

IntSize QComboBox::sizeHint() const 
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    
    if (!_widthGood) {
        float width = 0;
        DeprecatedValueListConstIterator<KWQListBoxItem> i = const_cast<const DeprecatedValueList<KWQListBoxItem> &>(_items).begin();
        DeprecatedValueListConstIterator<KWQListBoxItem> e = const_cast<const DeprecatedValueList<KWQListBoxItem> &>(_items).end();
        if (i != e) {
            WebCoreFont itemFont;
            WebCoreInitializeFont(&itemFont);
            itemFont.font = [button font];
            itemFont.forPrinter = ![NSGraphicsContext currentContextDrawingToScreen];
            id <WebCoreTextRenderer> itemRenderer = [[WebCoreTextRendererFactory sharedFactory] rendererWithFont:itemFont];
            id <WebCoreTextRenderer> labelRenderer = nil;
            WebCoreTextStyle style;
            WebCoreInitializeEmptyTextStyle(&style);
            style.applyRunRounding = NO;
            style.applyWordRounding = NO;
            do {
                const DeprecatedString &s = (*i).string;
                bool isGroupLabel = ((*i).type == KWQListBoxGroupLabel);
                ++i;

                WebCoreTextRun run;
                int length = s.length();
                WebCoreInitializeTextRun(&run, reinterpret_cast<const UniChar *>(s.unicode()), length, 0, length);

                id <WebCoreTextRenderer> renderer;
                if (isGroupLabel) {
                    if (labelRenderer == nil) {
                        WebCoreFont labelFont;
                        WebCoreInitializeFont(&labelFont);
                        labelFont.font = this->labelFont();
                        labelFont.forPrinter = ![NSGraphicsContext currentContextDrawingToScreen];
                        labelRenderer = [[WebCoreTextRendererFactory sharedFactory] rendererWithFont:labelFont];
                    }
                    renderer = labelRenderer;
                } else {
                    renderer = itemRenderer;
                }
                float textWidth = [renderer floatWidthForRun:&run style:&style];
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

IntRect QComboBox::frameGeometry() const
{
    IntRect r = Widget::frameGeometry();
    return IntRect(r.x() + dimensions()[leftMargin], r.y() + dimensions()[topMargin],
        r.width() - (dimensions()[leftMargin] + dimensions()[rightMargin]),
        r.height() - (dimensions()[topMargin] + dimensions()[bottomMargin]));
}

void QComboBox::setFrameGeometry(const IntRect& r)
{
    Widget::setFrameGeometry(IntRect(-dimensions()[leftMargin] + r.x(), -dimensions()[topMargin] + r.y(),
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

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
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

void QComboBox::itemSelected()
{
    ASSERT(_menuPopulated);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    KWQPopUpButton *button = (KWQPopUpButton *)getView();
    int i = [button indexOfSelectedItem];
    if (_currentItem == i) {
        return;
    }
    _currentItem = i;

    END_BLOCK_OBJC_EXCEPTIONS;

    if (client())
        client()->valueChanged(this);
}

void QComboBox::setFont(const Font& f)
{
    Widget::setFont(f);

    const NSControlSize size = KWQNSControlSizeForFont(f);
    NSControl * const button = static_cast<NSControl *>(getView());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (size != [[button cell] controlSize]) {
        [[button cell] setControlSize:size];
        [button setFont:[NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:size]]];
        KWQRelease(_labelFont);
        _labelFont = nil;
        _widthGood = false;
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

NSFont *QComboBox::labelFont() const
{
    if (_labelFont == nil) {
        NSControl * const button = static_cast<NSControl *>(getView());
        _labelFont = KWQRetain([NSFont boldSystemFontOfSize:[[button font] pointSize]]);
    }
    return _labelFont;
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

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return  w[[[button cell] controlSize]];
    END_BLOCK_OBJC_EXCEPTIONS;

    return w[NSSmallControlSize];
}

Widget::FocusPolicy QComboBox::focusPolicy() const
{
    FocusPolicy policy = Widget::focusPolicy();
    return policy == TabFocus ? StrongFocus : policy;
}

void QComboBox::setWritingDirection(TextDirection direction)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    KWQPopUpButton *button = static_cast<KWQPopUpButton *>(getView());
    KWQPopUpButtonCell *cell = [button cell];
    NSWritingDirection d = direction == RTL ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
    if ([cell baseWritingDirection] != d) {
        [cell setBaseWritingDirection:d];
        [button setNeedsDisplay:YES];
    }

    END_BLOCK_OBJC_EXCEPTIONS;
}

void QComboBox::populateMenu()
{
    if (!_menuPopulated) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;

        KWQPopUpButton *button = static_cast<KWQPopUpButton *>(getView());
        [button setPopulatingMenu:YES];
        [button removeAllItems];
        DeprecatedValueListConstIterator<KWQListBoxItem> i = const_cast<const DeprecatedValueList<KWQListBoxItem> &>(_items).begin();
        DeprecatedValueListConstIterator<KWQListBoxItem> e = const_cast<const DeprecatedValueList<KWQListBoxItem> &>(_items).end();
        for (; i != e; ++i) {
            if ((*i).type == KWQListBoxSeparator) {
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

void QComboBox::populate()
{
    populateMenu();
}

@implementation KWQPopUpButtonCell

- (id)initWithQComboBox:(QComboBox *)b
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

@implementation KWQPopUpButton

- (void)action:(id)sender
{
    QComboBox *box = static_cast<QComboBox *>([self widget]);
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
        if (widget && widget->client())
            widget->client()->focusIn(widget);
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
            if (widget)
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
        ? FrameMac::nextKeyViewForWidget(widget, KWQSelectingNext)
        : [super nextKeyView];
}

- (NSView *)previousKeyView
{
    Widget *widget = [self widget];
    return widget && inNextValidKeyView
        ? FrameMac::nextKeyViewForWidget(widget, KWQSelectingPrevious)
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
