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
#import "WKInspectorPrivateMac.h"
#import "WKViewPrivate.h"
#import "WebPageGroup.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebProcessProxy.h"
#import <WebKitSystemInterface.h>
#import <WebCore/InspectorFrontendClientLocal.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SoftLinking.h>
#import <wtf/text/WTFString.h>

SOFT_LINK_STAGED_FRAMEWORK_OPTIONAL(WebInspector, PrivateFrameworks, A)

using namespace WebCore;
using namespace WebKit;

// The height needed to match a typical NSToolbar.
static const CGFloat windowContentBorderThickness = 55;

// The margin from the top and right of the dock button (same as the full screen button).
static const CGFloat dockButtonMargin = 3;

// WKWebInspectorProxyObjCAdapter is a helper ObjC object used as a delegate or notification observer
// for the sole purpose of getting back into the C++ code from an ObjC caller.

@interface WKWebInspectorProxyObjCAdapter ()

- (id)initWithWebInspectorProxy:(WebInspectorProxy*)inspectorProxy;
- (void)close;

@end

@implementation WKWebInspectorProxyObjCAdapter

- (WKInspectorRef)inspectorRef
{
    return toAPI(static_cast<WebInspectorProxy*>(_inspectorProxy));
}

- (id)initWithWebInspectorProxy:(WebInspectorProxy*)inspectorProxy
{
    ASSERT_ARG(inspectorProxy, inspectorProxy);

    if (!(self = [super init]))
        return nil;

    _inspectorProxy = static_cast<void*>(inspectorProxy); // Not retained to prevent cycles

    return self;
}

- (IBAction)attach:(id)sender
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->attach();
}

- (void)close
{
    _inspectorProxy = 0;
}

- (void)windowWillClose:(NSNotification *)notification
{
    static_cast<WebInspectorProxy*>(_inspectorProxy)->close();
}

- (void)inspectedViewFrameDidChange:(NSNotification *)notification
{
    // Resizing the views while inside this notification can lead to bad results when entering
    // or exiting full screen. To avoid that we need to perform the work after a delay. We only
    // depend on this for enforcing the height constraints, so a small delay isn't terrible. Most
    // of the time the views will already have the correct frames because of autoresizing masks.

    dispatch_after(DISPATCH_TIME_NOW, dispatch_get_main_queue(), ^{
        if (!_inspectorProxy)
            return;
        static_cast<WebInspectorProxy*>(_inspectorProxy)->inspectedViewFrameDidChange();
    });
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

@interface NSWindow (AppKitDetails)
- (NSCursor *)_cursorForResizeDirection:(NSInteger)direction;
- (NSRect)_customTitleFrame;
@end

@interface WKWebInspectorWindow : NSWindow {
@public
    RetainPtr<NSButton> _dockButton;
}
@end

@implementation WKWebInspectorWindow

- (NSCursor *)_cursorForResizeDirection:(NSInteger)direction
{
    // Don't show a resize cursor for the northeast (top right) direction if the dock button is visible.
    // This matches what happens when the full screen button is visible.
    if (direction == 1 && ![_dockButton isHidden])
        return nil;
    return [super _cursorForResizeDirection:direction];
}

- (NSRect)_customTitleFrame
{
    // Adjust the title frame if needed to prevent it from intersecting the dock button.
    NSRect titleFrame = [super _customTitleFrame];
    NSRect dockButtonFrame = _dockButton.get().frame;
    if (NSMaxX(titleFrame) > NSMinX(dockButtonFrame) - dockButtonMargin)
        titleFrame.size.width -= (NSMaxX(titleFrame) - NSMinX(dockButtonFrame)) + dockButtonMargin;
    return titleFrame;
}

@end

namespace WebKit {

static bool inspectorReallyUsesWebKitUserInterface(WebPreferences* preferences)
{
    // This matches a similar check in WebInspectorMac.mm. Keep them in sync.

    // Call the soft link framework function to dlopen it, then [NSBundle bundleWithIdentifier:] will work.
    WebInspectorLibrary();

    if (![[NSBundle bundleWithIdentifier:@"com.apple.WebInspector"] pathForResource:@"Main" ofType:@"html"])
        return true;

    if (![[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"inspector" ofType:@"html" inDirectory:@"inspector"])
        return false;

    return preferences->inspectorUsesWebKitUserInterface();
}

static WKRect getWindowFrame(WKPageRef, const void* clientInfo)
{
    WebInspectorProxy* webInspectorProxy = static_cast<WebInspectorProxy*>(const_cast<void*>(clientInfo));
    ASSERT(webInspectorProxy);

    return webInspectorProxy->inspectorWindowFrame();
}

static void setWindowFrame(WKPageRef, WKRect frame, const void* clientInfo)
{
    WebInspectorProxy* webInspectorProxy = static_cast<WebInspectorProxy*>(const_cast<void*>(clientInfo));
    ASSERT(webInspectorProxy);

    webInspectorProxy->setInspectorWindowFrame(frame);
}

void WebInspectorProxy::setInspectorWindowFrame(WKRect& frame)
{
    if (m_isAttached)
        return;
    [m_inspectorWindow setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height) display:YES];
}

WKRect WebInspectorProxy::inspectorWindowFrame()
{
    if (m_isAttached)
        return WKRectMake(0, 0, 0, 0);

    NSRect frame = m_inspectorWindow.get().frame;
    return WKRectMake(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
}

void WebInspectorProxy::createInspectorWindow()
{
    ASSERT(!m_inspectorWindow);

    NSUInteger styleMask = (NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | NSTexturedBackgroundWindowMask);
    WKWebInspectorWindow *window = [[WKWebInspectorWindow alloc] initWithContentRect:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
    [window setDelegate:m_inspectorProxyObjCAdapter.get()];
    [window setMinSize:NSMakeSize(minimumWindowWidth, minimumWindowHeight)];
    [window setReleasedWhenClosed:NO];
    [window setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
    [window setContentBorderThickness:windowContentBorderThickness forEdge:NSMaxYEdge];
    WKNSWindowMakeBottomCornersSquare(window);

    NSView *contentView = [window contentView];

    // Create a full screen button so we can turn it into a dock button.
    m_dockButton = [NSWindow standardWindowButton:NSWindowFullScreenButton forStyleMask:styleMask];
    m_dockButton.get().target = m_inspectorProxyObjCAdapter.get();
    m_dockButton.get().action = @selector(attach:);

    // Store the dock button on the window too so it can check its visibility.
    window->_dockButton = m_dockButton;

    // Get the dock image and make it a template so the button cell effects will apply.
    NSImage *dockImage = [[NSBundle bundleForClass:[WKWebInspectorWKView class]] imageForResource:@"Dock"];
    [dockImage setTemplate:YES];

    // Set the dock image on the button cell.
    NSCell *dockButtonCell = m_dockButton.get().cell;
    dockButtonCell.image = dockImage;

    // Get the frame view, the superview of the content view, and its frame.
    // This will be the superview of the dock button too.
    NSView *frameView = contentView.superview;
    NSRect frameViewBounds = frameView.bounds;
    NSSize dockButtonSize = m_dockButton.get().frame.size;

    ASSERT(!frameView.isFlipped);

    // Position the dock button in the corner to match where the full screen button is normally.
    NSPoint dockButtonOrigin;
    dockButtonOrigin.x = NSMaxX(frameViewBounds) - dockButtonSize.width - dockButtonMargin;
    dockButtonOrigin.y = NSMaxY(frameViewBounds) - dockButtonSize.height - dockButtonMargin;
    m_dockButton.get().frameOrigin = dockButtonOrigin;

    // Set the autoresizing mask to keep the dock button pinned to the top right corner.
    m_dockButton.get().autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;

    [frameView addSubview:m_dockButton.get()];

    // Hide the dock button if we can't attach.
    m_dockButton.get().hidden = !canAttach();

    [m_inspectorView.get() setFrame:[contentView bounds]];
    [m_inspectorView.get() setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
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

    m_inspectorView.adoptNS([[WKWebInspectorWKView alloc] initWithFrame:NSMakeRect(0, 0, initialWindowWidth, initialWindowHeight) contextRef:toAPI(page()->process()->context()) pageGroupRef:toAPI(inspectorPageGroup()) relatedToPage:toAPI(m_page)]);
    ASSERT(m_inspectorView);

    [m_inspectorView.get() setDrawsBackground:NO];

    m_inspectorProxyObjCAdapter.adoptNS([[WKWebInspectorProxyObjCAdapter alloc] initWithWebInspectorProxy:this]);

    if (m_isAttached)
        platformAttach();
    else
        createInspectorWindow();

    WebPageProxy* inspectorPage = toImpl(m_inspectorView.get().pageRef);

    WKPageUIClient uiClient = {
        kWKPageUIClientCurrentVersion,
        this,   /* clientInfo */
        0, // createNewPage_deprecatedForUseWithV0
        0, // showPage
        0, // closePage
        0, // takeFocus
        0, // focus
        0, // unfocus
        0, // runJavaScriptAlert
        0, // runJavaScriptConfirm
        0, // runJavaScriptPrompt
        0, // setStatusText
        0, // mouseDidMoveOverElement_deprecatedForUseWithV0
        0, // missingPluginButtonClicked_deprecatedForUseWithV0
        0, // didNotHandleKeyEvent
        0, // didNotHandleWheelEvent
        0, // areToolbarsVisible
        0, // setToolbarsVisible
        0, // isMenuBarVisible
        0, // setMenuBarVisible
        0, // isStatusBarVisible
        0, // setStatusBarVisible
        0, // isResizable
        0, // setResizable
        getWindowFrame,
        setWindowFrame,
        0, // runBeforeUnloadConfirmPanel
        0, // didDraw
        0, // pageDidScroll
        0, // exceededDatabaseQuota
        0, // runOpenPanel
        0, // decidePolicyForGeolocationPermissionRequest
        0, // headerHeight
        0, // footerHeight
        0, // drawHeader
        0, // drawFooter
        0, // printFrame
        0, // runModal
        0, // unused
        0, // saveDataToFileInDownloadsFolder
        0, // shouldInterruptJavaScript
        0, // createPage
        0, // mouseDidMoveOverElement
        0, // decidePolicyForNotificationPermissionRequest
        0, // unavailablePluginButtonClicked
        0, // showColorPicker
        0, // hideColorPicker
    };

    inspectorPage->initializeUIClient(&uiClient);

    return inspectorPage;
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

    [m_inspectorProxyObjCAdapter.get() close];
    m_inspectorProxyObjCAdapter = 0;
}

void WebInspectorProxy::platformBringToFront()
{
    // FIXME <rdar://problem/10937688>: this will not bring a background tab in Safari to the front, only its window.
    [m_inspectorView.get().window makeKeyAndOrderFront:nil];
    [m_inspectorView.get().window makeFirstResponder:m_inspectorView.get()];
}

bool WebInspectorProxy::platformIsFront()
{
    // FIXME <rdar://problem/10937688>: this will not return false for a background tab in Safari, only a background window.
    return m_isVisible && [m_inspectorView.get().window isMainWindow];
}

void WebInspectorProxy::platformAttachAvailabilityChanged(bool available)
{
    m_dockButton.get().hidden = !available;
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

    [m_inspectorView.get() setAutoresizingMask:NSViewWidthSizable | NSViewMaxYMargin];

    // Start out hidden if we are not visible yet. When platformOpen is called, hidden will be set to NO.
    [m_inspectorView.get() setHidden:!m_isVisible];

    [[inspectedView superview] addSubview:m_inspectorView.get() positioned:NSWindowBelow relativeTo:inspectedView];
    [[inspectedView window] makeFirstResponder:m_inspectorView.get()];

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

    // Make sure that we size the inspected view's frame after detaching so that it takes up the space that the
    // attached inspector used to. This assumes the previous height was the Y origin.
    NSRect inspectedViewRect = [inspectedView frame];
    inspectedViewRect.size.height += NSMinY(inspectedViewRect);
    inspectedViewRect.origin.y = 0.0;
    [inspectedView setFrame:inspectedViewRect];

    // Return early if we are not visible. This means the inspector was closed while attached
    // and we should not create and show the inspector window.
    if (!m_isVisible)
        return;

    createInspectorWindow();

    // Make the inspector view visible in case it is still hidden from loading while attached.
    [m_inspectorView.get() setHidden:NO];

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
    NSString *path;
    if (inspectorReallyUsesWebKitUserInterface(page()->pageGroup()->preferences()))
        path = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] pathForResource:@"inspector" ofType:@"html" inDirectory:@"inspector"];
    else
        path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspector"] pathForResource:@"Main" ofType:@"html"];

    ASSERT([path length]);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

String WebInspectorProxy::inspectorBaseURL() const
{
    NSString *path;
    if (inspectorReallyUsesWebKitUserInterface(page()->pageGroup()->preferences()))
        path = [[NSBundle bundleWithIdentifier:@"com.apple.WebCore"] resourcePath];
    else
        path = [[NSBundle bundleWithIdentifier:@"com.apple.WebInspector"] resourcePath];

    ASSERT([path length]);

    return [[NSURL fileURLWithPath:path] absoluteString];
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
