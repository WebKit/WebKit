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

#if PLATFORM(IOS)

#import "RemoteLayerTreeHost.h"

#import <WebCore/WebCoreCALayerExtras.h>
#import <WebKitSystemInterface.h>

#import <UIKit/UIScrollView.h>
#import <QuartzCore/QuartzCore.h>

using namespace WebCore;

@interface CALayer(WKLayerInternal)
- (void)setContextId:(uint32_t)contextID;
@end

@interface UIView (WKHitTesting)
- (UIView *)_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event;
@end

@implementation UIView (WKHitTesting)

// UIView hit testing assumes that views should only hit test subviews that are entirely contained
// in the view. This is not true of web content.
- (UIView *)_recursiveFindDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event
{
    if (self.clipsToBounds && ![self pointInside:point withEvent:event])
        return nil;

    __block UIView *foundView = nil;
    [[self subviews] enumerateObjectsUsingBlock:^(UIView *view, NSUInteger idx, BOOL *stop) {
        CGPoint subviewPoint = [view convertPoint:point fromView:self];

        if ([view pointInside:subviewPoint withEvent:event])
            foundView = view;

        if (![view subviews])
            return;

        if (UIView *hitView = [view _recursiveFindDescendantViewAtPoint:subviewPoint withEvent:event])
            foundView = hitView;
    }];

    return foundView;
}

- (UIView *)_findDescendantViewAtPoint:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _recursiveFindDescendantViewAtPoint:point withEvent:event];
}

@end

@interface WKCompositingView : UIView
@end

@implementation WKCompositingView
- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _findDescendantViewAtPoint:point withEvent:event];
}
@end

@interface WKTransformView : UIView
@end

@implementation WKTransformView
+ (Class)layerClass
{
    return [CATransformLayer self];
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _findDescendantViewAtPoint:point withEvent:event];
}
@end

@interface WKRemoteView : UIView
@end

@implementation WKRemoteView
- (instancetype)initWithFrame:(CGRect)frame contextID:(uint32_t)contextID
{
    if ((self = [super initWithFrame:frame])) {
        [[self layer] setContextId:contextID];
    }
    
    return self;
}

+ (Class)layerClass
{
    return NSClassFromString(@"CALayerHost");
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    return [self _findDescendantViewAtPoint:point withEvent:event];
}

@end

namespace WebKit {

LayerOrView *RemoteLayerTreeHost::createLayer(const RemoteLayerTreeTransaction::LayerCreationProperties& properties, const RemoteLayerTreeTransaction::LayerProperties* layerProperties)
{
    RetainPtr<LayerOrView>& layerOrView = m_layers.add(properties.layerID, nullptr).iterator->value;

    ASSERT(!layerOrView);

    switch (properties.type) {
    case PlatformCALayer::LayerTypeLayer:
    case PlatformCALayer::LayerTypeWebLayer:
    case PlatformCALayer::LayerTypeRootLayer:
    case PlatformCALayer::LayerTypeSimpleLayer:
    case PlatformCALayer::LayerTypeTiledBackingLayer:
    case PlatformCALayer::LayerTypePageTiledBackingLayer:
    case PlatformCALayer::LayerTypeTiledBackingTileLayer:
        if (layerProperties && layerProperties->customBehavior == GraphicsLayer::CustomScrollingBehavior)
            layerOrView = adoptNS([[UIScrollView alloc] init]);
        else
            layerOrView = adoptNS([[WKCompositingView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeTransformLayer:
        layerOrView = adoptNS([[WKTransformView alloc] init]);
        break;
    case PlatformCALayer::LayerTypeCustom:
        if (!m_isDebugLayerTreeHost)
            layerOrView = adoptNS([[WKRemoteView alloc] initWithFrame:CGRectZero contextID:properties.hostingContextID]);
        else
            layerOrView = adoptNS([[WKCompositingView alloc] init]);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    // FIXME: Do through the view.
    [[layerOrView layer] web_disableAllActions];
    setLayerID([layerOrView layer], properties.layerID);

    return layerOrView.get();
}

} // namespace WebKit

#endif // PLATFORM(IOS)
