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

// A listener class to act as a event target for NSColorPanel and send
// the results to the C++ class, WebColorPickerMac.
@interface WKColorPanelMac : NSObject<NSWindowDelegate> {
@private
    BOOL _lastChangedByUser;
    WebColorPickerMac* _picker;
}

- (id)init;
- (void)setAndShowPicker:(WebKit::WebColorPickerMac*)picker withColor:(NSColor *)color;
- (void)didChooseColor:(NSColorPanel *)panel;
- (void)invalidate;

// Sets color to the NSColorPanel as a non user change.
- (void)setColor:(NSColor *)color;

@end

namespace WebKit {

PassRefPtr<WebColorPickerMac> WebColorPickerMac::create(WebColorPicker::Client* client, const WebCore::Color& initialColor)
{
    return adoptRef(new WebColorPickerMac(client, initialColor));
}

WebColorPickerMac::~WebColorPickerMac()
{
    ASSERT(!m_panel);
}

WebColorPickerMac::WebColorPickerMac(WebColorPicker::Client* client, const WebCore::Color& initialColor)
    : WebColorPicker(client)
{
    m_panel = adoptNS([[WKColorPanelMac alloc] init]);
}

void WebColorPickerMac::endPicker()
{
    [m_panel invalidate];
    m_panel = nullptr;
}

void WebColorPickerMac::setSelectedColor(const WebCore::Color& color)
{
    if (!m_client || !m_panel)
        return;
    
    [m_panel setColor:nsColor(color)];
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

    if (!m_panel)
        m_panel = adoptNS([[WKColorPanelMac alloc] init]);

    [m_panel setAndShowPicker:this withColor:nsColor(color)];
}

} // namespace WebKit

using namespace WebKit;

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

- (void)didChooseColor:(NSColorPanel *)panel
{
    // Handle the case where the <input type='color'> value is programmatically set.
    if (!_lastChangedByUser) {
        _lastChangedByUser = YES;
        return;
    }

    _picker->didChooseColor(WebCore::colorFromNSColor([panel color]));
}

- (void)setColor:(NSColor *)color
{
    _lastChangedByUser = NO;
    [[NSColorPanel sharedColorPanel] setColor:color];
}

@end

#endif // USE(APPKIT)

#endif // ENABLE(INPUT_TYPE_COLOR)
