/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeViews.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeNode.h"
#import "UIKitSPI.h"
#import "WKDeferringGestureRecognizer.h"
#import "WKDrawingView.h"
#import <WebCore/Region.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SoftLinking.h>

namespace WebKit {

static void collectDescendantViewsAtPoint(Vector<UIView *, 16>& viewsAtPoint, UIView *parent, CGPoint point, UIEvent *event)
{
    if (parent.clipsToBounds && ![parent pointInside:point withEvent:event])
        return;

    for (UIView *view in [parent subviews]) {
        CGPoint subviewPoint = [view convertPoint:point fromView:parent];

        auto handlesEvent = [&] {
            // FIXME: isUserInteractionEnabled is mostly redundant with event regions for web content layers.
            //        It is currently only needed for scroll views.
            if (!view.isUserInteractionEnabled)
                return false;

            if (CGRectIsEmpty([view frame]))
                return false;

            if (![view pointInside:subviewPoint withEvent:event])
                return false;

            if (![view isKindOfClass:[WKCompositingView class]])
                return true;
            auto* node = RemoteLayerTreeNode::forCALayer(view.layer);
            return node->eventRegion().contains(WebCore::IntPoint(subviewPoint));
        }();

        if (handlesEvent)
            viewsAtPoint.append(view);

        if (![view subviews])
            return;

        collectDescendantViewsAtPoint(viewsAtPoint, view, subviewPoint, event);
    };
}

#if ENABLE(EDITABLE_REGION)

static void collectDescendantViewsInRect(Vector<UIView *, 16>& viewsInRect, UIView *parent, CGRect rect)
{
    if (parent.clipsToBounds && !CGRectIntersectsRect(parent.bounds, rect))
        return;

    for (UIView *view in parent.subviews) {
        CGRect subviewRect = [view convertRect:rect fromView:parent];

        auto intersectsRect = [&] {
            // FIXME: isUserInteractionEnabled is mostly redundant with event regions for web content layers.
            //        It is currently only needed for scroll views.
            if (!view.isUserInteractionEnabled)
                return false;

            if (CGRectIsEmpty(view.frame))
                return false;

            if (!CGRectIntersectsRect(subviewRect, view.bounds))
                return false;

            if (![view isKindOfClass:WKCompositingView.class])
                return true;
            auto* node = RemoteLayerTreeNode::forCALayer(view.layer);
            return node->eventRegion().intersects(WebCore::IntRect { subviewRect });
        }();

        if (intersectsRect)
            viewsInRect.append(view);

        if (!view.subviews)
            return;

        collectDescendantViewsInRect(viewsInRect, view, subviewRect);
    };
}

bool mayContainEditableElementsInRect(UIView *rootView, const WebCore::FloatRect& rect)
{
    Vector<UIView *, 16> viewsInRect;
    collectDescendantViewsInRect(viewsInRect, rootView, rect);
    if (viewsInRect.isEmpty())
        return false;
    bool possiblyHasEditableElements = true;
    for (auto *view : WTF::makeReversedRange(viewsInRect)) {
        if (![view isKindOfClass:WKCompositingView.class])
            continue;
        auto* node = RemoteLayerTreeNode::forCALayer(view.layer);
        if (!node)
            continue;
        WebCore::IntRect rectToTest { [view convertRect:rect fromView:rootView] };
        if (node->eventRegion().containsEditableElementsInRect(rectToTest))
            return true;
        bool hasEditableRegion = node->eventRegion().hasEditableRegion();
        if (hasEditableRegion && node->eventRegion().contains(rectToTest))
            return false;
        if (hasEditableRegion)
            possiblyHasEditableElements = false;
    }
    return possiblyHasEditableElements;
}

#endif // ENABLE(EDITABLE_REGION)

static bool isScrolledBy(WKChildScrollView* scrollView, UIView *hitView)
{
    auto scrollLayerID = RemoteLayerTreeNode::layerID(scrollView.layer);

    for (UIView *view = hitView; view; view = [view superview]) {
        if (view == scrollView)
            return true;

        auto* node = RemoteLayerTreeNode::forCALayer(view.layer);
        if (node && scrollLayerID) {
            if (node->actingScrollContainerID() == scrollLayerID)
                return true;
            if (node->stationaryScrollContainerIDs().contains(scrollLayerID))
                return false;
        }
    }

    return false;
}

OptionSet<WebCore::TouchAction> touchActionsForPoint(UIView *rootView, const WebCore::IntPoint& point)
{
    Vector<UIView *, 16> viewsAtPoint;
    collectDescendantViewsAtPoint(viewsAtPoint, rootView, point, nil);

    if (viewsAtPoint.isEmpty())
        return { WebCore::TouchAction::Auto };

    UIView *hitView = nil;
    for (auto *view : WTF::makeReversedRange(viewsAtPoint)) {
        // We only hit WKChildScrollView directly if its content layer doesn't have an event region.
        // We don't generate the region if there is nothing interesting in it, meaning the touch-action is auto.
        if ([view isKindOfClass:[WKChildScrollView class]])
            return WebCore::TouchAction::Auto;

        if ([view isKindOfClass:[WKCompositingView class]]) {
            hitView = view;
            break;
        }
    }

    if (!hitView)
        return { WebCore::TouchAction::Auto };

    CGPoint hitViewPoint = [hitView convertPoint:point fromView:rootView];

    auto* node = RemoteLayerTreeNode::forCALayer(hitView.layer);
    if (!node)
        return { WebCore::TouchAction::Auto };

    return node->eventRegion().touchActionsForPoint(WebCore::IntPoint(hitViewPoint));
}

UIScrollView *findActingScrollParent(UIScrollView *scrollView, const RemoteLayerTreeHost& host)
{
    HashSet<WebCore::GraphicsLayer::PlatformLayerID> scrollersToSkip;

    for (UIView *view = [scrollView superview]; view; view = [view superview]) {
        if ([view isKindOfClass:[WKChildScrollView class]] && !scrollersToSkip.contains(RemoteLayerTreeNode::layerID(view.layer))) {
            // FIXME: Ideally we would return the scroller we want in all cases but the current UIKit SPI only allows returning a non-ancestor.
            return nil;
        }
        if (auto* node = RemoteLayerTreeNode::forCALayer(view.layer)) {
            if (auto* actingParent = host.nodeForID(node->actingScrollContainerID())) {
                if ([actingParent->uiView() isKindOfClass:[UIScrollView class]])
                    return (UIScrollView *)actingParent->uiView();
            }

            scrollersToSkip.add(node->stationaryScrollContainerIDs().begin(), node->stationaryScrollContainerIDs().end());
        }
    }
    return nil;
}

static Class scrollViewScrollIndicatorClass()
{
    static dispatch_once_t onceToken;
    static Class scrollIndicatorClass;
    dispatch_once(&onceToken, ^{
        scrollIndicatorClass = NSClassFromString(@"_UIScrollViewScrollIndicator");
    });
    return scrollIndicatorClass;
}

}

@interface UIView (WKHitTesting)
- (UIView *)_web_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event;
@end

@implementation UIView (WKHitTesting)

- (UIView *)_web_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event
{
    Vector<UIView *, 16> viewsAtPoint;
    WebKit::collectDescendantViewsAtPoint(viewsAtPoint, self, point, event);

    LOG_WITH_STREAM(UIHitTesting, stream << (void*)self << "_web_findDescendantViewAtPoint " << WebCore::FloatPoint(point) << " found " << viewsAtPoint.size() << " views");

    for (auto *view : WTF::makeReversedRange(viewsAtPoint)) {
        if ([view conformsToProtocol:@protocol(WKNativelyInteractible)]) {
            LOG_WITH_STREAM(UIHitTesting, stream << " " << (void*)view << " is natively interactible");
            CGPoint subviewPoint = [view convertPoint:point fromView:self];
            return [view hitTest:subviewPoint withEvent:event];
        }

        if ([view isKindOfClass:[WKChildScrollView class]]) {
            if (WebKit::isScrolledBy((WKChildScrollView *)view, viewsAtPoint.last())) {
                LOG_WITH_STREAM(UIHitTesting, stream << " " << (void*)view << " is child scroll view and scrolled by " << (void*)viewsAtPoint.last());
                return view;
            }
        }

        if ([view isKindOfClass:WebKit::scrollViewScrollIndicatorClass()] && [view.superview isKindOfClass:WKChildScrollView.class]) {
            if (WebKit::isScrolledBy((WKChildScrollView *)view.superview, viewsAtPoint.last())) {
                LOG_WITH_STREAM(UIHitTesting, stream << " " << (void*)view << " is the scroll indicator of child scroll view, which is scrolled by " << (void*)viewsAtPoint.last());
                return view;
            }
        }

        LOG_WITH_STREAM(UIHitTesting, stream << " ignoring " << [view class] << " " << (void*)view);
    }

    LOG_WITH_STREAM(UIHitTesting, stream << (void*)self << "_web_findDescendantViewAtPoint found no interactive views");
    return nil;
}

@end

@implementation WKCompositingView

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _web_findDescendantViewAtPoint:point withEvent:event];
}

- (NSString *)description
{
    return WebKit::RemoteLayerTreeNode::appendLayerDescription(super.description, self.layer);
}

@end

@implementation WKTransformView

+ (Class)layerClass
{
    return [CATransformLayer class];
}

@end

@implementation WKSimpleBackdropView

+ (Class)layerClass
{
    return [CABackdropLayer class];
}

@end

@implementation WKShapeView

+ (Class)layerClass
{
    return [CAShapeLayer class];
}

@end

@implementation WKRemoteView

- (instancetype)initWithFrame:(CGRect)frame contextID:(uint32_t)contextID
{
    if ((self = [super initWithFrame:frame])) {
        CALayerHost *layer = (CALayerHost *)self.layer;
        layer.contextId = contextID;
#if PLATFORM(MACCATALYST)
        // When running iOS apps on macOS, kCAContextIgnoresHitTest isn't respected; instead, we avoid
        // hit-testing to the remote context by disabling hit-testing on its host layer. See
        // <rdar://problem/40591107> for more details.
        layer.allowsHitTesting = NO;
#endif
    }

    return self;
}

+ (Class)layerClass
{
    return NSClassFromString(@"CALayerHost");
}

@end

@implementation WKUIRemoteView

- (instancetype)initWithFrame:(CGRect)frame pid:(pid_t)pid contextID:(uint32_t)contextID
{
    self = [super initWithFrame:frame pid:pid contextID:contextID];
    if (!self)
        return nil;

#if PLATFORM(MACCATALYST)
    // When running iOS apps on macOS, kCAContextIgnoresHitTest isn't respected; instead, we avoid
    // hit-testing to the remote context by disabling hit-testing on its host layer. See
    // <rdar://problem/40591107> for more details.
    self.layerHost.allowsHitTesting = NO;
#endif

    return self;
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _web_findDescendantViewAtPoint:point withEvent:event];
}

- (NSString *)description
{
    return WebKit::RemoteLayerTreeNode::appendLayerDescription(super.description, self.layer);
}

@end

@implementation WKBackdropView

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _web_findDescendantViewAtPoint:point withEvent:event];
}

- (NSString *)description
{
    return WebKit::RemoteLayerTreeNode::appendLayerDescription(super.description, self.layer);
}

@end

@implementation WKChildScrollView

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

// FIXME: Likely we can remove this special case for watchOS and tvOS.
#if !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    self.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
#endif

    return self;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRequireFailureOfGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([otherGestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)otherGestureRecognizer shouldDeferGestureRecognizer:gestureRecognizer];

    return NO;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldBeRequiredToFailByGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    if ([gestureRecognizer isKindOfClass:WKDeferringGestureRecognizer.class])
        return [(WKDeferringGestureRecognizer *)gestureRecognizer shouldDeferGestureRecognizer:otherGestureRecognizer];

    return NO;
}

@end

@implementation WKEmbeddedView

- (instancetype)initWithEmbeddedViewID:(WebCore::GraphicsLayer::EmbeddedViewID)embeddedViewID
{
    self = [super init];
    if (!self)
        return nil;

    _embeddedViewID = embeddedViewID;

    return self;
}

@end

#endif // PLATFORM(IOS_FAMILY)
