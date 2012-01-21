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
#import "WebContext.h"
#import "WKInspectorMac.h"
#import "WKViewPrivate.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <WebKitSystemInterface.h>
#import <WebCore/InspectorFrontendClientLocal.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/NotImplemented.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;
using namespace WebKit;

// The height needed to match a typical NSToolbar.
static const CGFloat windowContentBorderThickness = 55;

// WKWebInspectorProxyObjCAdapter is a helper ObjC object used as a delegate or notification observer
// for the sole purpose of getting back into the C++ code from an ObjC caller.

@interface WKWebInspectorProxyObjCAdapter : NSObject <NSWindowDelegate> {
    WebInspectorProxy* _inspectorProxy; // Not retained to prevent cycles
}

- (id)initWithWebInspectorProxy:(WebInspectorProxy*)inspectorProxy;

@end

@implementation WKWebInspectorProxyObjCAdapter

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

- (void)inspectedViewFrameDidChange:(NSNotification *)notification
{
    _inspectorProxy->inspectedViewFrameDidChange();
}

@end

@interface WKWebInspectorWKView : WKView
@end

@implementation WKWebInspectorWKView

- (NSInteger)tag
{
    return WKInspectorViewTag;
}

@end

namespace WebKit {

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(m_page);
    ASSERT(!m_inspectorView);

    m_inspectorView.adoptNS([[WKWebInspectorWKView alloc] initWithFrame:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) contextRef:toAPI(page()->process()->context()) pageGroupRef:toAPI(inspectorPageGroup())]);
    ASSERT(m_inspectorView);

    [m_inspectorView.get() setDrawsBackground:NO];

    return toImpl(m_inspectorView.get().pageRef);
}

void WebInspectorProxy::platformOpen()
{
    ASSERT(!m_inspectorWindow);

    m_inspectorProxyObjCAdapter.adoptNS([[WKWebInspectorProxyObjCAdapter alloc] initWithWebInspectorProxy:this]);

    bool useTexturedWindow = page()->process()->context()->overrideWebInspectorPagePath().isEmpty();

    NSUInteger styleMask = (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask);
    if (useTexturedWindow)
        styleMask |= NSTexturedBackgroundWindowMask;

    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
    [window setDelegate:m_inspectorProxyObjCAdapter.get()];
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setReleasedWhenClosed:NO];

    if (useTexturedWindow) {
        [window setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
        [window setContentBorderThickness:windowContentBorderThickness forEdge:NSMaxYEdge];
        WKNSWindowMakeBottomCornersSquare(window);
    }

    // Center the window initially before setting the frame autosave name so that the window will be in a good
    // position if there is no saved frame yet.
    [window center];
    [window setFrameAutosaveName:@"Web Inspector 2"];

    NSView *contentView = [window contentView];
    [m_inspectorView.get() setFrame:[contentView bounds]];
    [m_inspectorView.get() setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [contentView addSubview:m_inspectorView.get()];

    m_inspectorWindow.adoptNS(window);

    if (m_isAttached)
        platformAttach();
    else
        [window makeKeyAndOrderFront:nil];
}

void WebInspectorProxy::platformDidClose()
{
    [m_inspectorWindow.get() setDelegate:nil];
    [m_inspectorWindow.get() orderOut:nil];

    m_inspectorWindow = 0;
    m_inspectorView = 0;
    m_inspectorProxyObjCAdapter = 0;
}

void WebInspectorProxy::platformBringToFront()
{
    // FIXME: this will not bring a background tab in Safari to the front, only its window.
    [m_inspectorView.get().window makeKeyAndOrderFront:nil];
}

void WebInspectorProxy::platformInspectedURLChanged(const String& urlString)
{
    NSString *title = [NSString stringWithFormat:WEB_UI_STRING("Web Inspector — %@", "Web Inspector window title"), (NSString *)urlString];
    [m_inspectorWindow.get() setTitle:title];
}

void WebInspectorProxy::inspectedViewFrameDidChange()
{
    if (!m_isAttached)
        return;

    WKView *inspectedView = m_page->wkView();
    NSRect inspectedViewFrame = [inspectedView frame];

    CGFloat inspectedLeft = NSMinX(inspectedViewFrame);
    CGFloat inspectedTop = NSMaxY(inspectedViewFrame);
    CGFloat inspectedWidth = NSWidth(inspectedViewFrame);
    CGFloat inspectorHeight = NSHeight([m_inspectorView.get() frame]);
    
    CGFloat parentHeight = NSHeight([[inspectedView superview] frame]);
    inspectorHeight = InspectorFrontendClientLocal::constrainedAttachedWindowHeight(inspectorHeight, parentHeight);

    [m_inspectorView.get() setFrame:NSMakeRect(inspectedLeft, 0.0, inspectedWidth, inspectorHeight)];
    [inspectedView setFrame:NSMakeRect(inspectedLeft, inspectorHeight, inspectedWidth, inspectedTop - inspectorHeight)];
}

unsigned WebInspectorProxy::platformInspectedWindowHeight()
{
    WKView *inspectedView = m_page->wkView();
    NSRect inspectedViewRect = [inspectedView frame];
    return static_cast<unsigned>(inspectedViewRect.size.height);
}

void WebInspectorProxy::platformAttach()
{
    WKView *inspectedView = m_page->wkView();
    [[NSNotificationCenter defaultCenter] addObserver:m_inspectorProxyObjCAdapter.get() selector:@selector(inspectedViewFrameDidChange:) name:NSViewFrameDidChangeNotification object:inspectedView];

    [m_inspectorView.get() removeFromSuperview];

    // The inspector view shares the width and the left starting point of the inspected view.
    NSRect inspectedViewFrame = [inspectedView frame];
    [m_inspectorView.get() setFrame:NSMakeRect(NSMinX(inspectedViewFrame), 0, NSWidth(inspectedViewFrame), inspectorPageGroup()->preferences()->inspectorAttachedHeight())];

    [[inspectedView superview] addSubview:m_inspectorView.get() positioned:NSWindowBelow relativeTo:inspectedView];

    [m_inspectorWindow.get() orderOut:nil];

    inspectedViewFrameDidChange();
}

void WebInspectorProxy::platformDetach()
{
    WKView *inspectedView = m_page->wkView();
    [[NSNotificationCenter defaultCenter] removeObserver:m_inspectorProxyObjCAdapter.get() name:NSViewFrameDidChangeNotification object:inspectedView];

    [m_inspectorView.get() removeFromSuperview];

    // Move the inspector view back into the inspector window.
    NSView *inspectorWindowContentView = [m_inspectorWindow.get() contentView];
    [m_inspectorView.get() setFrame:[inspectorWindowContentView bounds]];
    [inspectorWindowContentView addSubview:m_inspectorView.get()];

    // Make sure that we size the inspected view's frame after detaching so that it takes up the space that the
    // attached inspector used to. This assumes the previous height was the Y origin.
    NSRect inspectedViewRect = [inspectedView frame];
    inspectedViewRect.size.height += NSMinY(inspectedViewRect);
    inspectedViewRect.origin.y = 0.0;
    [inspectedView setFrame:inspectedViewRect];

    if (m_isVisible)
        [m_inspectorWindow.get() makeKeyAndOrderFront:nil];
}

void WebInspectorProxy::platformSetAttachedWindowHeight(unsigned height)
{
    if (!m_isAttached)
        return;

    WKView *inspectedView = m_page->wkView();
    NSRect inspectedViewFrame = [inspectedView frame];

    // The inspector view shares the width and the left starting point of the inspected view.
    [m_inspectorView.get() setFrame:NSMakeRect(NSMinX(inspectedViewFrame), 0.0, NSWidth(inspectedViewFrame), height)];

    inspectedViewFrameDidChange();
}

String WebInspectorProxy::inspectorPageURL() const
{
    NSString *path = page()->process()->context()->overrideWebInspectorPagePath();
    if (![path length])
        path = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"inspector" ofType:@"html" inDirectory:@"inspector"];

    ASSERT([path length]);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

String WebInspectorProxy::inspectorBaseURL() const
{
    NSString *path = page()->process()->context()->overrideWebInspectorBaseDirectory();
    if (![path length]) {
        // WebCore's Web Inspector uses localized strings, which are not contained within inspector directory.
        path = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] resourcePath];
    }

    ASSERT([path length]);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
