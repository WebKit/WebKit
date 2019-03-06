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

#import "RemoteLayerTreeHost.h"
#import "UIKitSPI.h"
#import "WKDrawingView.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/SoftLinking.h>

static void collectDescendantViewsAtPoint(Vector<UIView *, 16>& viewsAtPoint, UIView *parent, CGPoint point, UIEvent *event)
{
    if (parent.clipsToBounds && ![parent pointInside:point withEvent:event])
        return;

    for (UIView *view in [parent subviews]) {
        CGPoint subviewPoint = [view convertPoint:point fromView:parent];

        // FIXME: This doesn't cover all possible cases yet.
        auto isTransparent = [&] {
            if ([view isKindOfClass:[WKTiledBackingView class]])
                return false;
            if (![view isKindOfClass:[WKCompositingView class]])
                return false;
            if (view.layer.contents)
                return false;
            return true;
        }();

        if (!isTransparent && [view pointInside:subviewPoint withEvent:event])
            viewsAtPoint.append(view);

        if (![view subviews])
            return;

        collectDescendantViewsAtPoint(viewsAtPoint, view, subviewPoint, event);
    };
}

@interface UIView (WKHitTesting)
- (UIView *)_web_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event;
@end

@implementation UIView (WKHitTesting)

- (UIView *)_web_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event
{
    Vector<UIView *, 16> viewsAtPoint;
    collectDescendantViewsAtPoint(viewsAtPoint, self, point, event);

    for (auto i = viewsAtPoint.size(); i--;) {
        auto *view = viewsAtPoint[i];
        if (!view.isUserInteractionEnabled)
            continue;

        if ([view conformsToProtocol:@protocol(WKNativelyInteractible)]) {
            CGPoint subviewPoint = [view convertPoint:point fromView:self];
            return [view hitTest:subviewPoint withEvent:event];
        }

        if ([view isKindOfClass:[WKChildScrollView class]]) {
            // See if the deepest view hit is actually a child of the scrollview.
            if ([viewsAtPoint.last() isDescendantOfView:view])
                return view;
        }
    }
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

@implementation WKTiledBackingView
@end

@implementation WKTransformView

+ (Class)layerClass
{
    return [CATransformLayer self];
}

@end

@implementation WKSimpleBackdropView

+ (Class)layerClass
{
    return [CABackdropLayer self];
}

@end

@implementation WKShapeView

+ (Class)layerClass
{
    return [CAShapeLayer self];
}

@end

@implementation WKRemoteView

- (instancetype)initWithFrame:(CGRect)frame contextID:(uint32_t)contextID
{
    if ((self = [super initWithFrame:frame])) {
        CALayerHost *layer = (CALayerHost *)self.layer;
        layer.contextId = contextID;
#if PLATFORM(IOSMAC)
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

#if USE(UIREMOTEVIEW_CONTEXT_HOSTING)
@implementation WKUIRemoteView

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _web_findDescendantViewAtPoint:point withEvent:event];
}

- (NSString *)description
{
    return WebKit::RemoteLayerTreeNode::appendLayerDescription(super.description, self.layer);
}

@end
#endif

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

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
    self.contentInsetAdjustmentBehavior = UIScrollViewContentInsetAdjustmentNever;
#endif

    return self;
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
