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
#import "RemoteLayerTreeDrawingAreaProxy.h"
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
    : m_page(page)
    , m_window(WTF::move(window))
    , m_mirrorRequest(NativePromiseRequest::create())
{
    attachLayer(page);
    updateSize();
}

TabCaptureWindow::~TabCaptureWindow()
{
    invalidate();
}

uint32_t TabCaptureWindow::windowID() const
{
    return static_cast<uint32_t>([m_window windowNumber]);
}

void TabCaptureWindow::attachLayer(WebPageProxy& page)
{
    RefPtr drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(page.drawingArea());
    if (!drawingArea) {
        RELEASE_LOG_ERROR(WebRTC, "TabCaptureWindow::attachLayer - drawing area is not a RemoteLayerTreeDrawingAreaProxy; mirror unavailable.");
        invalidate();
        m_readyPromiseProducer.reject();
        return;
    }

    drawingArea->enableMirrorLayerTree()->whenSettled(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }](auto&& result) {
        RefPtr protectedThis = weakThis;
        if (!protectedThis)
            return;

        protectedThis->m_mirrorRequest->complete();
        if (!result) {
            RELEASE_LOG_ERROR(WebRTC, "TabCaptureWindow::attachLayer - could not mirror the layer tree; aborting.");
            protectedThis->invalidate();
            protectedThis->m_readyPromiseProducer.reject();
            return;
        }

        // Set the layer first so that invalidate() disables the mirror even if we fail to use it.
        protectedThis->m_layer = result->layer;
        RetainPtr contentViewLayer = [[protectedThis->m_window contentView] layer];
        if (!contentViewLayer) {
            RELEASE_LOG_ERROR(WebRTC, "TabCaptureWindow::attachLayer - no content view layer; aborting.");
            protectedThis->invalidate();
            protectedThis->m_readyPromiseProducer.reject();
            return;
        }

        [contentViewLayer insertSublayer:result->layer.get() atIndex:0];
        protectedThis->updateSize();
        protectedThis->m_readyPromiseProducer.resolve();

        result->invalidationPromise->whenSettled(RunLoop::mainSingleton(), [weakThis] {
            RefPtr protectedThis = weakThis;
            if (!protectedThis || protectedThis->isInvalidated())
                return;

            RELEASE_LOG(WebRTC, "TabCaptureWindow::attachLayer - the mirror of the page layer tree went away, closing the window to stop capture.");
            protectedThis->invalidate();
        });
    })->track(m_mirrorRequest);
}

void TabCaptureWindow::invalidate()
{
    if (m_isInvalidated)
        return;

    m_isInvalidated = true;
    RetainPtr layer = std::exchange(m_layer, nullptr);
    [layer removeFromSuperlayer];
    [std::exchange(m_window, nullptr) close];

    bool isMirrorRequestPending = m_mirrorRequest->hasCallback();
    if (isMirrorRequestPending)
        m_mirrorRequest->disconnect();

    RefPtr page = m_page;
    if (!page)
        return;

    RefPtr drawingArea = dynamicDowncast<RemoteLayerTreeDrawingAreaProxy>(page->drawingArea());
    if (drawingArea && (isMirrorRequestPending || drawingArea->isMirrorLayer(layer)))
        drawingArea->disableMirrorLayerTree();

    page->tabCaptureWindowStateDidChange();
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
