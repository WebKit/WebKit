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
#import <WebCore/Document.h>
#import <WebCore/IntSize.h>
#import <WebCore/LocalFrame.h>
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

static Lock webFixedPositionContentDataLock;

struct ViewportConstrainedLayerData {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(ViewportConstrainedLayerData);
    ViewportConstrainedLayerData()
        : m_enclosingAcceleratedScrollLayer(nil)
    { }
    CALayer* m_enclosingAcceleratedScrollLayer; // May be nil.
    std::unique_ptr<ViewportConstraints> m_viewportConstraints;
};

typedef HashMap<RetainPtr<CALayer>, std::unique_ptr<ViewportConstrainedLayerData>> LayerInfoMap;

struct WebFixedPositionContentData {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(WebFixedPositionContentData);
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

WebFixedPositionContentData::~WebFixedPositionContentData() = default;

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
    Locker locker { webFixedPositionContentDataLock };

    LayerInfoMap::const_iterator end = _private->m_viewportConstrainedLayers.end();
    for (LayerInfoMap::const_iterator it = _private->m_viewportConstrainedLayers.begin(); it != end; ++it) {
        CALayer *layer = it->key.get();
        ViewportConstrainedLayerData* constraintData = it->value.get();
        const ViewportConstraints& constraints = *(constraintData->m_viewportConstraints.get());

        switch (constraints.constraintType()) {
        case ViewportConstraints::FixedPositionConstraint: {
            auto& fixedConstraints = downcast<FixedPositionViewportConstraints>(constraints);

            auto layerPosition = fixedConstraints.viewportRelativeLayerPosition(positionedObjectsRect);
            
            CGRect layerBounds = [layer bounds];
            CGPoint anchorPoint = [layer anchorPoint];
            CGPoint newPosition = CGPointMake(layerPosition.x() - constraints.alignmentOffset().width() + anchorPoint.x * layerBounds.size.width,
                layerPosition.y() - constraints.alignmentOffset().height() + anchorPoint.y * layerBounds.size.height);
            [layer setPosition:newPosition];
            break;
        }
        case ViewportConstraints::StickyPositionConstraint: {
            auto& stickyConstraints = downcast<StickyPositionViewportConstraints>(constraints);

            FloatPoint layerPosition = stickyConstraints.anchorLayerPositionForConstrainingRect(positionedObjectsRect);

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
    Locker locker { webFixedPositionContentDataLock };

    LayerInfoMap::const_iterator end = _private->m_viewportConstrainedLayers.end();
    for (LayerInfoMap::const_iterator it = _private->m_viewportConstrainedLayers.begin(); it != end; ++it) {
        CALayer *layer = it->key.get();
        ViewportConstrainedLayerData* constraintData = it->value.get();
        
        if (constraintData->m_enclosingAcceleratedScrollLayer == scrollLayer) {
            const StickyPositionViewportConstraints& stickyConstraints = static_cast<const StickyPositionViewportConstraints&>(*(constraintData->m_viewportConstraints.get()));
            FloatRect constrainingRectAtLastLayout = stickyConstraints.constrainingRectAtLastLayout();
            FloatRect scrolledConstrainingRect = FloatRect(scrollPosition.x, scrollPosition.y, constrainingRectAtLastLayout.width(), constrainingRectAtLastLayout.height());
            FloatPoint layerPosition = stickyConstraints.anchorLayerPositionForConstrainingRect(scrolledConstrainingRect);

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
        if (auto* frame = [_private->m_webView _mainCoreFrame])
            frame->viewportOffsetChanged(LocalFrame::CompletedScrollOffset);
    });
}

- (void)setViewportConstrainedLayers:(WTF::HashMap<CALayer *, std::unique_ptr<WebCore::ViewportConstraints>>&)layerMap stickyContainerMap:(const WTF::HashMap<CALayer*, CALayer*>&)stickyContainers
{
    Locker locker { webFixedPositionContentDataLock };

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
    Locker locker { webFixedPositionContentDataLock };
    return !_private->m_viewportConstrainedLayers.isEmpty();
}

@end

#endif // PLATFORM(IOS_FAMILY)
