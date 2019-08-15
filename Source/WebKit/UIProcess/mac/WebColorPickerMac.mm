/* 
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebColorPickerMac.h"

#if ENABLE(INPUT_TYPE_COLOR)

#if USE(APPKIT)

#import <WebCore/Color.h>
#import <WebCore/ColorMac.h>
#import <pal/spi/mac/NSColorWellSPI.h>
#import <pal/spi/mac/NSPopoverColorWellSPI.h>
#import <pal/spi/mac/NSPopoverSPI.h>
#import <wtf/WeakObjCPtr.h>

static const size_t maxColorSuggestions = 12;
static const CGFloat colorPickerMatrixNumColumns = 12.0;
static const CGFloat colorPickerMatrixBorderWidth = 1.0;

// FIXME: <rdar://problem/41173525> We should not have to track changes in NSPopoverColorWell's implementation.
static const CGFloat colorPickerMatrixSwatchWidth = 13.0;

@protocol WKPopoverColorWellDelegate <NSObject>
- (void)didClosePopover;
@end

@interface WKPopoverColorWell : NSPopoverColorWell {
    RetainPtr<NSColorList> _suggestedColors;
    WeakObjCPtr<id <WKPopoverColorWellDelegate>> _webDelegate;
}

@property (nonatomic, weak) id<WKPopoverColorWellDelegate> webDelegate;

- (void)setSuggestedColors:(NSColorList *)suggestedColors;
@end

@interface WKColorPopoverMac : NSObject<WKColorPickerUIMac, WKPopoverColorWellDelegate, NSWindowDelegate> {
@private
    BOOL _lastChangedByUser;
    WebKit::WebColorPickerMac *_picker;
    RetainPtr<WKPopoverColorWell> _popoverWell;
}
- (id)initWithFrame:(const WebCore::IntRect &)rect inView:(NSView *)view;
@end

namespace WebKit {

Ref<WebColorPickerMac> WebColorPickerMac::create(WebColorPicker::Client* client, const WebCore::Color& initialColor, const WebCore::IntRect& rect, Vector<WebCore::Color>&& suggestions, NSView *view)
{
    return adoptRef(*new WebColorPickerMac(client, initialColor, rect, WTFMove(suggestions), view));
}

WebColorPickerMac::~WebColorPickerMac()
{
    if (m_colorPickerUI) {
        [m_colorPickerUI invalidate];
        m_colorPickerUI = nil;
    }
}

WebColorPickerMac::WebColorPickerMac(WebColorPicker::Client* client, const WebCore::Color& initialColor, const WebCore::IntRect& rect, Vector<WebCore::Color>&& suggestions, NSView *view)
    : WebColorPicker(client)
    , m_suggestions(WTFMove(suggestions))
{
    m_colorPickerUI = adoptNS([[WKColorPopoverMac alloc] initWithFrame:rect inView:view]);
}

void WebColorPickerMac::endPicker()
{
    [m_colorPickerUI invalidate];
    m_colorPickerUI = nil;
    WebColorPicker::endPicker();
}

void WebColorPickerMac::setSelectedColor(const WebCore::Color& color)
{
    if (!m_client || !m_colorPickerUI)
        return;
    
    [m_colorPickerUI setColor:nsColor(color)];
}

void WebColorPickerMac::didChooseColor(const WebCore::Color& color)
{
    if (!m_client)
        return;
    
    m_client->didChooseColor(color);
}

void WebColorPickerMac::showColorPicker(const WebCore::Color& color)
{
    if (!m_client)
        return;

    [m_colorPickerUI setAndShowPicker:this withColor:nsColor(color) suggestions:WTFMove(m_suggestions)];
}

} // namespace WebKit

@implementation WKPopoverColorWell

+ (NSPopover *)_colorPopoverCreateIfNecessary:(BOOL)forceCreation
{
    static NSPopover *colorPopover = nil;
    if (forceCreation) {
        NSPopover *popover = [[NSPopover alloc] init];
        [popover _setRequiresCorrectContentAppearance:YES];
        popover.behavior = NSPopoverBehaviorTransient;

        NSColorPopoverController *controller = [[NSClassFromString(@"NSColorPopoverController") alloc] init];
        popover.contentViewController = controller;
        controller.popover = popover;
        [controller release];

        colorPopover = popover;
    }

    return colorPopover;
}

- (id <WKPopoverColorWellDelegate>)webDelegate
{
    return _webDelegate.getAutoreleased();
}

- (void)setWebDelegate:(id <WKPopoverColorWellDelegate>)webDelegate
{
    _webDelegate = webDelegate;
}

- (void)_showPopover
{
    NSPopover *popover = [[self class] _colorPopoverCreateIfNecessary:YES];
    popover.delegate = self;

    [self deactivate];

    // Deactivate previous NSPopoverColorWell
    NSColorWell *owner = [NSColorWell _exclusiveColorPanelOwner];
    if ([owner isKindOfClass:[NSPopoverColorWell class]])
        [owner deactivate];

    NSColorPopoverController *controller = (NSColorPopoverController *)[popover contentViewController];
    controller.delegate = self;

    if (_suggestedColors) {
        NSUInteger numColors = [[_suggestedColors allKeys] count];
        CGFloat swatchWidth = (colorPickerMatrixNumColumns * colorPickerMatrixSwatchWidth + (colorPickerMatrixNumColumns * colorPickerMatrixBorderWidth - numColors)) / numColors;
        CGFloat swatchHeight = colorPickerMatrixSwatchWidth;

        // topBarMatrixView cannot be accessed until view has been loaded
        if (!controller.isViewLoaded)
            [controller loadView];

        NSColorPickerMatrixView *topMatrix = controller.topBarMatrixView;
        [topMatrix setNumberOfColumns:numColors];
        [topMatrix setSwatchSize:NSMakeSize(swatchWidth, swatchHeight)];
        [topMatrix setColorList:_suggestedColors.get()];
    }

    [self activate:YES];
    [popover showRelativeToRect:self.bounds ofView:self preferredEdge:NSMinYEdge];
}

- (void)popoverDidClose:(NSNotification *)notification {
    [self.webDelegate didClosePopover];
}

- (NSView *)hitTest:(NSPoint)point
{
    return nil;
}

- (void)setSuggestedColors:(NSColorList *)suggestedColors
{
    _suggestedColors = suggestedColors;
}

@end

@implementation WKColorPopoverMac
- (id)initWithFrame:(const WebCore::IntRect &)rect inView:(NSView *)view
{
    if(!(self = [super init]))
        return self;

    _popoverWell = adoptNS([[WKPopoverColorWell alloc] initWithFrame:[view convertRect:NSRectFromCGRect(rect) toView:nil]]);
    if (!_popoverWell)
        return self;

    [_popoverWell setAlphaValue:0.0];
    [[view window].contentView addSubview:_popoverWell.get()];

    return self;
}

- (void)setAndShowPicker:(WebKit::WebColorPickerMac*)picker withColor:(NSColor *)color suggestions:(Vector<WebCore::Color>&&)suggestions
{
    _picker = picker;

    [_popoverWell setTarget:self];
    [_popoverWell setWebDelegate:self];
    [_popoverWell setAction:@selector(didChooseColor:)];
    [_popoverWell setColor:color];

    NSColorList *suggestedColors = nil;
    if (suggestions.size()) {
        suggestedColors = [[[NSColorList alloc] init] autorelease];
        for (size_t i = 0; i < std::min(suggestions.size(), maxColorSuggestions); i++)
            [suggestedColors insertColor:nsColor(suggestions.at(i)) key:@(i).stringValue atIndex:i];
    }

    [_popoverWell setSuggestedColors:suggestedColors];
    [_popoverWell _showPopover];

    [[NSColorPanel sharedColorPanel] setDelegate:self];
    
    _lastChangedByUser = YES;
}

- (void)invalidate
{
    [_popoverWell removeFromSuperviewWithoutNeedingDisplay];
    [_popoverWell setTarget:nil];
    [_popoverWell setAction:nil];
    [_popoverWell deactivate];
    
    _popoverWell = nil;
    _picker = nil;

    NSColorPanel *panel = [NSColorPanel sharedColorPanel];
    if (panel.delegate == self) {
        panel.delegate = nil;
        [panel close];
    }
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (!_picker)
        return;

    if (notification.object == [NSColorPanel sharedColorPanel]) {
        _lastChangedByUser = YES;
        _picker->endPicker();
    }
}

- (void)didChooseColor:(id)sender
{
    if (sender != _popoverWell)
        return;

    // Handle the case where the <input type='color'> value is programmatically set.
    if (!_lastChangedByUser) {
        _lastChangedByUser = YES;
        return;
    }

    _picker->didChooseColor(WebCore::colorFromNSColor([_popoverWell color]));
}

- (void)setColor:(NSColor *)color
{
    _lastChangedByUser = NO;
    [_popoverWell setColor:color];
}

- (void)didClosePopover
{
    if (!_picker)
        return;

    if (![NSColorPanel sharedColorPanel].isVisible)
        _picker->endPicker();
}

@end

#endif // USE(APPKIT)

#endif // ENABLE(INPUT_TYPE_COLOR)
