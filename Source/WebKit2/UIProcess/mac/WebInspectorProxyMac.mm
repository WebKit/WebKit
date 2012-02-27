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

// These methods can be used by UI elements such as menu items and toolbar buttons when the inspector is the main/key window.
// These methods are really only implemented to keep  UI elements enabled and working.

- (void)showWebInspector:(id)sender
{
    // This method is a toggle, so it will close the Web Inspector if it is already front. Since the window delegate is handling
    // the action, we know it is already showing, so we only need to support close here. To keep this working with older Safari
    // builds the method name is still just "showWebInspector:" instead of something like "toggleWebInspector:".
    ASSERT(_inspectorProxy->isFront());
    _inspectorProxy->close();
}

- (void)showErrorConsole:(id)sender
{
    _inspectorProxy->showConsole();
}

- (void)showResources:(id)sender
{
    _inspectorProxy->showResources();
}

- (void)viewSource:(id)sender
{
    _inspectorProxy->showMainResourceForFrame(_inspectorProxy->page()->mainFrame());
}

- (void)toggleDebuggingJavaScript:(id)sender
{
    _inspectorProxy->toggleJavaScriptDebugging();
}

- (void)toggleProfilingJavaScript:(id)sender
{
    _inspectorProxy->toggleJavaScriptProfiling();
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    BOOL isMenuItem = [(id)item isKindOfClass:[NSMenuItem class]];
    NSMenuItem *menuItem = (NSMenuItem *)item;
    if ([item action] == @selector(showWebInspector:) && isMenuItem) {
        // This action is a toggle, so it will close the Web Inspector if it is already front. Since the window delegate is handling
        // the action, we know it is already showing, so we only need to update the title to mention hide.
        ASSERT(_inspectorProxy->isFront());
        [menuItem setTitle:WEB_UI_STRING("Hide Web Inspector", "title for Hide Web Inspector menu item")];
    } else if ([item action] == @selector(toggleDebuggingJavaScript:) && isMenuItem) {
        if (_inspectorProxy->isDebuggingJavaScript())
            [menuItem setTitle:WEB_UI_STRING("Stop Debugging JavaScript", "title for Stop Debugging JavaScript menu item")];
        else
            [menuItem setTitle:WEB_UI_STRING("Start Debugging JavaScript", "title for Start Debugging JavaScript menu item")];
    } else if ([item action] == @selector(toggleProfilingJavaScript:) && isMenuItem) {
        if (_inspectorProxy->isProfilingJavaScript())
            [menuItem setTitle:WEB_UI_STRING("Stop Profiling JavaScript", "title for Stop Profiling JavaScript menu item")];
        else
            [menuItem setTitle:WEB_UI_STRING("Start Profiling JavaScript", "title for Start Profiling JavaScript menu item")];
    }

    return YES;
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

void WebInspectorProxy::createInspectorWindow()
{
    ASSERT(!m_inspectorWindow);

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

    NSView *contentView = [window contentView];
    [m_inspectorView.get() setFrame:[contentView bounds]];
    [contentView addSubview:m_inspectorView.get()];

    // Center the window initially before setting the frame autosave name so that the window will be in a good
    // position if there is no saved frame yet.
    [window center];
    [window setFrameAutosaveName:@"Web Inspector 2"];

    m_inspectorWindow.adoptNS(window);

    updateInspectorWindowTitle();
}

void WebInspectorProxy::updateInspectorWindowTitle() const
{
    if (!m_inspectorWindow)
        return;

    NSString *title = [NSString stringWithFormat:WEB_UI_STRING("Web Inspector — %@", "Web Inspector window title"), (NSString *)m_urlString];
    [m_inspectorWindow.get() setTitle:title];
}

WebPageProxy* WebInspectorProxy::platformCreateInspectorPage()
{
    ASSERT(m_page);
    ASSERT(!m_inspectorView);

    m_inspectorView.adoptNS([[WKWebInspectorWKView alloc] initWithFrame:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) contextRef:toAPI(page()->process()->context()) pageGroupRef:toAPI(inspectorPageGroup())]);
    ASSERT(m_inspectorView);

    [m_inspectorView.get() setDrawsBackground:NO];
    [m_inspectorView.get() setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

    m_inspectorProxyObjCAdapter.adoptNS([[WKWebInspectorProxyObjCAdapter alloc] initWithWebInspectorProxy:this]);

    if (m_isAttached)
        platformAttach();
    else
        createInspectorWindow();

    return toImpl(m_inspectorView.get().pageRef);
}

void WebInspectorProxy::platformOpen()
{
    if (m_isAttached) {
        // Make the inspector view visible since it was hidden while loading.
        [m_inspectorView.get() setHidden:NO];

        // Adjust the frames now that we are visible and inspectedViewFrameDidChange wont return early.
        inspectedViewFrameDidChange();
    } else
        [m_inspectorWindow.get() makeKeyAndOrderFront:nil];
}

void WebInspectorProxy::platformDidClose()
{
    if (m_inspectorWindow) {
        [m_inspectorWindow.get() setDelegate:nil];
        [m_inspectorWindow.get() orderOut:nil];
        m_inspectorWindow = 0;
    }

    m_inspectorView = 0;

    m_inspectorProxyObjCAdapter = 0;
}

void WebInspectorProxy::platformBringToFront()
{
    // FIXME <rdar://problem/10937688>: this will not bring a background tab in Safari to the front, only its window.
    [m_inspectorView.get().window makeKeyAndOrderFront:nil];
}

bool WebInspectorProxy::platformIsFront()
{
    // FIXME <rdar://problem/10937688>: this will not return false for a background tab in Safari, only a background window.
    return m_isVisible && [m_inspectorView.get().window isMainWindow];
}

void WebInspectorProxy::platformInspectedURLChanged(const String& urlString)
{
    m_urlString = urlString;

    updateInspectorWindowTitle();
}

void WebInspectorProxy::inspectedViewFrameDidChange()
{
    if (!m_isAttached || !m_isVisible)
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

    // Start out hidden if we are not visible yet. When platformOpen is called, hidden will be set to NO.
    [m_inspectorView.get() setHidden:!m_isVisible];

    [[inspectedView superview] addSubview:m_inspectorView.get() positioned:NSWindowBelow relativeTo:inspectedView];

    if (m_inspectorWindow) {
        [m_inspectorWindow.get() setDelegate:nil];
        [m_inspectorWindow.get() orderOut:nil];
        m_inspectorWindow = 0;
    }

    inspectedViewFrameDidChange();
}

void WebInspectorProxy::platformDetach()
{
    WKView *inspectedView = m_page->wkView();
    [[NSNotificationCenter defaultCenter] removeObserver:m_inspectorProxyObjCAdapter.get() name:NSViewFrameDidChangeNotification object:inspectedView];

    [m_inspectorView.get() removeFromSuperview];

    createInspectorWindow();

    // Make the inspector view visible in case it is still hidden from loading while attached.
    [m_inspectorView.get() setHidden:NO];

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
