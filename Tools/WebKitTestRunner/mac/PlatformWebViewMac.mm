/*
 * Copyright (C) 2010, 2013, 2015 Apple Inc. All rights reserved.
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
#import "PlatformWebView.h"

#import "TestController.h"
#import "TestRunnerWKWebView.h"
#import "WebKitTestRunnerDraggingInfo.h"
#import "WebKitTestRunnerWindow.h"
#import <WebKit/WKImageCG.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfiguration.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/mac/AppKitCompatibilityDeclarations.h>

#if WK_API_ENABLED
@interface WKWebView ()
- (WKPageRef)_pageForTesting;
@end
#endif

using namespace WTR;

// FIXME: Move to NSWindowSPI.h.
enum {
    _NSBackingStoreUnbuffered = 3
};

// FIXME: Move to NSWindowSPI.h.
@interface NSWindow ()
- (void)_setWindowResolution:(CGFloat)resolution;
// FIXME: Remove once the variant above exists on all platforms we need (cf. rdar://problem/47614795).
- (void)_setWindowResolution:(CGFloat)resolution displayIfChanged:(BOOL)displayIfChanged;
@end

namespace WTR {

PlatformWebView::PlatformWebView(WKWebViewConfiguration* configuration, const TestOptions& options)
    : m_windowIsKey(true)
    , m_options(options)
{
#if WK_API_ENABLED
    // FIXME: Not sure this is the best place for this; maybe we should have API to set this so we can do it from TestController?
    if (m_options.useRemoteLayerTree)
        [[NSUserDefaults standardUserDefaults] setValue:@YES forKey:@"WebKit2UseRemoteLayerTreeDrawingArea"];

    RetainPtr<WKWebViewConfiguration> copiedConfiguration = adoptNS([configuration copy]);
    WKPreferencesSetThreadedScrollingEnabled((__bridge WKPreferencesRef)[copiedConfiguration preferences], m_options.useThreadedScrolling);

    NSRect rect = NSMakeRect(0, 0, TestController::viewWidth, TestController::viewHeight);
    m_view = [[TestRunnerWKWebView alloc] initWithFrame:rect configuration:copiedConfiguration.get()];
    [m_view _setWindowOcclusionDetectionEnabled:NO];

    NSScreen *firstScreen = [[NSScreen screens] objectAtIndex:0];
    NSRect windowRect = m_options.shouldShowWebView ? NSOffsetRect(rect, 100, 100) : NSOffsetRect(rect, -10000, [firstScreen frame].size.height - rect.size.height + 10000);
    m_window = [[WebKitTestRunnerWindow alloc] initWithContentRect:windowRect styleMask:NSWindowStyleMaskBorderless backing:(NSBackingStoreType)_NSBackingStoreUnbuffered defer:YES];
    m_window.platformWebView = this;
    [m_window setColorSpace:[firstScreen colorSpace]];
    [m_window setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];
    [m_window setCollectionBehavior:NSWindowCollectionBehaviorStationary];
    [[m_window contentView] addSubview:m_view];
    if (m_options.shouldShowWebView)
        [m_window orderFront:nil];
    else
        [m_window orderBack:nil];
    [m_window setReleasedWhenClosed:NO];
#endif
}

void PlatformWebView::setWindowIsKey(bool isKey)
{
    m_windowIsKey = isKey;
    if (m_windowIsKey)
        [m_window makeKeyWindow];
    else
        [m_window resignKeyWindow];
}

void PlatformWebView::resizeTo(unsigned width, unsigned height, WebViewSizingMode sizingMode)
{
    WKRect frame = windowFrame();
    frame.size.width = width;
    frame.size.height = height;
    setWindowFrame(frame, sizingMode);
}

PlatformWebView::~PlatformWebView()
{
    m_window.platformWebView = nullptr;
    [m_window close];
    [m_window release];
    [m_view release];
}

PlatformWindow PlatformWebView::keyWindow()
{
    return [WebKitTestRunnerWindow _WTR_keyWindow];
}

WKPageRef PlatformWebView::page()
{
#if WK_API_ENABLED
    return [m_view _pageForTesting];
#else
    return nullptr;
#endif
}

void PlatformWebView::focus()
{
    [m_window makeFirstResponder:platformView()];
    setWindowIsKey(true);
}

WKRect PlatformWebView::windowFrame()
{
    NSRect frame = [m_window frameRespectingFakeOrigin];

    WKRect wkFrame;
    wkFrame.origin.x = frame.origin.x;
    wkFrame.origin.y = frame.origin.y;
    wkFrame.size.width = frame.size.width;
    wkFrame.size.height = frame.size.height;
    return wkFrame;
}

void PlatformWebView::setWindowFrame(WKRect frame, WebViewSizingMode)
{
    [m_window setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height) display:YES];
    [platformView() setFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height)];
}

void PlatformWebView::didInitializeClients()
{
    // Set a temporary 1x1 window frame to force a WindowAndViewFramesChanged notification. <rdar://problem/13380145>
    forceWindowFramesChanged();
}

void PlatformWebView::addChromeInputField()
{
    NSTextField *textField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 20)];
    textField.tag = 1;
    [[m_window contentView] addSubview:textField];
    [textField release];

    NSView *view = platformView();
    [textField setNextKeyView:view];
    [view setNextKeyView:textField];
}

void PlatformWebView::removeChromeInputField()
{
    NSView *textField = [[m_window contentView] viewWithTag:1];
    if (textField) {
        [textField removeFromSuperview];
        makeWebViewFirstResponder();
    }
}

void PlatformWebView::addToWindow()
{
    [[m_window contentView] addSubview:m_view];
}

void PlatformWebView::removeFromWindow()
{
    [m_view removeFromSuperview];
}

void PlatformWebView::makeWebViewFirstResponder()
{
    [m_window makeFirstResponder:platformView()];
}

bool PlatformWebView::drawsBackground() const
{
#if WK_API_ENABLED
    return [m_view _drawsBackground];
#else
    return false;
#endif
}

void PlatformWebView::setDrawsBackground(bool drawsBackground)
{
#if WK_API_ENABLED
    [m_view _setDrawsBackground:drawsBackground];
#else
    UNUSED_PARAM(drawsBackground);
#endif
}

void PlatformWebView::setEditable(bool editable)
{
#if WK_API_ENABLED
    m_view._editable = editable;
#else
    UNUSED_PARAM(editable);
#endif
}

RetainPtr<CGImageRef> PlatformWebView::windowSnapshotImage()
{
    [platformView() display];
    CGWindowImageOption options = kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque;

    if ([m_window backingScaleFactor] == 1)
        options |= kCGWindowImageNominalResolution;

    return adoptCF(CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, [m_window windowNumber], options));
}

void PlatformWebView::changeWindowScaleIfNeeded(float newScale)
{
    CGFloat currentScale = [m_window backingScaleFactor];
    if (currentScale == newScale)
        return;

    if ([m_window respondsToSelector:@selector(_setWindowResolution:)])
        [m_window _setWindowResolution:newScale];
    else
        [m_window _setWindowResolution:newScale displayIfChanged:YES];
#if WK_API_ENABLED
    [m_view _setOverrideDeviceScaleFactor:newScale];
#endif

    // Instead of re-constructing the current window, let's fake resize it to ensure that the scale change gets picked up.
    forceWindowFramesChanged();
    // Changing the scaling factor on the window does not trigger NSWindowDidChangeBackingPropertiesNotification. We need to send the notification manually.
    RetainPtr<NSMutableDictionary> notificationUserInfo = adoptNS([[NSMutableDictionary alloc] initWithCapacity:1]);
    [notificationUserInfo setObject:[NSNumber numberWithDouble:currentScale] forKey:NSBackingPropertyOldScaleFactorKey];
    [[NSNotificationCenter defaultCenter] postNotificationName:NSWindowDidChangeBackingPropertiesNotification object:m_window userInfo:notificationUserInfo.get()];
}

void PlatformWebView::forceWindowFramesChanged()
{
    WKRect wkFrame = windowFrame();
    [m_window setFrame:NSMakeRect(0, 0, 1, 1) display:YES];
    setWindowFrame(wkFrame);
}

void PlatformWebView::setNavigationGesturesEnabled(bool enabled)
{
#if WK_API_ENABLED
    [platformView() setAllowsBackForwardNavigationGestures:enabled];
#endif
}

} // namespace WTR
