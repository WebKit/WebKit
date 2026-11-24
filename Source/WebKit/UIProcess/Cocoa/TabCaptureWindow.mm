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
#import "TabCaptureWindow.h"

#if HAVE(WINDOW_CAPTURE)

#import "Logging.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import <wtf/TZoneMallocInlines.h>

@interface WKTabCaptureFlippedView : NSView
@end

@implementation WKTabCaptureFlippedView
- (BOOL)isFlipped
{
    return YES;
}
@end

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TabCaptureWindow);

constexpr CGFloat initialWidth = 640;
constexpr CGFloat initialHeight = 480;
constexpr CGFloat offscreenCoordinate = -100000;

static RetainPtr<NSWindow> createOffscreenWindow()
{
    NSRect offscreenFrame = NSMakeRect(offscreenCoordinate, offscreenCoordinate, initialWidth, initialHeight);
    RetainPtr window = adoptNS([[NSWindow alloc] initWithContentRect:offscreenFrame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [window setReleasedWhenClosed:NO];
    [window setBackgroundColor:[NSColor whiteColor]];
    [window setCollectionBehavior:NSWindowCollectionBehaviorTransient];

    RetainPtr contentView = adoptNS([[WKTabCaptureFlippedView alloc] init]);
    [contentView setWantsLayer:YES];
    [window setContentView:contentView];
    [window orderFrontRegardless];

    if ([window windowNumber] <= 0) {
        RELEASE_LOG_ERROR(WebRTC, "TabCaptureWindow createOffscreenWindow - no windowNumber for window %p.", window.get());
        [window close];
        return nullptr;
    }

    return window;
}

RefPtr<TabCaptureWindow> TabCaptureWindow::create(WebPageProxy& page)
{
    RetainPtr window = createOffscreenWindow();
    if (!window)
        return nullptr;

    return adoptRef(*new TabCaptureWindow(page, WTF::move(window)));
}

TabCaptureWindow::TabCaptureWindow(WebPageProxy& page, RetainPtr<NSWindow>&& window)
    : m_pageIdentifier(page.identifier())
    , m_page(page)
    , m_window(WTF::move(window))
{
    attachLayer(page);
    updateSize();
}

TabCaptureWindow::~TabCaptureWindow()
{
    [m_window close];
}

uint32_t TabCaptureWindow::windowID() const
{
    return static_cast<uint32_t>([m_window windowNumber]);
}

void TabCaptureWindow::attachLayer(WebPageProxy& page)
{
    UNUSED_PARAM(page);
    // FIXME: Insert a mirror of the page layer tree.
}

void TabCaptureWindow::updateSize()
{
    RefPtr page = m_page;
    if (!page)
        return;

    RetainPtr webView = page->cocoaView();
    if (!webView)
        return;

    auto insets = page->obscuredContentInsets();
    RetainPtr contentViewLayer = [[m_window contentView] layer];
    auto sublayerTransform = CATransform3DMakeTranslation(-insets.left(), -insets.top(), 0);
    if (!CATransform3DEqualToTransform(sublayerTransform, [contentViewLayer sublayerTransform]))
        [contentViewLayer setSublayerTransform:sublayerTransform];

    NSSize size = [webView frame].size;
    size.width -= insets.left() + insets.right();
    size.height -= insets.top() + insets.bottom();
    if (size.width > 0 && size.height > 0 && !NSEqualSizes(size, [[m_window contentView] frame].size))
        [m_window setContentSize:size];
}

} // namespace WebKit

#endif // HAVE(WINDOW_CAPTURE)
