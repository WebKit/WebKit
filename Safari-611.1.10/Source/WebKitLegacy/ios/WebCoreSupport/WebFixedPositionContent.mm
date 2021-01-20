/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "WebFixedPositionContent.h"
#import "WebFixedPositionContentInternal.h"

#import "WebViewInternal.h"
#import <WebCore/ChromeClient.h>
#import <WebCore/Frame.h>
#import <WebCore/IntSize.h>
#import <WebCore/ScrollingConstraints.h>
#import <WebCore/WebCoreThreadRun.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>

#import <wtf/HashMap.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>

#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#import <algorithm>

using namespace WebCore;
using namespace std;

static Lock webFixedPositionContentDataLock;

struct ViewportConstrainedLayerData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    ViewportConstrainedLayerData()
        : m_enclosingAcceleratedScrollLayer(nil)
    { }
    CALayer* m_enclosingAcceleratedScrollLayer; // May be nil.
    std::unique_ptr<ViewportConstraints> m_viewportConstraints;
};

typedef HashMap<RetainPtr<CALayer>, std::unique_ptr<ViewportConstrainedLayerData>> LayerInfoMap;

struct WebFixedPositionContentData {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
public:
    WebFixedPositionContentData(WebView *);
    ~WebFixedPositionContentData();
    
    WebView *m_webView;
    LayerInfoMap m_viewportConstrainedLayers;
};


WebFixedPositionContentData::WebFixedPositionContentData(WebView *webView)
    : m_webView(webView)
{
}

WebFixedPositionContentData::~WebFixedPositionContentData()
{
}

@implementation WebFixedPositionContent {
    struct WebFixedPositionContentData* _private;
}

- (id)initWithWebView:(WebView *)webView
{
    if ((self = [super init])) {
        _private = new WebFixedPositionContentData(webView);
    }
    return self;
}

- (void)dealloc
{
    delete _private;
    [super dealloc];
}

- (void)scrollOrZoomChanged:(CGRect)positionedObjectsRect
{
    auto locker = holdLock(webFixedPositionContentDataLock);

    LayerInfoMap::const_iterator end = _private->m_viewportConstrainedLayers.end();
    for (LayerInfoMap::const_iterator it = _private->m_viewportConstrainedLayers.begin(); it != end; ++it) {
        CALayer *layer = it->key.get();
        ViewportConstrainedLayerData* constraintData = it->value.get();
        const ViewportConstraints& constraints = *(constraintData->m_viewportConstraints.get());

        switch (constraints.constraintType()) {
        case ViewportConstraints::FixedPositionConstraint: {
                const FixedPositionViewportConstraints& fixedConstraints = static_cast<const FixedPositionViewportConstraints&>(constraints);

                FloatPoint layerPosition = fixedConstraints.layerPositionForViewportRect(positionedObjectsRect);
            
                CGRect layerBounds = [layer bounds];
                CGPoint anchorPoint = [layer anchorPoint];
                CGPoint newPosition = CGPointMake(layerPosition.x() - constraints.alignmentOffset().width() + anchorPoint.x * layerBounds.size.width,
                                                  layerPosition.y() - constraints.alignmentOffset().height() + anchorPoint.y * layerBounds.size.height);
                [layer setPosition:newPosition];
                break;
            }
        case ViewportConstraints::StickyPositionConstraint: {
                const StickyPositionViewportConstraints& stickyConstraints = static_cast<const StickyPositionViewportConstraints&>(constraints);

                FloatPoint layerPosition = stickyConstraints.layerPositionForConstrainingRect(positionedObjectsRect);

                CGRect layerBounds = [layer bounds];
                CGPoint anchorPoint = [layer anchorPoint];
                CGPoint newPosition = CGPointMake(layerPosition.x() - constraints.alignmentOffset().width() + anchorPoint.x * layerBounds.size.width,
                                                  layerPosition.y() - constraints.alignmentOffset().height() + anchorPoint.y * layerBounds.size.height);
                [layer setPosition:newPosition];
                break;
            }
        }
    }
}

- (void)overflowScrollPositionForLayer:(CALayer *)scrollLayer changedTo:(CGPoint)scrollPosition
{
    auto locker = holdLock(webFixedPositionContentDataLock);

    LayerInfoMap::const_iterator end = _private->m_viewportConstrainedLayers.end();
    for (LayerInfoMap::const_iterator it = _private->m_viewportConstrainedLayers.begin(); it != end; ++it) {
        CALayer *layer = it->key.get();
        ViewportConstrainedLayerData* constraintData = it->value.get();
        
        if (constraintData->m_enclosingAcceleratedScrollLayer == scrollLayer) {
            const StickyPositionViewportConstraints& stickyConstraints = static_cast<const StickyPositionViewportConstraints&>(*(constraintData->m_viewportConstraints.get()));
            FloatRect constrainingRectAtLastLayout = stickyConstraints.constrainingRectAtLastLayout();
            FloatRect scrolledConstrainingRect = FloatRect(scrollPosition.x, scrollPosition.y, constrainingRectAtLastLayout.width(), constrainingRectAtLastLayout.height());
            FloatPoint layerPosition = stickyConstraints.layerPositionForConstrainingRect(scrolledConstrainingRect);

            CGRect layerBounds = [layer bounds];
            CGPoint anchorPoint = [layer anchorPoint];
            CGPoint newPosition = CGPointMake(layerPosition.x() - stickyConstraints.alignmentOffset().width() + anchorPoint.x * layerBounds.size.width,
                                              layerPosition.y() - stickyConstraints.alignmentOffset().height() + anchorPoint.y * layerBounds.size.height);
            [layer setPosition:newPosition];
        }
    }
}

// FIXME: share code with 'sendScrollEvent'?
- (void)didFinishScrollingOrZooming
{
    WebThreadRun(^{
        if (Frame* frame = [_private->m_webView _mainCoreFrame])
            frame->viewportOffsetChanged(Frame::CompletedScrollOffset);
    });
}

- (void)setViewportConstrainedLayers:(WTF::HashMap<CALayer *, std::unique_ptr<WebCore::ViewportConstraints>>&)layerMap stickyContainerMap:(const WTF::HashMap<CALayer*, CALayer*>&)stickyContainers
{
    auto locker = holdLock(webFixedPositionContentDataLock);

    _private->m_viewportConstrainedLayers.clear();

    for (auto& layerAndConstraints : layerMap) {
        CALayer* layer = layerAndConstraints.key;
        auto layerData = makeUnique<ViewportConstrainedLayerData>();

        layerData->m_enclosingAcceleratedScrollLayer = stickyContainers.get(layer);
        layerData->m_viewportConstraints = WTFMove(layerAndConstraints.value);

        _private->m_viewportConstrainedLayers.set(layer, WTFMove(layerData));
    }
}

- (BOOL)hasFixedOrStickyPositionLayers
{
    auto locker = holdLock(webFixedPositionContentDataLock);
    return !_private->m_viewportConstrainedLayers.isEmpty();
}

@end

#endif // PLATFORM(IOS_FAMILY)
