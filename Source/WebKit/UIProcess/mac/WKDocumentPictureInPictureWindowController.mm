/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "WKDocumentPictureInPictureWindowController.h"

#if ENABLE(DOCUMENT_PIP)

#import "APIPageConfiguration.h"
#import "WKWebViewConfigurationInternal.h"
#import "WKWebViewInternal.h"
#import "WebViewImpl.h"
#import <AppKit/AppKit.h>
#import <WebCore/WindowFeatures.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

// The user agent controls the size and placement of document picture-in-picture windows
// to prevent sites from covering the screen or spoofing other UI.
// https://wicg.github.io/document-picture-in-picture/#maximum-size
static constexpr CGFloat defaultContentWidth = 480;
static constexpr CGFloat defaultContentHeight = 270;
static constexpr CGFloat minimumContentWidth = 240;
static constexpr CGFloat minimumContentHeight = 135;
static constexpr CGFloat maximumScreenFraction = 0.5;
static constexpr CGFloat screenEdgeInset = 20;
static constexpr NSTimeInterval cornerSnapAnimationDuration = 0.2;
static constexpr NSTimeInterval dragEndPollingInterval = 1. / 60;

static void* openerAppearanceObservationContext = &openerAppearanceObservationContext;

@implementation WKDocumentPictureInPictureWindowController {
    RetainPtr<WKWebView> _webView;
    RetainPtr<NSPanel> _window;
    WeakObjCPtr<WKWebView> _openerWebView;
    NSSize _maximumContentSize;
    BOOL _isClosing;
    BOOL _isObservingOpenerAppearance;
    RetainPtr<NSTimer> _dragEndTimer;
}

- (instancetype)initWithOpenerWebView:(WKWebView *)openerWebView configuration:(API::PageConfiguration&)configuration
{
    if (!(self = [super init]))
        return nil;

    _openerWebView = openerWebView;

    RetainPtr screen = [[openerWebView window] screen] ?: [NSScreen mainScreen];
    NSRect visibleFrame = [screen visibleFrame];
    _maximumContentSize = NSMakeSize(std::max(minimumContentWidth, std::floor(NSWidth(visibleFrame) * maximumScreenFraction)), std::max(minimumContentHeight, std::floor(NSHeight(visibleFrame) * maximumScreenFraction)));

    auto& windowFeatures = configuration.windowFeatures();
    NSSize contentSize = NSMakeSize(defaultContentWidth, defaultContentHeight);
    if (windowFeatures && windowFeatures->width && windowFeatures->height)
        contentSize = NSMakeSize(*windowFeatures->width, *windowFeatures->height);
    contentSize = [self _clampedContentSize:contentSize];

    // https://wicg.github.io/document-picture-in-picture/#origin-visibility
    RetainPtr<NSString> title;
    if (auto& openerInfo = configuration.openerInfo())
        title = openerInfo->securityOrigin.toString().createNSString();

    _webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, contentSize.width, contentSize.height) configuration:protect(wrapper(configuration)).get()]);
    [_webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    if (CheckedPtr impl = [_webView _impl])
        impl->setHostingDocumentPictureInPictureWindowController(self);

    // Share the opener's delegates so that alerts, downloads, and similar requests from the
    // picture-in-picture window are handled by the client like those from the opener.
    // FIXME: Add UIDelegate methods for creating the WKWebView for DocumentPiP.
    [_webView setUIDelegate:[openerWebView UIDelegate]];
    [_webView setNavigationDelegate:[openerWebView navigationDelegate]];

    // The HUD panel forces a dark appearance on its contents. Have the web view follow the opener's
    // appearance instead, so that moving content into the picture-in-picture window does not change
    // its color scheme.
    [_webView setAppearance:[openerWebView effectiveAppearance]];
    [openerWebView addObserver:self forKeyPath:@"effectiveAppearance" options:0 context:openerAppearanceObservationContext];
    _isObservingOpenerAppearance = YES;

    // https://wicg.github.io/document-picture-in-picture/#positioning
    // The initial position is chosen by the user agent: the bottom trailing corner of the opener's screen.
    // FIXME: Add support for "remembering" the last used corner of the screen
    NSRect contentRect = NSMakeRect(NSMaxX(visibleFrame) - contentSize.width - screenEdgeInset, NSMinY(visibleFrame) + screenEdgeInset, contentSize.width, contentSize.height);
    // Use a non-activating HUD panel so that interacting with the picture-in-picture window does not
    // activate the application or bring the opener's window forward, and so that the title bar is compact.
    _window = adoptNS([[NSPanel alloc] initWithContentRect:contentRect styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskNonactivatingPanel | NSWindowStyleMaskUtilityWindow | NSWindowStyleMaskHUDWindow backing:NSBackingStoreBuffered defer:NO]);
    [_window setReleasedWhenClosed:NO];
    [_window setRestorable:NO];
    [_window setDelegate:self];
    [_window setTitle:protect(title.get() ?: @"")];
    [_window setTabbingMode:NSWindowTabbingModeDisallowed];
    [_window setHidesOnDeactivate:NO];

    // Float above other windows, follow the user across Spaces, and appear over full screen apps,
    // but never enter full screen.
    [_window setFloatingPanel:YES];
    [_window setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorFullScreenAuxiliary | NSWindowCollectionBehaviorFullScreenNone | NSWindowCollectionBehaviorFullScreenDisallowsTiling];

    // Only offer the close button; the window cannot be minimized, and it is resized by dragging its edges.
    [[_window standardWindowButton:NSWindowMiniaturizeButton] setHidden:YES];
    [[_window standardWindowButton:NSWindowZoomButton] setHidden:YES];

    [_window setContentMinSize:NSMakeSize(minimumContentWidth, minimumContentHeight)];
    [_window setContentMaxSize:_maximumContentSize];
    [_window setContentView:_webView.get()];

    return self;
}

- (void)dealloc
{
    [_dragEndTimer invalidate];
    [self _stopObservingOpenerAppearance];
    [_window setDelegate:nil];
    [super dealloc];
}

- (void)_stopObservingOpenerAppearance
{
    if (!_isObservingOpenerAppearance)
        return;
    _isObservingOpenerAppearance = NO;

    if (RetainPtr openerWebView = _openerWebView.get())
        [openerWebView removeObserver:self forKeyPath:@"effectiveAppearance" context:openerAppearanceObservationContext];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
    if (context != openerAppearanceObservationContext) {
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
        return;
    }

    if (RetainPtr openerWebView = _openerWebView.get())
        [_webView setAppearance:[openerWebView effectiveAppearance]];
}

- (WKWebView *)webView
{
    return _webView.get();
}

- (NSPanel *)window
{
    return _window.get();
}

- (WebKit::WebPageProxy *)page
{
    return [_webView _page].get();
}

#pragma mark Corner snapping

- (NSRect)_frameSnappedToNearestCorner:(NSRect)frame
{
    RetainPtr screen = [_window screen] ?: [NSScreen mainScreen];
    NSRect visibleFrame = [screen visibleFrame];
    NSPoint center = NSMakePoint(NSMidX(frame), NSMidY(frame));

    frame.origin.x = center.x < NSMidX(visibleFrame) ? NSMinX(visibleFrame) + screenEdgeInset : NSMaxX(visibleFrame) - NSWidth(frame) - screenEdgeInset;
    frame.origin.y = center.y < NSMidY(visibleFrame) ? NSMinY(visibleFrame) + screenEdgeInset : NSMaxY(visibleFrame) - NSHeight(frame) - screenEdgeInset;
    return frame;
}

- (void)_snapToNearestCorner
{
    // Like video picture-in-picture, holding Command while moving the window places it freely.
    if (_isClosing || ([NSEvent modifierFlags] & NSEventModifierFlagCommand))
        return;

    NSRect snappedFrame = [self _frameSnappedToNearestCorner:[_window frame]];
    if (NSEqualRects(snappedFrame, [_window frame]))
        return;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = cornerSnapAnimationDuration;
        [[_window animator] setFrame:snappedFrame display:YES];
    }];
}

- (void)_waitForDragToEnd
{
    // The window server moves the window without delivering the mouse-up to the application,
    // so poll for the mouse button being released to detect the end of the drag.
    [_dragEndTimer invalidate];
    _dragEndTimer = [NSTimer scheduledTimerWithTimeInterval:dragEndPollingInterval repeats:YES block:makeBlockPtr([weakSelf = WeakObjCPtr<WKDocumentPictureInPictureWindowController>(self)](NSTimer *timer) {
        RetainPtr strongSelf = weakSelf.get();
        if (!strongSelf) {
            [timer invalidate];
            return;
        }
        if ([NSEvent pressedMouseButtons] & 1)
            return;
        [timer invalidate];
        strongSelf->_dragEndTimer = nil;
        [strongSelf _snapToNearestCorner];
    }).get()];
}

- (NSSize)_clampedContentSize:(NSSize)size
{
    return NSMakeSize(std::clamp(size.width, minimumContentWidth, _maximumContentSize.width), std::clamp(size.height, minimumContentHeight, _maximumContentSize.height));
}

- (void)show
{
    if (_isClosing)
        return;
    [_window makeKeyAndOrderFront:nil];
}

- (void)focus
{
    [self show];
}

- (void)close
{
    if (_isClosing)
        return;
    [_window close];
}

- (void)setWindowFrame:(NSRect)windowFrame
{
    if (_isClosing)
        return;

    // https://wicg.github.io/document-picture-in-picture/#moving-the-pip-window
    // Only the size of the window may be changed by the page. Keep a window that is snapped to a
    // corner anchored to that corner; otherwise keep its top-left corner fixed.
    NSRect currentFrame = [_window frame];
    bool isSnappedToCorner = NSEqualRects(currentFrame, [self _frameSnappedToNearestCorner:currentFrame]);
    NSRect contentRect = [_window contentRectForFrameRect:NSMakeRect(0, 0, NSWidth(windowFrame), NSHeight(windowFrame))];
    contentRect.size = [self _clampedContentSize:contentRect.size];
    NSRect newFrame = [_window frameRectForContentRect:contentRect];
    newFrame.origin = NSMakePoint(NSMinX(currentFrame), NSMaxY(currentFrame) - NSHeight(newFrame));
    if (isSnappedToCorner) {
        newFrame.origin.x = NSMidX(currentFrame) < NSMidX([[_window screen] visibleFrame]) ? NSMinX(currentFrame) : NSMaxX(currentFrame) - NSWidth(newFrame);
        newFrame.origin.y = NSMidY(currentFrame) < NSMidY([[_window screen] visibleFrame]) ? NSMinY(currentFrame) : NSMaxY(currentFrame) - NSHeight(newFrame);
    }
    [_window setFrame:newFrame display:YES animate:NO];
}

#pragma mark NSWindowDelegate

- (void)windowWillMove:(NSNotification *)notification
{
    [self _waitForDragToEnd];
}

- (void)windowDidEndLiveResize:(NSNotification *)notification
{
    [self _snapToNearestCorner];
}

- (void)windowWillClose:(NSNotification *)notification
{
    if (_isClosing)
        return;
    _isClosing = YES;

    [_dragEndTimer invalidate];
    _dragEndTimer = nil;

    RetainPtr protectedSelf = self;
    [self _stopObservingOpenerAppearance];
    [_webView _close];

    if (RetainPtr openerWebView = _openerWebView.get()) {
        if (CheckedPtr openerImpl = [openerWebView _impl])
            openerImpl->documentPictureInPictureWindowControllerDidClose(self);
    }
}

@end

#endif // ENABLE(DOCUMENT_PIP)
