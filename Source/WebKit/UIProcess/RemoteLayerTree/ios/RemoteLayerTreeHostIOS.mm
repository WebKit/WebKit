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
#import "RemoteLayerTreeHost.h"

#if PLATFORM(IOS_FAMILY)

#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "UIKitSPI.h"
#import "WebPageProxy.h"
#import <UIKit/UIScrollView.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

using namespace WebCore;

@interface UIView (WKHitTesting)
- (UIView *)_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event;
@end

@implementation UIView (WKHitTesting)

// UIView hit testing assumes that views should only hit test subviews that are entirely contained
// in the view. This is not true of web content.
// We only want to find UIScrollViews here. Other views are ignored.
- (UIView *)_recursiveFindDescendantScrollViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event
{
    if (self.clipsToBounds && ![self pointInside:point withEvent:event])
        return nil;

    __block UIView *foundView = nil;
    [[self subviews] enumerateObjectsUsingBlock:^(UIView *view, NSUInteger idx, BOOL *stop) {
        CGPoint subviewPoint = [view convertPoint:point fromView:self];

        if ([view pointInside:subviewPoint withEvent:event] && [view isKindOfClass:[UIScrollView class]] && view.isUserInteractionEnabled)
            foundView = view;

        if (![view subviews])
            return;

        if (UIView *hitView = [view _recursiveFindDescendantScrollViewAtPoint:subviewPoint withEvent:event])
            foundView = hitView;
    }];

    return foundView;
}

- (UIView *)_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _recursiveFindDescendantScrollViewAtPoint:point withEvent:event];
}

@end

@interface WKCompositingView : UIView
@end

@implementation WKCompositingView

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _findDescendantViewAtPoint:point withEvent:event];
}

- (NSString *)description
{
    NSString *viewDescription = [super description];
    NSString *webKitDetails = [NSString stringWithFormat:@" layerID = %llu \"%@\"", WebKit::RemoteLayerTreeHost::layerID(self.layer), self.layer.name ? self.layer.name : @""];
    return [viewDescription stringByAppendingString:webKitDetails];
}

@end

@interface WKTransformView : WKCompositingView
@end

@implementation WKTransformView

+ (Class)layerClass
{
    return [CATransformLayer self];
}

@end

@interface WKSimpleBackdropView : WKCompositingView
@end

@implementation WKSimpleBackdropView

+ (Class)layerClass
{
    return [CABackdropLayer self];
}

@end

@interface WKShapeView : WKCompositingView
@end

@implementation WKShapeView

+ (Class)layerClass
{
    return [CAShapeLayer self];
}

@end

@interface WKRemoteView : WKCompositingView
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
@interface WKUIRemoteView : _UIRemoteView
@end

@implementation WKUIRemoteView

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _findDescendantViewAtPoint:point withEvent:event];
}

- (NSString *)description
{
    NSString *viewDescription = [super description];
    NSString *webKitDetails = [NSString stringWithFormat:@" layerID = %llu \"%@\"", WebKit::RemoteLayerTreeHost::layerID(self.layer), self.layer.name ? self.layer.name : @""];
    return [viewDescription stringByAppendingString:webKitDetails];
}

@end
#endif

static RetainPtr<UIView> createRemoteView(pid_t pid, uint32_t contextID)
{
#if USE(UIREMOTEVIEW_CONTEXT_HOSTING)
    // FIXME: Remove this respondsToSelector check when possible.
    static BOOL canUseUIRemoteView;
    static std::once_flag initializeCanUseUIRemoteView;
    std::call_once(initializeCanUseUIRemoteView, [] {
        canUseUIRemoteView = [_UIRemoteView instancesRespondToSelector:@selector(initWithFrame:pid:contextID:)];
    });
    if (canUseUIRemoteView)
        return adoptNS([[WKUIRemoteView alloc] initWithFrame:CGRectZero pid:pid contextID:contextID]);
#else
    UNUSED_PARAM(pid);
#endif
    return adoptNS([[WKRemoteView alloc] initWithFrame:CGRectZero contextID:contextID]);
}

@interface WKBackdropView : _UIBackdropView
@end

@implementation WKBackdropView

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _findDescendantViewAtPoint:point withEvent:event];
}

- (NSString *)description
{
    NSString *viewDescription = [super description];
    NSString *webKitDetails = [NSString stringWithFormat:@" layerID = %llu \"%@\"", WebKit::RemoteLayerTreeHost::layerID(self.layer), self.layer.name ? self.layer.name : @""];
    return [viewDescription stringByAppendingString:webKitDetails];
}

@end

namespace WebKit {

LayerOrView *RemoteLayerTreeHost::createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeTransaction::LayerProperties* layerProperties)
{
    RetainPtr<LayerOrView>& view = m_layers.add(properties.layerID, nullptr).iterator->value;

    ASSERT(!view);

    switch (properties.type) {
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        view = adoptNS([[WKCompositingView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeBackdropLayer:
        view = adoptNS([[WKSimpleBackdropView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeLightSystemBackdropLayer:
        view = adoptNS([[WKBackdropView alloc] initWithFrame:CGRectZero privateStyle:_UIBackdropViewStyle_Light]);
        break;
    case PlatformCALayer::LayerTypeDarkSystemBackdropLayer:
        view = adoptNS([[WKBackdropView alloc] initWithFrame:CGRectZero privateStyle:_UIBackdropViewStyle_Dark]);
        break;
    case PlatformCALayer::LayerTypeTransformLayer:
        view = adoptNS([[WKTransformView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeCustom:
    case PlatformCALayer::LayerTypeAVPlayerLayer:
    case PlatformCALayer::LayerTypeContentsProvidedLayer:
        if (!m_isDebugLayerTreeHost) {
            view = createRemoteView(m_drawingArea->page().processIdentifier(), properties.hostingContextID);
            if (properties.type == PlatformCALayer::LayerTypeAVPlayerLayer) {
                // Invert the scale transform added in the WebProcess to fix <rdar://problem/18316542>.
                float inverseScale = 1 / properties.hostingDeviceScaleFactor;
                [[view layer] setTransform:CATransform3DMakeScale(inverseScale, inverseScale, 1)];
            }
        } else
            view = adoptNS([[WKCompositingView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeShapeLayer:
        view = adoptNS([[WKShapeView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeScrollingLayer:
        if (!m_isDebugLayerTreeHost) {
            auto scrollView = adoptNS([[UIScrollView alloc] init]);
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000
            [scrollView setContentInsetAdjustmentBehavior:UIScrollViewContentInsetAdjustmentNever];
#endif
            view = scrollView;
        } else // The debug indicator parents views under layers, which can cause crashes with UIScrollView.
            view = adoptNS([[UIView alloc] init]);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    setLayerID([view layer], properties.layerID);

    return view.get();
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
