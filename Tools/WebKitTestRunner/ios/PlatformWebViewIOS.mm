/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PlatformWebView.h"
#include "TestController.h"

#import <WebKit2/WKImageCG.h>
#import <WebKit2/WKViewPrivate.h>
#import <WebKit2/WKPreferencesPrivate.h>
#import <wtf/RetainPtr.h>

@interface WebKitTestRunnerWindow : UIWindow {
    WTR::PlatformWebView* _platformWebView;
    CGPoint _fakeOrigin;
}
@property (nonatomic, assign) WTR::PlatformWebView* platformWebView;
@end

@interface TestRunnerWKView : WKView {
    BOOL _useTiledDrawing;
}

- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage useTiledDrawing:(BOOL)useTiledDrawing;

@property (nonatomic, assign) BOOL useTiledDrawing;
@end

@implementation TestRunnerWKView

@synthesize useTiledDrawing = _useTiledDrawing;

- (id)initWithFrame:(CGRect)frame contextRef:(WKContextRef)contextRef pageGroupRef:(WKPageGroupRef)pageGroupRef relatedToPage:(WKPageRef)relatedPage useTiledDrawing:(BOOL)useTiledDrawing
{
    _useTiledDrawing = useTiledDrawing;
    return [super initWithFrame:frame contextRef:contextRef pageGroupRef:pageGroupRef];
}

- (BOOL)_shouldUseTiledDrawingArea
{
    return _useTiledDrawing;
}

@end

@implementation WebKitTestRunnerWindow
@synthesize platformWebView = _platformWebView;

- (BOOL)isKeyWindow
{
    return _platformWebView ? _platformWebView->windowIsKey() : YES;
}

- (void)setFrameOrigin:(CGPoint)point
{
    _fakeOrigin = point;
}

// FIXME: these frame gyrations cause the window to go half offscreen.
- (void)setFrame:(CGRect)windowFrame
{
    CGRect currentFrame = [super frame];

    _fakeOrigin = windowFrame.origin;

    [super setFrame:CGRectMake(currentFrame.origin.x, currentFrame.origin.y, windowFrame.size.width, windowFrame.size.height)];
}

- (CGRect)frameRespectingFakeOrigin
{
    CGRect currentFrame = [self frame];
    return CGRectMake(_fakeOrigin.x, _fakeOrigin.y, currentFrame.size.width, currentFrame.size.height);
}

- (CGFloat)backingScaleFactor
{
    return 1;
}

@end

@interface UIWindow (Details)

- (void)_setWindowResolution:(CGFloat)resolution displayIfChanged:(BOOL)displayIfChanged;

@end

namespace WTR {

PlatformWebView::PlatformWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef, WKPageRef relatedPage, WKDictionaryRef options)
    : m_windowIsKey(true)
    , m_options(options)
{
    WKRetainPtr<WKStringRef> useTiledDrawingKey(AdoptWK, WKStringCreateWithUTF8CString("TiledDrawing"));
    WKTypeRef useTiledDrawingValue = options ? WKDictionaryGetItemForKey(options, useTiledDrawingKey.get()) : NULL;
    bool useTiledDrawing = useTiledDrawingValue && WKBooleanGetValue(static_cast<WKBooleanRef>(useTiledDrawingValue));

    CGRect rect = CGRectMake(0, 0, TestController::viewWidth, TestController::viewHeight);
    m_view = [[TestRunnerWKView alloc] initWithFrame:rect contextRef:contextRef pageGroupRef:pageGroupRef relatedToPage:relatedPage useTiledDrawing:useTiledDrawing];

    WKPreferencesSetCompositingBordersVisible(WKPageGroupGetPreferences(pageGroupRef), YES);
    WKPreferencesSetCompositingRepaintCountersVisible(WKPageGroupGetPreferences(pageGroupRef), YES);

    CGRect windowRect = rect;
    m_window = [[WebKitTestRunnerWindow alloc] initWithFrame:windowRect];
    m_window.platformWebView = this;

    [m_window addSubview:m_view];
    [m_window makeKeyAndVisible];
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    CGRect windowFrame = [m_window frame];
    windowFrame.size.width = width;
    windowFrame.size.height = height;
    [m_window setFrame:windowFrame];
    [m_view setFrame:CGRectMake(0, 0, width, height)];
}

PlatformWebView::~PlatformWebView()
{
    m_window.platformWebView = 0;
//    [m_window close];
    [m_view release];
    [m_window release];
}

WKPageRef PlatformWebView::page()
{
    return [m_view pageRef];
}

void PlatformWebView::focus()
{
//    [m_window makeFirstResponder:m_view]; // FIXME: iOS equivalent?
    setWindowIsKey(true);
}

WKRect PlatformWebView::windowFrame()
{
    CGRect frame = [m_window frameRespectingFakeOrigin];

    WKRect wkFrame;
    wkFrame.origin.x = frame.origin.x;
    wkFrame.origin.y = frame.origin.y;
    wkFrame.size.width = frame.size.width;
    wkFrame.size.height = frame.size.height;
    return wkFrame;
}

void PlatformWebView::setWindowFrame(WKRect frame)
{
    [m_window setFrame:CGRectMake(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height)];
}

void PlatformWebView::didInitializeClients()
{
    // Set a temporary 1x1 window frame to force a WindowAndViewFramesChanged notification. <rdar://problem/13380145>
    WKRect wkFrame = windowFrame();
    [m_window setFrame:CGRectMake(0, 0, 1, 1)];
    setWindowFrame(wkFrame);
}

void PlatformWebView::addChromeInputField()
{
    UITextField* textField = [[UITextField alloc] initWithFrame:CGRectMake(0, 0, 100, 20)];
    textField.tag = 1;
    [m_window addSubview:textField];
    [textField release];
}

void PlatformWebView::removeChromeInputField()
{
    UITextField* textField = (UITextField*)[m_window viewWithTag:1];
    if (textField) {
        [textField removeFromSuperview];
        makeWebViewFirstResponder();
        [textField release];
    }
}

void PlatformWebView::makeWebViewFirstResponder()
{
//    [m_window makeFirstResponder:m_view];
}

void PlatformWebView::changeWindowScaleIfNeeded(float)
{
    // Retina only surface.
}

WKRetainPtr<WKImageRef> PlatformWebView::windowSnapshotImage()
{
    return 0; // FIXME for iOS?
}

bool PlatformWebView::viewSupportsOptions(WKDictionaryRef options) const
{
    WKRetainPtr<WKStringRef> useTiledDrawingKey(AdoptWK, WKStringCreateWithUTF8CString("TiledDrawing"));
    WKTypeRef useTiledDrawingValue = WKDictionaryGetItemForKey(options, useTiledDrawingKey.get());
    bool useTiledDrawing = useTiledDrawingValue && WKBooleanGetValue(static_cast<WKBooleanRef>(useTiledDrawingValue));

    return useTiledDrawing == [(TestRunnerWKView *)m_view useTiledDrawing];
}

} // namespace WTR
