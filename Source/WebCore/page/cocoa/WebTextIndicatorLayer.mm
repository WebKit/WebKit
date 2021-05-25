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
#import "WebTextIndicatorLayer.h"

#import "GeometryUtilities.h"
#import "GraphicsContext.h"
#import "PathUtilities.h"
#import "TextIndicator.h"
#import "TextIndicatorWindow.h"
#import "WebActionDisablingCALayerDelegate.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>

#if PLATFORM(MAC)
#import <pal/spi/cocoa/NSColorSPI.h>
#endif

constexpr CFTimeInterval bounceWithCrossfadeAnimationDuration = 0.3;
constexpr CFTimeInterval fadeInAnimationDuration = 0.15;
constexpr CFTimeInterval fadeOutAnimationDuration = 0.3;

constexpr CGFloat borderWidth = 0;
constexpr CGFloat cornerRadius = 0;
constexpr CGFloat dropShadowOffsetX = 0;
constexpr CGFloat dropShadowOffsetY = 1;

constexpr NSString * const textLayerKey = @"TextLayer";
constexpr NSString * const dropShadowLayerKey = @"DropShadowLayer";
constexpr NSString * const rimShadowLayerKey = @"RimShadowLayer";

@implementation WebTextIndicatorLayer

@synthesize fadingOut = _fadingOut;

static bool indicatorWantsContentCrossfade(const WebCore::TextIndicator& indicator)
{
    if (!indicator.data().contentImageWithHighlight)
        return false;

    switch (indicator.presentationTransition()) {
    case WebCore::TextIndicatorPresentationTransition::BounceAndCrossfade:
        return true;

    case WebCore::TextIndicatorPresentationTransition::Bounce:
    case WebCore::TextIndicatorPresentationTransition::FadeIn:
    case WebCore::TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static bool indicatorWantsFadeIn(const WebCore::TextIndicator& indicator)
{
    switch (indicator.presentationTransition()) {
    case WebCore::TextIndicatorPresentationTransition::FadeIn:
        return true;

    case WebCore::TextIndicatorPresentationTransition::Bounce:
    case WebCore::TextIndicatorPresentationTransition::BounceAndCrossfade:
    case WebCore::TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

- (bool)indicatorWantsBounce:(const WebCore::TextIndicator&)indicator
{
    switch (indicator.presentationTransition()) {
    case WebCore::TextIndicatorPresentationTransition::BounceAndCrossfade:
    case WebCore::TextIndicatorPresentationTransition::Bounce:
        return true;

    case WebCore::TextIndicatorPresentationTransition::FadeIn:
    case WebCore::TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

- (bool)indicatorWantsManualAnimation:(const WebCore::TextIndicator&)indicator
{
    switch (indicator.presentationTransition()) {
    case WebCore::TextIndicatorPresentationTransition::FadeIn:
        return true;

    case WebCore::TextIndicatorPresentationTransition::Bounce:
    case WebCore::TextIndicatorPresentationTransition::BounceAndCrossfade:
    case WebCore::TextIndicatorPresentationTransition::None:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

- (instancetype)initWithFrame:(CGRect)frame textIndicator:(WebCore::TextIndicator&)textIndicator margin:(CGSize)margin offset:(CGPoint)offset
{
    if (!(self = [super init]))
        return nil;
    
    self.anchorPoint = CGPointZero;
    self.frame = frame;

    _textIndicator = &textIndicator;
    _margin = margin;

    RefPtr<WebCore::NativeImage> contentsImage;
    WebCore::FloatSize contentsImageLogicalSize { 1, 1 };
    if (auto* contentImage = _textIndicator->contentImage()) {
        contentsImageLogicalSize = contentImage->size();
        contentsImageLogicalSize.scale(1 / _textIndicator->contentImageScaleFactor());
        if (indicatorWantsContentCrossfade(*_textIndicator) && _textIndicator->contentImageWithHighlight())
            contentsImage = _textIndicator->contentImageWithHighlight()->nativeImage();
        else
            contentsImage = contentImage->nativeImage();
    }

    auto bounceLayers = adoptNS([[NSMutableArray alloc] init]);

    RetainPtr<CGColorRef> highlightColor;
    auto rimShadowColor = adoptCF(CGColorCreateGenericGray(0, 0.35));
    auto dropShadowColor = adoptCF(CGColorCreateGenericGray(0, 0.2));
    auto borderColor = adoptCF(CGColorCreateSRGB(0.96, 0.9, 0, 1));
#if PLATFORM(MAC)
    highlightColor = [NSColor findHighlightColor].CGColor;
#else
    highlightColor = adoptCF(CGColorCreateSRGB(.99, .89, 0.22, 1.0));
#endif
    
    auto textRectsInBoundingRectCoordinates = _textIndicator->textRectsInBoundingRectCoordinates();

    auto paths = WebCore::PathUtilities::pathsWithShrinkWrappedRects(textRectsInBoundingRectCoordinates, cornerRadius);

    for (const auto& path : paths) {
        WebCore::FloatRect pathBoundingRect = path.boundingRect();

        WebCore::Path translatedPath;
        WebCore::AffineTransform transform;
        transform.translate(-pathBoundingRect.location());
        translatedPath.addPath(path, transform);

        WebCore::FloatRect offsetTextRect = pathBoundingRect;
        offsetTextRect.move(offset.x, offset.y);

        WebCore::FloatRect bounceLayerRect = offsetTextRect;
        bounceLayerRect.move(_margin.width, _margin.height);

        RetainPtr<CALayer> bounceLayer = adoptNS([[CALayer alloc] init]);
        [bounceLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [bounceLayer setFrame:bounceLayerRect];
        [bounceLayer setOpacity:0];
        [bounceLayers addObject:bounceLayer.get()];

        WebCore::FloatRect yellowHighlightRect(WebCore::FloatPoint(), bounceLayerRect.size());

#if PLATFORM(MAC)
        RetainPtr<CALayer> dropShadowLayer = adoptNS([[CALayer alloc] init]);
        [dropShadowLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [dropShadowLayer setShadowColor:dropShadowColor.get()];
        [dropShadowLayer setShadowRadius:WebCore::dropShadowBlurRadius];
        [dropShadowLayer setShadowOffset:CGSizeMake(dropShadowOffsetX, dropShadowOffsetY)];
        [dropShadowLayer setShadowPath:translatedPath.platformPath()];
        [dropShadowLayer setShadowOpacity:1];
        [dropShadowLayer setFrame:yellowHighlightRect];
        [bounceLayer addSublayer:dropShadowLayer.get()];
        [bounceLayer setValue:dropShadowLayer.get() forKey:dropShadowLayerKey];

        RetainPtr<CALayer> rimShadowLayer = adoptNS([[CALayer alloc] init]);
        [rimShadowLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [rimShadowLayer setFrame:yellowHighlightRect];
        [rimShadowLayer setShadowColor:rimShadowColor.get()];
        [rimShadowLayer setShadowRadius:WebCore::rimShadowBlurRadius];
        [rimShadowLayer setShadowPath:translatedPath.platformPath()];
        [rimShadowLayer setShadowOffset:CGSizeZero];
        [rimShadowLayer setShadowOpacity:1];
        [rimShadowLayer setFrame:yellowHighlightRect];
        [bounceLayer addSublayer:rimShadowLayer.get()];
        [bounceLayer setValue:rimShadowLayer.get() forKey:rimShadowLayerKey];
#endif // PLATFORM(MAC)
        
        RetainPtr<CALayer> textLayer = adoptNS([[CALayer alloc] init]);
        [textLayer setBackgroundColor:highlightColor.get()];
        [textLayer setBorderColor:borderColor.get()];
        [textLayer setBorderWidth:borderWidth];
        [textLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        if (contentsImage)
            [textLayer setContents:(__bridge id)contentsImage->platformImage().get()];

        RetainPtr<CAShapeLayer> maskLayer = adoptNS([[CAShapeLayer alloc] init]);
        [maskLayer setPath:translatedPath.platformPath()];
        [textLayer setMask:maskLayer.get()];

        WebCore::FloatRect imageRect = pathBoundingRect;
        [textLayer setContentsRect:CGRectMake(imageRect.x() / contentsImageLogicalSize.width(), imageRect.y() / contentsImageLogicalSize.height(), imageRect.width() / contentsImageLogicalSize.width(), imageRect.height() / contentsImageLogicalSize.height())];
        [textLayer setContentsGravity:kCAGravityCenter];
        [textLayer setContentsScale:_textIndicator->contentImageScaleFactor()];
        [textLayer setFrame:yellowHighlightRect];
        [bounceLayer setValue:textLayer.get() forKey:textLayerKey];
        [bounceLayer addSublayer:textLayer.get()];
    }

    self.sublayers = bounceLayers.get();
    _bounceLayers = bounceLayers;

    return self;
}

static RetainPtr<CAKeyframeAnimation> createBounceAnimation(CFTimeInterval duration)
{
    RetainPtr<CAKeyframeAnimation> bounceAnimation = [CAKeyframeAnimation animationWithKeyPath:@"transform"];
    [bounceAnimation setValues:@[
        [NSValue valueWithCATransform3D:CATransform3DIdentity],
        [NSValue valueWithCATransform3D:CATransform3DMakeScale(WebCore::midBounceScale, WebCore::midBounceScale, 1)],
        [NSValue valueWithCATransform3D:CATransform3DIdentity]
        ]];
    [bounceAnimation setDuration:duration];

    return bounceAnimation;
}

static RetainPtr<CABasicAnimation> createContentCrossfadeAnimation(CFTimeInterval duration, WebCore::TextIndicator& textIndicator)
{
    RetainPtr<CABasicAnimation> crossfadeAnimation = [CABasicAnimation animationWithKeyPath:@"contents"];
    auto contentsImage = textIndicator.contentImage()->nativeImage();
    [crossfadeAnimation setToValue:(__bridge id)contentsImage->platformImage().get()];
    [crossfadeAnimation setFillMode:kCAFillModeForwards];
    [crossfadeAnimation setRemovedOnCompletion:NO];
    [crossfadeAnimation setDuration:duration];

    return crossfadeAnimation;
}

static RetainPtr<CABasicAnimation> createShadowFadeAnimation(CFTimeInterval duration)
{
    RetainPtr<CABasicAnimation> fadeShadowInAnimation = [CABasicAnimation animationWithKeyPath:@"shadowOpacity"];
    [fadeShadowInAnimation setFromValue:@0];
    [fadeShadowInAnimation setToValue:@1];
    [fadeShadowInAnimation setFillMode:kCAFillModeForwards];
    [fadeShadowInAnimation setRemovedOnCompletion:NO];
    [fadeShadowInAnimation setDuration:duration];

    return fadeShadowInAnimation;
}

static RetainPtr<CABasicAnimation> createFadeInAnimation(CFTimeInterval duration)
{
    RetainPtr<CABasicAnimation> fadeInAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [fadeInAnimation setFromValue:@0];
    [fadeInAnimation setToValue:@1];
    [fadeInAnimation setFillMode:kCAFillModeForwards];
    [fadeInAnimation setRemovedOnCompletion:NO];
    [fadeInAnimation setDuration:duration];

    return fadeInAnimation;
}

- (CFTimeInterval)_animationDuration
{
    if ([self indicatorWantsBounce:*_textIndicator]) {
        if (indicatorWantsContentCrossfade(*_textIndicator))
            return bounceWithCrossfadeAnimationDuration;
        return WebCore::bounceAnimationDuration.value();
    }

    return fadeInAnimationDuration;
}

- (BOOL)hasCompletedAnimation
{
    return _hasCompletedAnimation;
}

- (void)present
{
    bool wantsBounce = [self indicatorWantsBounce:*_textIndicator];
    bool wantsCrossfade = indicatorWantsContentCrossfade(*_textIndicator);
    bool wantsFadeIn = indicatorWantsFadeIn(*_textIndicator);
    CFTimeInterval animationDuration = [self _animationDuration];

    _hasCompletedAnimation = false;

    RetainPtr<CAAnimation> presentationAnimation;
    if (wantsBounce)
        presentationAnimation = createBounceAnimation(animationDuration);
    else if (wantsFadeIn)
        presentationAnimation = createFadeInAnimation(animationDuration);

    RetainPtr<CABasicAnimation> crossfadeAnimation;
    RetainPtr<CABasicAnimation> fadeShadowInAnimation;
    if (wantsCrossfade) {
        crossfadeAnimation = createContentCrossfadeAnimation(animationDuration, *_textIndicator);
        fadeShadowInAnimation = createShadowFadeAnimation(animationDuration);
    }

    [CATransaction begin];
    for (CALayer *bounceLayer in _bounceLayers.get()) {
        if ([self indicatorWantsManualAnimation:*_textIndicator])
            bounceLayer.speed = 0;

        if (!wantsFadeIn)
            bounceLayer.opacity = 1;

        if (presentationAnimation)
            [bounceLayer addAnimation:presentationAnimation.get() forKey:@"presentation"];

        if (wantsCrossfade) {
            [[bounceLayer valueForKey:textLayerKey] addAnimation:crossfadeAnimation.get() forKey:@"contentTransition"];
            [[bounceLayer valueForKey:dropShadowLayerKey] addAnimation:fadeShadowInAnimation.get() forKey:@"fadeShadowIn"];
            [[bounceLayer valueForKey:rimShadowLayerKey] addAnimation:fadeShadowInAnimation.get() forKey:@"fadeShadowIn"];
        }
    }
    [CATransaction commit];
}

- (void)hideWithCompletionHandler:(void(^)(void))completionHandler
{
    RetainPtr<CABasicAnimation> fadeAnimation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [fadeAnimation setFromValue:@1];
    [fadeAnimation setToValue:@0];
    [fadeAnimation setFillMode:kCAFillModeForwards];
    [fadeAnimation setRemovedOnCompletion:NO];
    [fadeAnimation setDuration:fadeOutAnimationDuration];

    [CATransaction begin];
    [CATransaction setCompletionBlock:completionHandler];
    [self addAnimation:fadeAnimation.get() forKey:@"fadeOut"];
    [CATransaction commit];
}

- (void)setAnimationProgress:(float)progress
{
    if (_hasCompletedAnimation)
        return;

    if (progress == 1) {
        _hasCompletedAnimation = true;

        for (CALayer *bounceLayer in _bounceLayers.get()) {
            // Continue the animation from wherever it had manually progressed to.
            CFTimeInterval beginTime = bounceLayer.timeOffset;
            bounceLayer.speed = 1;
            beginTime = [bounceLayer convertTime:CACurrentMediaTime() fromLayer:nil] - beginTime;
            bounceLayer.beginTime = beginTime;
        }
    } else {
        CFTimeInterval animationDuration = [self _animationDuration];
        for (CALayer *bounceLayer in _bounceLayers.get())
            bounceLayer.timeOffset = progress * animationDuration;
    }
}

- (BOOL)isFlipped
{
    return YES;
}

@end
