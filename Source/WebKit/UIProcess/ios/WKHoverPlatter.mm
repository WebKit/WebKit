/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "WKHoverPlatter.h"

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT)

#import "WKHoverPlatterParameters.h"
#import <WebCore/PathUtilities.h>
#import <WebCore/WebCoreCALayerExtras.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

static RetainPtr<CABasicAnimation> createBaseAnimation()
{
    auto parameters = WKHoverPlatterDomain.rootSettings;
    if (parameters.useSpring) {
        auto spring = adoptNS([[CASpringAnimation alloc] init]);
        [spring setMass:parameters.springMass];
        [spring setStiffness:parameters.springStiffness];
        [spring setDamping:parameters.springDamping];
        [spring setFillMode:kCAFillModeBackwards];
        [spring setDuration:[spring settlingDuration]];
        return spring;
    }

    auto ease = adoptNS([[CABasicAnimation alloc] init]);
    [ease setFillMode:kCAFillModeBackwards];
    [ease setDuration:parameters.duration];
    [ease setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn]];
    return ease;
}

static void addAnimation(CALayer *layer, NSString *key, id fromValue, id toValue)
{
    auto animation = createBaseAnimation();
    [animation setKeyPath:key];
    [animation setFromValue:fromValue];
    [animation setToValue:toValue];
    [layer setValue:toValue forKey:key];
    [layer addAnimation:animation.get() forKey:key];
};

@implementation WKHoverPlatter {
    __weak UIView *_view;
    __weak id <WKHoverPlatterDelegate> _delegate;
    WebCore::FloatPoint _hoverPoint;

    BOOL _isSittingDown;

    RetainPtr<CALayer> _platterLayer;
    RetainPtr<CAPortalLayer> _snapshotLayer;

    RetainPtr<CALayer> _debugIndicatorLayer;
    RetainPtr<CATextLayer> _linkCountLayer;
    RetainPtr<CAShapeLayer> _linkHighlightLayer;
}

- (instancetype)initWithView:(UIView *)view delegate:(id <WKHoverPlatterDelegate>)delegate
{
    auto parameters = WKHoverPlatterDomain.rootSettings;
    if (!parameters.platterEnabledForMouse && !parameters.platterEnabledForLongPress) {
        [self release];
        return nil;
    }

    self = [super init];
    if (!self)
        return nil;

    _view = view;
    _delegate = delegate;

    return self;
}

- (void)invalidate
{
    [self dismissPlatterWithAnimation:NO];
    _view = nil;
    _delegate = nil;
}

- (WebCore::FloatPoint)hoverPoint
{
    return _hoverPoint;
}

- (void)didReceiveMouseEvent:(const WebKit::NativeWebMouseEvent&)event
{
    auto point = event.position();

    if (WKHoverPlatterDomain.rootSettings.platterEnabledForMouse) {
        _hoverPoint = point;
        [self update];
        return;
    }

    auto boundingRect = self.platterBoundingRect;
    if (!boundingRect.contains(point) && point != WebCore::FloatPoint { -1, -1 })
        [self dismissPlatterWithAnimation:YES];
}

- (void)didLongPressAtPoint:(WebCore::FloatPoint)point
{
    if (!WKHoverPlatterDomain.rootSettings.platterEnabledForLongPress)
        return;

    _hoverPoint = point;
    [self update];
}

- (WebCore::FloatRect)platterBoundingRect
{
    auto radius = WKHoverPlatterDomain.rootSettings.platterRadius;
    return { _hoverPoint - WebCore::FloatSize(radius, radius), WebCore::FloatSize(radius * 2, radius * 2) };
}

- (WebCore::FloatRect)linkSearchRect
{
    auto radius = WKHoverPlatterDomain.rootSettings.linkSearchRadius;
    return { _hoverPoint - WebCore::FloatSize(radius, radius), WebCore::FloatSize(radius * 2, radius * 2) };
}

- (void)update
{
    auto parameters = WKHoverPlatterDomain.rootSettings;

    if (parameters.showDebugOverlay)
        [self updateDebugIndicator];

    auto boundingRect = self.platterBoundingRect;
    RetainPtr<CALayer> platter = _platterLayer ?: adoptNS([[CALayer alloc] init]);
    [platter setBounds:CGRectMake(0, 0, boundingRect.width(), boundingRect.height())];
    [platter setPosition:boundingRect.center()];
    [platter setShadowColor:UIColor.blackColor.CGColor];
    [platter setShadowOffset:CGSizeMake(0, 10)];
    [platter setShadowOpacity:0.25];
    [platter setShadowRadius:10];
    [platter web_disableAllActions];
    [[[_view superview] layer] addSublayer:platter.get()];

    RetainPtr<CAPortalLayer> snapshot = _snapshotLayer ?: adoptNS([[CAPortalLayer alloc] init]);
    [snapshot setSourceLayer:[_view layer]];
    [snapshot setMatchesPosition:YES];
    [snapshot setMatchesTransform:YES];
    [snapshot setMasksToBounds:YES];
    [snapshot setCornerRadius:parameters.platterRadius];
    [snapshot setBackgroundColor:UIColor.blackColor.CGColor];
    [snapshot setBorderColor:[UIColor colorWithWhite:0 alpha:0.2].CGColor];
    [snapshot setBorderWidth:1];
    [snapshot setAnchorPoint:CGPointMake(0.5, 0.5)];
    [snapshot setSublayerTransform:CATransform3DMakeScale(parameters.platterScale, parameters.platterScale, 1)];
    [snapshot setMasksToBounds:YES];
    [snapshot setFrame:CGRectMake(0, 0, boundingRect.width(), boundingRect.height())];
    [snapshot web_disableAllActions];
    [platter addSublayer:snapshot.get()];

    if (_isSittingDown) {
        if (parameters.animateScale) {
            auto inverseScale = 1.f / parameters.platterScale;
            addAnimation(platter.get(), @"transform", [NSValue valueWithCATransform3D:CATransform3DMakeScale(inverseScale, inverseScale, 1)], [NSValue valueWithCATransform3D:CATransform3DIdentity]);
            addAnimation(snapshot.get(), @"sublayerTransform", [NSValue valueWithCATransform3D:CATransform3DIdentity], [NSValue valueWithCATransform3D:[snapshot sublayerTransform]]);
            addAnimation(platter.get(), @"shadowOpacity", @0, @([platter shadowOpacity]));
            addAnimation(snapshot.get(), @"borderColor", (id)UIColor.clearColor.CGColor, (id)[snapshot borderColor]);
        } else
            addAnimation(platter.get(), @"opacity", @0, @1);
    }

    _platterLayer = platter;
    _snapshotLayer = snapshot;

    _isSittingDown = false;
}

- (void)updateDebugIndicator
{
    auto searchBoundingRect = self.linkSearchRect;

    RetainPtr<CALayer> debugIndicator = _debugIndicatorLayer ?: adoptNS([[CALayer alloc] init]);
    [debugIndicator setBounds:CGRectMake(0, 0, searchBoundingRect.width(), searchBoundingRect.height())];
    [debugIndicator setPosition:searchBoundingRect.center()];
    [debugIndicator setBorderColor:[UIColor colorWithRed:1 green:0 blue:0 alpha:1].CGColor];
    [debugIndicator setBorderWidth:1];
    [debugIndicator web_disableAllActions];
    [[_view layer] addSublayer:debugIndicator.get()];

    RetainPtr<CATextLayer> linkCountLayer = _linkCountLayer ?: adoptNS([[CATextLayer alloc] init]);
    [linkCountLayer setBounds:CGRectMake(0, 0, 50, 16)];
    [linkCountLayer setPosition:CGPointMake(-10, (searchBoundingRect.height() / 2) - 8)];
    [linkCountLayer setForegroundColor:UIColor.blackColor.CGColor];
    [linkCountLayer setShadowColor:UIColor.whiteColor.CGColor];
    [linkCountLayer setShadowOffset:CGSizeMake(0, 0)];
    [linkCountLayer setShadowOpacity:1];
    [linkCountLayer setShadowRadius:2];
    [linkCountLayer setFontSize:16];
    [linkCountLayer setContentsScale:4];
    [linkCountLayer setAlignmentMode:kCAAlignmentCenter];
    [linkCountLayer web_disableAllActions];
    [debugIndicator addSublayer:linkCountLayer.get()];

    RetainPtr<CAShapeLayer> linkHighlightLayer = _linkHighlightLayer ?: adoptNS([[CAShapeLayer alloc] init]);
    [linkHighlightLayer setFillColor:UIColor.clearColor.CGColor];
    [linkHighlightLayer setStrokeColor:[UIColor.blueColor colorWithAlphaComponent:0.3].CGColor];
    [linkHighlightLayer web_disableAllActions];
    [[_view layer] addSublayer:linkHighlightLayer.get()];

    [_delegate interactableRegionsForHoverPlatter:self inRect:searchBoundingRect completionHandler:makeBlockPtr([self, strongSelf = retainPtr(self)] (Vector<WebCore::FloatRect> rects) {
        if (!_linkCountLayer && !_linkHighlightLayer)
            return;

        [_linkCountLayer setString:[NSString stringWithFormat:@"%zu", rects.size()]];

        WebCore::Path path;
        for (const auto& rect : rects)
            path.addRect(rect);
        [_linkHighlightLayer setPath:path.platformPath()];
    }).get()];

    _debugIndicatorLayer = debugIndicator;
    _linkCountLayer = linkCountLayer;
    _linkHighlightLayer = linkHighlightLayer;
}

- (void)dismissPlatterWithAnimation:(BOOL)withAnimation
{
    if (_isSittingDown)
        return;
    _isSittingDown = true;

    if (!withAnimation) {
        [_debugIndicatorLayer removeFromSuperlayer];
        [_platterLayer removeFromSuperlayer];
        [_linkCountLayer removeFromSuperlayer];
        [_linkHighlightLayer removeFromSuperlayer];
        [self clearLayers];
        return;
    }

    [CATransaction begin];
    [CATransaction setCompletionBlock:[protectedSelf = retainPtr(self), platterLayer = _platterLayer] {
        [protectedSelf didFinishDismissalAnimation:platterLayer.get()];
    }];

    auto parameters = WKHoverPlatterDomain.rootSettings;
    if (parameters.animateScale) {
        auto inverseScale = 1.f / parameters.platterScale;
        addAnimation(_platterLayer.get(), @"transform", [NSValue valueWithCATransform3D:CATransform3DIdentity], [NSValue valueWithCATransform3D:CATransform3DMakeScale(inverseScale, inverseScale, 1)]);
        addAnimation(_snapshotLayer.get(), @"sublayerTransform", [NSValue valueWithCATransform3D:[_snapshotLayer sublayerTransform]], [NSValue valueWithCATransform3D:CATransform3DIdentity]);
        addAnimation(_platterLayer.get(), @"shadowOpacity", @([_platterLayer shadowOpacity]), @0);
        addAnimation(_snapshotLayer.get(), @"borderColor", (id)[_snapshotLayer borderColor], (id)UIColor.clearColor.CGColor);
    } else
        addAnimation(_platterLayer.get(), @"opacity", @1, @0);

    [CATransaction commit];
}

- (void)didFinishDismissalAnimation:(CALayer *)platterLayer
{
    [platterLayer removeFromSuperlayer];
}

- (void)clearLayers
{
    _debugIndicatorLayer = nil;
    _platterLayer = nil;
    _snapshotLayer = nil;
    _linkCountLayer = nil;
    _linkHighlightLayer = nil;
}

- (WebCore::FloatPoint)adjustedPointForPoint:(WebCore::FloatPoint)point
{
    if (_isSittingDown)
        return point;

    auto delta = point - _hoverPoint;
    delta.scale(1.f / WKHoverPlatterDomain.rootSettings.platterScale);
    return _hoverPoint + delta;
}

- (WebKit::NativeWebMouseEvent)adjustedEventForEvent:(const WebKit::NativeWebMouseEvent&)event
{
    if (_isSittingDown)
        return event;

    auto inverseScale = 1.f / WKHoverPlatterDomain.rootSettings.platterScale;
    auto adjustedPoint = roundedIntPoint([self adjustedPointForPoint:event.position()]);
    return WebKit::NativeWebMouseEvent(event, adjustedPoint, adjustedPoint, event.deltaX() * inverseScale, event.deltaY() * inverseScale, event.deltaZ() * inverseScale);
}

@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT)
