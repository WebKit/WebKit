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

#if HAVE(UIKIT_WITH_MOUSE_SUPPORT) || ENABLE(HOVER_GESTURE_RECOGNIZER)

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

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/WKHoverPlatterAdditions.mm>
#else
static CGFloat setAdditionalPlatterLayerProperties(CALayer *, CALayer *) { return 0; }
static void addAdditionalIncomingAnimations(CALayer *, CGFloat) { }
static void addAdditionalDismissalAnimations(CALayer *) { }
#endif

@implementation WKHoverPlatter {
    __weak UIView *_view;
    __weak id <WKHoverPlatterDelegate> _delegate;
    WebCore::FloatPoint _hoverPoint;

    BOOL _hasOutstandingPositionInformationRequest;
    BOOL _hasDeferredPositionInformationRequest;

    BOOL _isSittingDown;

    WebCore::FloatRect _lastHoverBounds;

    RetainPtr<CAShapeLayer> _lastMaskLayer;
    RetainPtr<CALayer> _lastShadowLayer;
    RetainPtr<CALayer> _lastPlatterLayer;
    RetainPtr<CALayer> _lastBackgroundLayer;
    RetainPtr<CAPortalLayer> _lastSnapshotLayer;
}

- (instancetype)initWithView:(UIView *)view delegate:(id <WKHoverPlatterDelegate>)delegate
{
    auto parameters = WKHoverPlatterDomain.rootSettings;
    if (!parameters.platterEnabledForMouse && !parameters.platterEnabledForHover)
        return nil;

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

- (void)setHoverPoint:(WebCore::FloatPoint)point
{
    _hoverPoint = point;

    if (_hasOutstandingPositionInformationRequest) {
        _hasDeferredPositionInformationRequest = YES;
        return;
    }

    [self requestPositionInformationForCurrentHoverPoint];
}

- (void)requestPositionInformationForCurrentHoverPoint
{
    _hasDeferredPositionInformationRequest = NO;
    _hasOutstandingPositionInformationRequest = YES;
    WebKit::InteractionInformationRequest positionInformationRequest { WebCore::roundedIntPoint(_hoverPoint) };
    // FIXME: This TextIndicator doesn't need a snapshot (and avoiding it would be a sizable performance win).
    positionInformationRequest.includeLinkIndicator = YES;
    [_delegate positionInformationForHoverPlatter:self withRequest:positionInformationRequest completionHandler:[weakSelf = WeakObjCPtr<WKHoverPlatter>(self)] (WebKit::InteractionInformationAtPosition information) {
        [weakSelf didReceivePositionInformation:information];
    }];
}

- (void)didReceivePositionInformation:(const WebKit::InteractionInformationAtPosition&)information
{
    _hasOutstandingPositionInformationRequest = NO;
    if (_hasDeferredPositionInformationRequest)
        [self requestPositionInformationForCurrentHoverPoint];

    auto preInflationBoundingRect = information.linkIndicator.textBoundingRectInRootViewCoordinates;
    if (preInflationBoundingRect.isEmpty())
        preInflationBoundingRect = information.bounds;

    if (!information.isLink || preInflationBoundingRect.isEmpty()) {
        _lastHoverBounds = { };
        [self dismissPlatterWithAnimation:YES];
        return;
    }

    if (_lastHoverBounds == preInflationBoundingRect && !_isSittingDown)
        return;
    _lastHoverBounds = preInflationBoundingRect;

    auto parameters = WKHoverPlatterDomain.rootSettings;
    auto animateBetweenPlatters = parameters.animateBetweenPlatters;
    if (!animateBetweenPlatters)
        [self dismissPlatterWithAnimation:YES];

    _isSittingDown = false;

    WebCore::FloatRect boundingRect;
    Vector<WebCore::FloatRect> inflatedIndicatedRects;
    auto indicatedRects = information.linkIndicator.textRectsInBoundingRectCoordinates;
    auto platterPadding = parameters.platterPadding;
    if (indicatedRects.isEmpty()) {
        boundingRect = preInflationBoundingRect;
        boundingRect.inflate(platterPadding);
        inflatedIndicatedRects.append(preInflationBoundingRect);
    } else {
        for (auto rect : indicatedRects) {
            rect.inflate(platterPadding);
            inflatedIndicatedRects.append(rect);
            boundingRect.unite(rect);
        }
        boundingRect.moveBy(preInflationBoundingRect.location());
    }

    RetainPtr<CGPathRef> inflatedPath = WebCore::PathUtilities::pathWithShrinkWrappedRects(inflatedIndicatedRects, parameters.platterCornerRadius).platformPath();
    auto platterInflationSize = parameters.platterInflationSize;
    CGFloat platterScale = CGFloatMin((boundingRect.width() + platterInflationSize) / boundingRect.width(), (boundingRect.height() + platterInflationSize) / boundingRect.height());

    // FIXME: Consider using UIKit's in-process retargetable animations, because reading animation state from the presentation layer is racy.
    bool hadOldLayers = !!_lastShadowLayer;
    CGRect oldShadowBounds = [_lastShadowLayer presentationLayer].bounds;
    CGPoint oldShadowPosition = [_lastShadowLayer presentationLayer].position;
    CGFloat oldShadowOpacity = [_lastShadowLayer presentationLayer].shadowOpacity;
    CGFloat oldShadowRadius = [_lastShadowLayer presentationLayer].shadowRadius;
    CATransform3D oldShadowSublayerTransform = _lastShadowLayer ? [_lastShadowLayer presentationLayer].sublayerTransform : CATransform3DIdentity;
    RetainPtr<CGPathRef> oldShadowPath = [_lastShadowLayer presentationLayer].shadowPath;
    RetainPtr<CALayer> shadow = _lastShadowLayer ?: adoptNS([[CALayer alloc] init]);
    auto shadowFrame = CGRectOffset(boundingRect, platterPadding, platterPadding);
    [shadow setFrame:shadowFrame];
    [shadow setShadowColor:UIColor.blackColor.CGColor];
    [shadow setShadowRadius:parameters.platterShadowRadius];
    [shadow setShadowOffset:CGSizeZero];
    [shadow setShadowOpacity:parameters.platterShadowOpacity];
    [shadow setSublayerTransform:CATransform3DMakeScale(platterScale, platterScale, 1)];
    [shadow setShadowPath:inflatedPath.get()];
    [shadow web_disableAllActions];
    [[[_view superview] layer] addSublayer:shadow.get()];

    CGRect oldPlatterBounds = [_lastPlatterLayer presentationLayer].bounds;
    CGPoint oldPlatterPosition = [_lastPlatterLayer presentationLayer].position;
    RetainPtr<CALayer> platter = _lastPlatterLayer ?: adoptNS([[CALayer alloc] init]);
    [platter setBounds:CGRectMake(0, 0, boundingRect.width(), boundingRect.height())];
    [platter setPosition:CGPointMake(shadowFrame.size.width / 2 - platterPadding, shadowFrame.size.height / 2 - platterPadding)];
    auto oldPlatterAdditionalAnimationValue = setAdditionalPlatterLayerProperties(platter.get(), [_lastPlatterLayer presentationLayer]);
    [platter web_disableAllActions];
    [shadow addSublayer:platter.get()];

    CGRect oldMaskBounds = [_lastMaskLayer presentationLayer].bounds;
    CGPoint oldMaskPosition = [_lastMaskLayer presentationLayer].position;
    RetainPtr<CGPathRef> oldMaskPath = [_lastMaskLayer presentationLayer].path;
    RetainPtr<CAShapeLayer> mask = _lastMaskLayer ?: adoptNS([[CAShapeLayer alloc] init]);
    [mask setFillColor:[UIColor blackColor].CGColor];
    [mask setBounds:CGRectMake(0, 0, boundingRect.width(), boundingRect.height())];
    [mask setPosition:CGPointMake(boundingRect.width() / 2 + platterPadding, boundingRect.height() / 2 + platterPadding)];
    [mask setPath:inflatedPath.get()];
    [mask web_disableAllActions];

    CGRect oldBackgroundBounds = [_lastBackgroundLayer presentationLayer].bounds;
    CGPoint oldBackgroundPosition = [_lastBackgroundLayer presentationLayer].position;
    RetainPtr<CGColorRef> oldBackgroundColor = [_lastBackgroundLayer presentationLayer].backgroundColor;
    RetainPtr<CALayer> background = _lastBackgroundLayer ?: adoptNS([[CALayer alloc] init]);
    [background setFrame:[platter bounds]];
    [background setBackgroundColor:cachedCGColor(information.linkIndicator.estimatedBackgroundColor).get()];
    [background setMask:mask.get()];
    [background web_disableAllActions];
    [platter addSublayer:background.get()];

    CGRect oldSnapshotBounds = [_lastSnapshotLayer presentationLayer].bounds;
    CGPoint oldSnapshotPosition = [_lastSnapshotLayer presentationLayer].position;
    CATransform3D oldSnapshotSublayerTransform = _lastSnapshotLayer ? [_lastSnapshotLayer presentationLayer].sublayerTransform : CATransform3DIdentity;
    RetainPtr<CAPortalLayer> snapshot = _lastSnapshotLayer ?: adoptNS([[CAPortalLayer alloc] init]);
    [snapshot setSourceLayer:[_view layer]];
    [snapshot setMatchesPosition:YES];
    [snapshot setMatchesTransform:YES];
    [snapshot setAnchorPoint:CGPointMake(0.5, 0.5)];
    [snapshot setSublayerTransform:CATransform3DMakeScale(platterScale, platterScale, 1)];
    [snapshot setMasksToBounds:YES];
    [snapshot setFrame:CGRectMake(0, 0, boundingRect.width(), boundingRect.height())];
    [snapshot web_disableAllActions];
    [background addSublayer:snapshot.get()];

    BOOL needsAnimateIn = !_lastPlatterLayer || _isSittingDown;
    if (needsAnimateIn) {
        addAnimation(shadow.get(), @"shadowOpacity", @(oldShadowOpacity), @([shadow shadowOpacity]));
        addAnimation(shadow.get(), @"shadowRadius", @(oldShadowRadius), @([shadow shadowRadius]));
        addAnimation(shadow.get(), @"sublayerTransform", [NSValue valueWithCATransform3D:oldShadowSublayerTransform], [NSValue valueWithCATransform3D:[shadow sublayerTransform]]);

        addAnimation(snapshot.get(), @"sublayerTransform", [NSValue valueWithCATransform3D:oldSnapshotSublayerTransform], [NSValue valueWithCATransform3D:[snapshot sublayerTransform]]);

        addAdditionalIncomingAnimations(platter.get(), oldPlatterAdditionalAnimationValue);
    }

    if (animateBetweenPlatters && hadOldLayers) {
        addAnimation(mask.get(), @"bounds", [NSValue valueWithCGRect:oldMaskBounds], [NSValue valueWithCGRect:[mask bounds]]);
        addAnimation(mask.get(), @"position", [NSValue valueWithCGPoint:oldMaskPosition], [NSValue valueWithCGPoint:[mask position]]);
        addAnimation(mask.get(), @"path", (id)oldMaskPath.get(), (id)[mask path]);

        addAnimation(shadow.get(), @"bounds", [NSValue valueWithCGRect:oldShadowBounds], [NSValue valueWithCGRect:[shadow bounds]]);
        addAnimation(shadow.get(), @"position", [NSValue valueWithCGPoint:oldShadowPosition], [NSValue valueWithCGPoint:[shadow position]]);
        addAnimation(shadow.get(), @"shadowPath", (id)oldShadowPath.get(), (id)[shadow shadowPath]);

        addAnimation(platter.get(), @"bounds", [NSValue valueWithCGRect:oldPlatterBounds], [NSValue valueWithCGRect:[platter bounds]]);
        addAnimation(platter.get(), @"position", [NSValue valueWithCGPoint:oldPlatterPosition], [NSValue valueWithCGPoint:[platter position]]);

        addAnimation(background.get(), @"bounds", [NSValue valueWithCGRect:oldBackgroundBounds], [NSValue valueWithCGRect:[background bounds]]);
        addAnimation(background.get(), @"position", [NSValue valueWithCGPoint:oldBackgroundPosition], [NSValue valueWithCGPoint:[background position]]);

        addAnimation(snapshot.get(), @"bounds", [NSValue valueWithCGRect:oldSnapshotBounds], [NSValue valueWithCGRect:[snapshot bounds]]);
        addAnimation(snapshot.get(), @"position", [NSValue valueWithCGPoint:oldSnapshotPosition], [NSValue valueWithCGPoint:[snapshot position]]);
    }

    _lastMaskLayer = mask;
    _lastShadowLayer = shadow;
    _lastPlatterLayer = platter;
    _lastBackgroundLayer = background;
    _lastSnapshotLayer = snapshot;
}

- (void)dismissPlatterWithAnimation:(BOOL)withAnimation
{
    if (_isSittingDown)
        return;
    _isSittingDown = true;

    if (!withAnimation) {
        [_lastShadowLayer removeFromSuperlayer];
        [self clearLayers];
        return;
    }

    [CATransaction begin];
    [CATransaction setCompletionBlock:[protectedSelf = retainPtr(self), shadowLayer = _lastShadowLayer] {
        [protectedSelf didFinishAnimationForShadow:shadowLayer.get()];
    }];

    auto parameters = WKHoverPlatterDomain.rootSettings;
    addAnimation(_lastShadowLayer.get(), @"shadowOpacity", @([_lastShadowLayer shadowOpacity]), @0);
    addAnimation(_lastShadowLayer.get(), @"shadowRadius", @([_lastShadowLayer shadowRadius]), @0);
    addAnimation(_lastShadowLayer.get(), @"sublayerTransform", [NSValue valueWithCATransform3D:[_lastShadowLayer sublayerTransform]], [NSValue valueWithCATransform3D:CATransform3DIdentity]);
    addAnimation(_lastSnapshotLayer.get(), @"sublayerTransform", [NSValue valueWithCATransform3D:[_lastSnapshotLayer sublayerTransform]], [NSValue valueWithCATransform3D:CATransform3DIdentity]);
    addAdditionalDismissalAnimations(_lastPlatterLayer.get());

    [CATransaction commit];

    if (!parameters.animateBetweenPlatters)
        [self clearLayers];
}

- (void)didFinishAnimationForShadow:(CALayer *)shadowLayer
{
    auto parameters = WKHoverPlatterDomain.rootSettings;
    if (!parameters.animateBetweenPlatters)
        [shadowLayer removeFromSuperlayer];

    if (_isSittingDown && parameters.animateBetweenPlatters && shadowLayer == _lastShadowLayer) {
        [shadowLayer removeFromSuperlayer];
        [self clearLayers];
    }
}

- (void)clearLayers
{
    _lastMaskLayer = nil;
    _lastShadowLayer = nil;
    _lastPlatterLayer = nil;
    _lastBackgroundLayer = nil;
    _lastSnapshotLayer = nil;
}

@end

#endif // HAVE(UIKIT_WITH_MOUSE_SUPPORT) || ENABLE(HOVER_GESTURE_RECOGNIZER)
