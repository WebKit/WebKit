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

#import "config.h"
#import "WKAccessibilityWebPageObjectIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "WebFrame.h"
#import "WebPage.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/Document.h>
#import <WebCore/DocumentView.h>
#import <WebCore/IntPoint.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/WAKAppKitStubs.h>

/* 
 The implementation of this class will be augmented by an accessibility bundle that is loaded only when accessibility is requested to be enabled.
 */

@implementation WKAccessibilityWebPageObject

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;
    
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_accessibilityCategoryInstalled:) name:@"AccessibilityCategoryInstalled" object:nil];

    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    SUPPRESS_UNRETAINED_ARG [_remoteTokenDictionary release];
    [super dealloc];
}

- (void)_accessibilityCategoryInstalled:(id)notification
{
    // Accessibility bundle will override this method so that it knows when to initialize the accessibility runtime within the WebProcess.
}

// Returns the root frame hosted in this process whose visible content contains the given screen point, preferring
// the given frame, and sets contentPoint to the point in that frame's content coordinates.
static RefPtr<WebCore::LocalFrame> rootFrameContainingScreenPoint(WebCore::Page& page, WebCore::LocalFrame* preferredFrame, const WebCore::IntPoint& screenPoint, WebCore::IntPoint& contentPoint)
{
    auto frameContainsScreenPoint = [&](WebCore::LocalFrame& frame) {
        RefPtr document = frame.document();
        CheckedPtr cache = document ? document->axObjectCache() : nullptr;
        CheckedPtr frameView = frame.view();
        if (!cache || !cache->frameGeometry() || !frameView)
            return false;

        auto point = cache->mapScreenPointToPagePoint(screenPoint);
        if (!WebCore::IntRect(frameView->scrollPosition(), frameView->visibleSize()).contains(point))
            return false;

        contentPoint = point;
        return true;
    };

    if (RefPtr frame = preferredFrame; frame && frameContainsScreenPoint(*frame))
        return frame;

    for (auto& weakRootFrame : page.rootFrames()) {
        Ref rootFrame = weakRootFrame;
        if (rootFrame.ptr() != preferredFrame && frameContainsScreenPoint(rootFrame))
            return rootFrame;
    }
    return nullptr;
}

- (double)pageScale
{
    return protect(m_page)->pageScaleFactor();
}

- (id)accessibilityHitTest:(NSPoint)point
{
    if (!m_page)
        return nil;

    RefPtr localFocusedFrame = [self localFocusedFrame];

    WebCore::IntPoint convertedPoint;

#if ENABLE(PDF_PLUGIN)
    if (m_hasMainFramePlugin && ![self shouldFallbackToWebContentAXObjectForMainFramePlugin]) {
        // PDF plug-in handles the scroll view offset natively as part of the layer conversions, so don't
        // do a coordinate conversion for those hit tests.
        convertedPoint = WebCore::IntPoint { point };
    } else
#endif // ENABLE(PDF_PLUGIN)
    if (!is<WebCore::LocalFrame>(protect(m_page)->mainFrame())) {
        // This process hosts cross-site iframes, and the root view belongs to the main frame, so let a frame's
        // cache convert using the frame's own screen position. A hit test for any of the iframes in this process
        // can arrive at the element for just one of them, so hit test whichever iframe contains the point.
        // We can't use localFocusedFrame for this check, because that will always return a local frame (that isn't necessarily the main frame).
        RefPtr corePage = protect(m_page)->corePage();
        if (RefPtr frame = corePage ? rootFrameContainingScreenPoint(*corePage, localFocusedFrame.get(), WebCore::IntPoint(point), convertedPoint) : nullptr)
            localFocusedFrame = WTF::move(frame);
        else {
            RefPtr document = localFocusedFrame ? localFocusedFrame->document() : nullptr;
            if (CheckedPtr cache = document ? document->axObjectCache() : nullptr)
                convertedPoint = cache->mapScreenPointToPagePoint(WebCore::IntPoint(point));
        }
    } else
        convertedPoint = protect(m_page)->accessibilityScreenToRootView(WebCore::IntPoint(point));

    return [[self accessibilityRootObjectWrapper:localFocusedFrame.get()] accessibilityHitTest:convertedPoint];
}

@end

#endif // PLATFORM(IOS_FAMILY)

