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

#include "config.h"
#include "PlatformWebView.h"

#import <WebKit2/WKImageCG.h>
#import <WebKit2/WKViewPrivate.h>
#import <wtf/RetainPtr.h>

@interface WebKitTestRunnerWindow : NSWindow {
    WTR::PlatformWebView* _platformWebView;
}
@property (nonatomic, assign) WTR::PlatformWebView* platformWebView;
@end

@implementation WebKitTestRunnerWindow
@synthesize platformWebView = _platformWebView;

- (BOOL)isKeyWindow
{
    return _platformWebView ? _platformWebView->windowIsKey() : YES;
}
@end

namespace WTR {

PlatformWebView::PlatformWebView(WKContextRef contextRef, WKPageGroupRef pageGroupRef)
    : m_windowIsKey(true)
{
    NSRect rect = NSMakeRect(0, 0, 800, 600);
    m_view = [[WKView alloc] initWithFrame:rect contextRef:contextRef pageGroupRef:pageGroupRef];

    NSRect windowRect = NSOffsetRect(rect, -10000, [(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame].size.height - rect.size.height + 10000);
    m_window = [[WebKitTestRunnerWindow alloc] initWithContentRect:windowRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
    m_window.platformWebView = this;
    [m_window setColorSpace:[[NSScreen mainScreen] colorSpace]];
    [[m_window contentView] addSubview:m_view];
    [m_window orderBack:nil];
    [m_window setReleasedWhenClosed:NO];
}

void PlatformWebView::resizeTo(unsigned width, unsigned height)
{
    [m_view setFrame:NSMakeRect(0, 0, width, height)];
}

PlatformWebView::~PlatformWebView()
{
    m_window.platformWebView = 0;
    [m_window close];
    [m_window release];
    [m_view release];
}

WKPageRef PlatformWebView::page()
{
    return [m_view pageRef];
}

void PlatformWebView::focus()
{
    [m_window makeFirstResponder:m_view];
    setWindowIsKey(true);
}

WKRect PlatformWebView::windowFrame()
{
    NSRect frame = [m_window frame];

    WKRect wkFrame;
    wkFrame.origin.x = frame.origin.x;
    wkFrame.origin.y = frame.origin.y;
    wkFrame.size.width = frame.size.width;
    wkFrame.size.height = frame.size.height;
    return wkFrame;
}

void PlatformWebView::setWindowFrame(WKRect frame)
{
    [m_window setFrame:NSMakeRect(frame.origin.x, frame.origin.y, frame.size.width, frame.size.height) display:YES];
}

void PlatformWebView::addChromeInputField()
{
    NSTextField* textField = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 100, 20)];
    textField.tag = 1;
    [[m_window contentView] addSubview:textField];
    [textField release];

    [textField setNextKeyView:m_view];
    [m_view setNextKeyView:textField];
}

void PlatformWebView::removeChromeInputField()
{
    NSView* textField = [[m_window contentView] viewWithTag:1];
    if (textField) {
        [textField removeFromSuperview];
        makeWebViewFirstResponder();
    }
}

void PlatformWebView::makeWebViewFirstResponder()
{
    [m_window makeFirstResponder:m_view];
}

WKRetainPtr<WKImageRef> PlatformWebView::windowSnapshotImage()
{
    [m_view display];
    RetainPtr<CGImageRef> windowSnapshotImage(AdoptCF, CGWindowListCreateImage(CGRectNull, kCGWindowListOptionIncludingWindow, [m_window windowNumber], kCGWindowImageBoundsIgnoreFraming | kCGWindowImageShouldBeOpaque));
    
    // windowSnapshotImage will be in the display's color space, but WKImageCreateFromCGImage() will draw
    // this image into a GenericRGB bitmap context, so the returned image is GenericRGB.
    return adoptWK(WKImageCreateFromCGImage(windowSnapshotImage.get(), 0));
}

} // namespace WTR
