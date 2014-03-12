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

#if PLATFORM(IOS)

#import "WebFixedPositionContent.h"
#import "WebFixedPositionContentInternal.h"

#import "WebViewInternal.h"
#import <WebCore/ChromeClient.h>
#import <WebCore/Frame.h>
#import <WebCore/IntSize.h>
#import <WebCore/ScrollingConstraints.h>
#import <WebCore/WebCoreThreadRun.h>

#import <wtf/HashMap.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/Threading.h>

#import <CoreGraphics/CGFloat.h>
#import <QuartzCore/QuartzCore.h>
#import <Foundation/Foundation.h>
#import <algorithm>

using namespace WebCore;
using namespace std;

static Mutex& WebFixedPositionContentDataLock()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

struct ViewportConstrainedLayerData {
    ViewportConstrainedLayerData()
        : m_enclosingAcceleratedScrollLayer(nil)
    { }
    CALayer* m_enclosingAcceleratedScrollLayer; // May be nil.
    OwnPtr<ViewportConstraints> m_viewportConstraints;
};

typedef HashMap<RetainPtr<CALayer>, OwnPtr<ViewportConstrainedLayerData> > LayerInfoMap;

struct WebFixedPositionContentData {
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

@implementation WebFixedPositionContent

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
    MutexLocker lock(WebFixedPositionContentDataLock());

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
    MutexLocker lock(WebFixedPositionContentDataLock());

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

- (void)setViewportConstrainedLayers:(WTF::HashMap<CALayer *, OwnPtr<WebCore::ViewportConstraints> >&)layerMap stickyContainerMap:(WTF::HashMap<CALayer*, CALayer*>&)stickyContainers
{
    MutexLocker lock(WebFixedPositionContentDataLock());

    _private->m_viewportConstrainedLayers.clear();

    HashMap<CALayer *, OwnPtr<ViewportConstraints> >::iterator end = layerMap.end();
    for (HashMap<CALayer *, OwnPtr<ViewportConstraints> >::iterator it = layerMap.begin(); it != end; ++it) {
        CALayer* layer = it->key;
        OwnPtr<ViewportConstrainedLayerData> layerData = adoptPtr(new ViewportConstrainedLayerData);

        layerData->m_enclosingAcceleratedScrollLayer = stickyContainers.get(layer);
        layerData->m_viewportConstraints = it->value.release();

        _private->m_viewportConstrainedLayers.set(layer, layerData.release());
    }
}

- (BOOL)hasFixedOrStickyPositionLayers
{
    MutexLocker lock(WebFixedPositionContentDataLock());
    return !_private->m_viewportConstrainedLayers.isEmpty();
}

static ViewportConstraints::AnchorEdgeFlags anchorEdgeFlagsForAnchorEdge(WebFixedPositionAnchorEdge edge)
{
    switch (edge) {
    case WebFixedPositionAnchorEdgeLeft:
        return ViewportConstraints::AnchorEdgeFlags::AnchorEdgeLeft;
    case WebFixedPositionAnchorEdgeRight:
        return ViewportConstraints::AnchorEdgeFlags::AnchorEdgeRight;
    case WebFixedPositionAnchorEdgeTop:
        return ViewportConstraints::AnchorEdgeFlags::AnchorEdgeTop;
    case WebFixedPositionAnchorEdgeBottom:
        return ViewportConstraints::AnchorEdgeFlags::AnchorEdgeBottom;
    }
}

- (CGFloat)minimumOffsetFromFixedPositionLayersToAnchorEdge:(WebFixedPositionAnchorEdge)anchorEdge ofRect:(CGRect)rect inLayer:(CALayer *)layer
{
    MutexLocker lock(WebFixedPositionContentDataLock());
    ViewportConstraints::AnchorEdgeFlags anchorEdgeFlags = anchorEdgeFlagsForAnchorEdge(anchorEdge);
    CGFloat minimumOffset = CGFLOAT_MAX;
    LayerInfoMap::const_iterator end = _private->m_viewportConstrainedLayers.end();
    for (LayerInfoMap::const_iterator it = _private->m_viewportConstrainedLayers.begin(); it != end; ++it) {
        CALayer *fixedLayer = it->key.get();
        ViewportConstrainedLayerData* constraintData = it->value.get();
        const ViewportConstraints& constraints = *(constraintData->m_viewportConstraints.get());

        if (!constraints.hasAnchorEdge(anchorEdgeFlags))
            continue;
        // According to Simon: It's possible that there are windows of time
        // where these CALayers are unparented (because we've flushed on the web
        // thread but haven't updated those layers yet).
        if (![fixedLayer superlayer])
            continue;

        CGRect fixedLayerExtent = [layer convertRect:[fixedLayer bounds] fromLayer:fixedLayer];
        CGFloat offset;
        switch (anchorEdge) {
        case WebFixedPositionAnchorEdgeLeft:
            offset = CGRectGetMinX(fixedLayerExtent) - CGRectGetMinX(rect);
            break;
        case WebFixedPositionAnchorEdgeRight:
            offset = CGRectGetMaxX(rect) - CGRectGetMaxX(fixedLayerExtent);
            break;
        case WebFixedPositionAnchorEdgeTop:
            offset = CGRectGetMinY(fixedLayerExtent) - CGRectGetMinY(rect);
            break;
        case WebFixedPositionAnchorEdgeBottom:
            offset = CGRectGetMaxY(rect) - CGRectGetMaxY(fixedLayerExtent);
            break;
        }
        minimumOffset = CGFloatMin(minimumOffset, offset);
    }
    return minimumOffset;
}

@end

#endif // PLATFORM(IOS)
