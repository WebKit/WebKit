/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebInspectorProxy.h"

#if ENABLE(INSPECTOR)

#import "WKAPICast.h"
#import "WKView.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <WebKitSystemInterface.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;
using namespace WebKit;

// The height needed to match a typical NSToolbar.
static const CGFloat windowContentBorderThickness = 55;

// WebInspectorProxyObjCAdapter is a helper ObjC object used as a delegate or notification observer
// for the sole purpose of getting back into the C++ code from an ObjC caller.

@interface WebInspectorProxyObjCAdapter : NSObject <NSWindowDelegate> {
    WebInspectorProxy* _inspectorProxy; // Not retained to prevent cycles
}

- (id)initWithWebInspectorProxy:(WebInspectorProxy*)inspectorProxy;

@end

@implementation WebInspectorProxyObjCAdapter

- (id)initWithWebInspectorProxy:(WebInspectorProxy*)inspectorProxy
{
    ASSERT_ARG(inspectorProxy, inspectorProxy);

    if (!(self = [super init]))
        return nil;

    _inspectorProxy = inspectorProxy; // Not retained to prevent cycles

    return self;
}

- (void)windowWillClose:(NSNotification *)notification
{
    _inspectorProxy->close();
}

@end

namespace WebKit {

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(m_page);
    ASSERT(!m_inspectorView);

    m_inspectorView.adoptNS([[WKView alloc] initWithFrame:NSZeroRect contextRef:toAPI(page()->process()->context()) pageGroupRef:toAPI(inspectorPageGroup())]);
    ASSERT(m_inspectorView);

    [m_inspectorView.get() setDrawsBackground:NO];

    return toImpl(m_inspectorView.get().pageRef);
}

void WebInspectorProxy::platformOpen()
{
    ASSERT(!m_inspectorWindow);

    m_inspectorProxyObjCAdapter.adoptNS([[WebInspectorProxyObjCAdapter alloc] initWithWebInspectorProxy:this]);

    // FIXME: support opening in docked mode here.

    NSUInteger styleMask = (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | NSTexturedBackgroundWindowMask);
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
    [window setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
    [window setContentBorderThickness:windowContentBorderThickness forEdge:NSMaxYEdge];
    [window setDelegate:m_inspectorProxyObjCAdapter.get()];
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setReleasedWhenClosed:NO];

    // Center the window initially before setting the frame autosave name so that the window will be in a good
    // position if there is no saved frame yet.
    [window center];
    [window setFrameAutosaveName:@"Web Inspector 2"];

    WKNSWindowMakeBottomCornersSquare(window);

    NSView *contentView = [window contentView];
    [m_inspectorView.get() setFrame:[contentView bounds]];
    [m_inspectorView.get() setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [contentView addSubview:m_inspectorView.get()];

    [window makeKeyAndOrderFront:nil];

    m_inspectorWindow.adoptNS(window);
}

void WebInspectorProxy::platformClose()
{
    // FIXME: support closing in docked mode here.

    [m_inspectorWindow.get() setDelegate:nil];
    [m_inspectorWindow.get() orderOut:nil];

    m_inspectorWindow = 0;
    m_inspectorView = 0;
    m_inspectorProxyObjCAdapter = 0;
}

void WebInspectorProxy::platformInspectedURLChanged(const String& urlString)
{
    NSString *title = [NSString stringWithFormat:WEB_UI_STRING("Web Inspector — %@", "Web Inspector window title"), (NSString *)urlString];
    [m_inspectorWindow.get() setTitle:title];
}

String WebInspectorProxy::inspectorPageURL() const
{
    NSString *path = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"inspector" ofType:@"html" inDirectory:@"inspector"];
    ASSERT(path);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
