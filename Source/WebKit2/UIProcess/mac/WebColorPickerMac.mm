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

#import <WebCore/ColorMac.h>

using namespace WebKit;

#if ENABLE(INPUT_TYPE_COLOR_POPOVER)

// The methods we use from NSPopoverColorWell aren't declared in its header
// so there's no benefit to trying to include them. Instead we just declare
// the class and methods here.
@interface NSPopoverColorWell : NSColorWell
@end

@interface NSPopoverColorWell (AppKitSecretsIKnow)
- (void)_showPopover;
@end

@interface WKColorPopoverMac : NSObject<WKColorPickerUIMac, NSWindowDelegate> {
@private
    BOOL _lastChangedByUser;
    WebColorPickerMac *_picker;
    RetainPtr<NSPopoverColorWell> _popoverWell;
}
- (id)initWithFrame:(const WebCore::IntRect &)rect inView:(WKView *)view;
@end

#else

@interface WKColorPanelMac : NSObject<WKColorPickerUIMac, NSWindowDelegate> {
@private
    BOOL _lastChangedByUser;
    WebColorPickerMac *_picker;
}
- (id)init;
@end

#endif // ENABLE(INPUT_TYPE_COLOR_POPOVER)

namespace WebKit {

PassRefPtr<WebColorPickerMac> WebColorPickerMac::create(WebColorPicker::Client* client, const WebCore::Color& initialColor, const WebCore::IntRect& rect, WKView* view)
{
    return adoptRef(new WebColorPickerMac(client, initialColor, rect, view));
}

WebColorPickerMac::~WebColorPickerMac()
{
    if (m_colorPickerUI)
        endPicker();
}

WebColorPickerMac::WebColorPickerMac(WebColorPicker::Client* client, const WebCore::Color& initialColor, const WebCore::IntRect& rect, WKView* view)
    : WebColorPicker(client)
{
#if ENABLE(INPUT_TYPE_COLOR_POPOVER)
    m_colorPickerUI = adoptNS([[WKColorPopoverMac alloc] initWithFrame:rect inView:view]);
#else
    m_colorPickerUI = adoptNS([[WKColorPanelMac alloc] init]);
#endif
}

void WebColorPickerMac::endPicker()
{
    [m_colorPickerUI invalidate];
    m_colorPickerUI = nil;
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

#if !ENABLE(INPUT_TYPE_COLOR_POPOVER)
    if (!m_colorPickerUI)
        m_colorPickerUI = adoptNS([[WKColorPanelMac alloc] init]);
#endif

    [m_colorPickerUI setAndShowPicker:this withColor:nsColor(color)];
}

} // namespace WebKit

#if ENABLE(INPUT_TYPE_COLOR_POPOVER)

@implementation WKColorPopoverMac
- (id)initWithFrame:(const WebCore::IntRect &)rect inView:(WKView *)view
{
    if(!(self = [super init]))
        return self;

    _popoverWell = adoptNS([[NSPopoverColorWell alloc] initWithFrame:[view convertRect:NSRectFromCGRect(rect) toView:nil]]);
    if (!_popoverWell)
        return self;

    [[view window].contentView addSubview:_popoverWell.get()];

    return self;
}

- (void)setAndShowPicker:(WebKit::WebColorPickerMac*)picker withColor:(NSColor *)color
{
    _picker = picker;

    [_popoverWell setTarget:self];
    [_popoverWell setAction:@selector(didChooseColor:)];
    [_popoverWell setColor:color];
    [_popoverWell _showPopover];
    
    _lastChangedByUser = YES;
}
- (void)dealloc
{
    ASSERT(!_popoverWell);
    ASSERT(!_picker);
    [super dealloc];
}

- (void)invalidate
{
    [_popoverWell removeFromSuperviewWithoutNeedingDisplay];
    [_popoverWell setTarget:nil];
    [_popoverWell setAction:nil];
    [_popoverWell deactivate];
    _popoverWell = nil;
    _picker = nil;
}

- (void)windowWillClose:(NSNotification *)notification
{
    _lastChangedByUser = YES;
    _picker->endPicker();
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

@end

#else

@implementation WKColorPanelMac

- (id)init
{
    self = [super init];
    return self;
}

- (void)setAndShowPicker:(WebColorPickerMac*)picker withColor:(NSColor *)color
{
    _picker = picker;

    NSColorPanel *panel = [NSColorPanel sharedColorPanel];

    [panel setShowsAlpha:NO];
    [panel setDelegate:self];
    [panel setTarget:self];

    [panel setColor:color];

    _lastChangedByUser = YES;
    [panel setAction:@selector(didChooseColor:)];
    [panel makeKeyAndOrderFront:nil];
}

- (void)invalidate
{
    NSColorPanel *panel = [NSColorPanel sharedColorPanel];
    if ([panel delegate] == self) {
        [panel setDelegate:nil];
        [panel setTarget:nil];
        [panel setAction:nil];
    }
    _picker = nil;
}

- (void)windowWillClose:(NSNotification *)notification
{
    _lastChangedByUser = YES;
    _picker->endPicker();
}

- (void)didChooseColor:(id)sender
{
    if (sender != [NSColorPanel sharedColorPanel])
        return;

    // Handle the case where the <input type='color'> value is programmatically set.
    if (!_lastChangedByUser) {
        _lastChangedByUser = YES;
        return;
    }

    _picker->didChooseColor(WebCore::colorFromNSColor([sender color]));
}

- (void)setColor:(NSColor *)color
{
    _lastChangedByUser = NO;
    [[NSColorPanel sharedColorPanel] setColor:color];
}

@end

#endif // ENABLE(INPUT_TYPE_COLOR_POPOVER)

#endif // USE(APPKIT)

#endif // ENABLE(INPUT_TYPE_COLOR)
