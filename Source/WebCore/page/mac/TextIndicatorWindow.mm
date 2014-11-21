/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#import "TextIndicatorWindow.h"

#if PLATFORM(MAC)

#import "GraphicsContext.h"
#import "QuartzCoreSPI.h"
#import "TextIndicator.h"
#import "WebActionDisablingCALayerDelegate.h"

const CFTimeInterval bounceAnimationDuration = 0.12;
const CFTimeInterval bounceWithCrossfadeAnimationDuration = 0.3;
const CFTimeInterval timeBeforeFadeStarts = bounceAnimationDuration + 0.2;
const CFTimeInterval fadeOutAnimationDuration = 0.3;

#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
const CGFloat midBounceScale = 1.5;
const CGFloat horizontalBorder = 3;
const CGFloat verticalBorder = 1;
const CGFloat borderWidth = 1.0;
const CGFloat cornerRadius = 3;
const CGFloat dropShadowOffsetX = 0;
const CGFloat dropShadowOffsetY = 1;
const CGFloat dropShadowBlurRadius = 1.5;
#else
const CGFloat midBounceScale = 1.2;
const CGFloat horizontalBorder = 2;
const CGFloat verticalBorder = 1;
const CGFloat borderWidth = 0;
const CGFloat cornerRadius = 0;
const CGFloat dropShadowOffsetX = 0;
const CGFloat dropShadowOffsetY = 5;
const CGFloat dropShadowBlurRadius = 12;
const CGFloat rimShadowBlurRadius = 2;
#endif

NSString *textLayerKey = @"TextLayer";
NSString *dropShadowLayerKey = @"DropShadowLayer";
NSString *rimShadowLayerKey = @"RimShadowLayer";

using namespace WebCore;

@interface WebTextIndicatorView : NSView {
    RefPtr<TextIndicator> _textIndicator;
    RetainPtr<NSArray> _bounceLayers;
    NSSize _margin;
}

- (instancetype)initWithFrame:(NSRect)frame textIndicator:(PassRefPtr<TextIndicator>)textIndicator margin:(NSSize)margin;

- (void)presentWithCompletionHandler:(void(^)(void))completionHandler;
- (void)hideWithCompletionHandler:(void(^)(void))completionHandler;

@end

@implementation WebTextIndicatorView

- (instancetype)initWithFrame:(NSRect)frame textIndicator:(PassRefPtr<TextIndicator>)textIndicator margin:(NSSize)margin
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _textIndicator = textIndicator;
    _margin = margin;

    self.wantsLayer = YES;
    self.layer.anchorPoint = CGPointZero;

    bool wantsCrossfade = _textIndicator->presentationTransition() == TextIndicatorPresentationTransition::BounceAndCrossfade;

    FloatSize contentsImageLogicalSize = _textIndicator->contentImage()->size();
    contentsImageLogicalSize.scale(1 / _textIndicator->contentImageScaleFactor());
    RetainPtr<CGImageRef> contentsImage;
    if (wantsCrossfade)
        contentsImage = _textIndicator->contentImageWithHighlight()->getCGImageRef();
    else
        contentsImage = _textIndicator->contentImage()->getCGImageRef();

    RetainPtr<NSMutableArray> bounceLayers = adoptNS([[NSMutableArray alloc] init]);

    RetainPtr<CGColorRef> highlightColor = [NSColor colorWithDeviceRed:1 green:1 blue:0 alpha:1].CGColor;
    RetainPtr<CGColorRef> rimShadowColor = [NSColor colorWithDeviceWhite:0 alpha:0.15].CGColor;
    RetainPtr<CGColorRef> dropShadowColor = [NSColor colorWithDeviceWhite:0 alpha:0.2].CGColor;

    RetainPtr<CGColorRef> borderColor = [NSColor colorWithDeviceRed:.96 green:.90 blue:0 alpha:1].CGColor;
    RetainPtr<CGColorRef> gradientDarkColor = [NSColor colorWithDeviceRed:.929 green:.8 blue:0 alpha:1].CGColor;
    RetainPtr<CGColorRef> gradientLightColor = [NSColor colorWithDeviceRed:.949 green:.937 blue:0 alpha:1].CGColor;

    for (auto& textRect : _textIndicator->textRectsInBoundingRectCoordinates()) {
        FloatRect bounceLayerRect = textRect;
        bounceLayerRect.move(_margin.width, _margin.height);
        bounceLayerRect.inflateX(horizontalBorder);
        bounceLayerRect.inflateY(verticalBorder);

        RetainPtr<CALayer> bounceLayer = adoptNS([[CALayer alloc] init]);
        [bounceLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [bounceLayer setFrame:bounceLayerRect];
        [bounceLayer setOpacity:0];
        [bounceLayers addObject:bounceLayer.get()];

        FloatRect yellowHighlightRect(FloatPoint(), bounceLayerRect.size());
        // FIXME (138888): Ideally we wouldn't remove the margin in this case, but we need to
        // ensure that the yellow highlight and contentImageWithHighlight overlap precisely.
        if (wantsCrossfade) {
            yellowHighlightRect.inflateX(-horizontalBorder);
            yellowHighlightRect.inflateY(-verticalBorder);
        }

        RetainPtr<CALayer> dropShadowLayer = adoptNS([[CALayer alloc] init]);
        [dropShadowLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [dropShadowLayer setShadowColor:dropShadowColor.get()];
        [dropShadowLayer setShadowRadius:dropShadowBlurRadius];
        [dropShadowLayer setShadowOffset:CGSizeMake(dropShadowOffsetX, dropShadowOffsetY)];
        [dropShadowLayer setShadowPathIsBounds:YES];
        [dropShadowLayer setShadowOpacity:1];
        [dropShadowLayer setFrame:yellowHighlightRect];
        [dropShadowLayer setCornerRadius:cornerRadius];
        [bounceLayer addSublayer:dropShadowLayer.get()];
        [bounceLayer setValue:dropShadowLayer.get() forKey:dropShadowLayerKey];

#if !ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
        RetainPtr<CALayer> rimShadowLayer = adoptNS([[CALayer alloc] init]);
        [rimShadowLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [rimShadowLayer setFrame:yellowHighlightRect];
        [rimShadowLayer setShadowColor:rimShadowColor.get()];
        [rimShadowLayer setShadowRadius:rimShadowBlurRadius];
        [rimShadowLayer setShadowPathIsBounds:YES];
        [rimShadowLayer setShadowOffset:CGSizeZero];
        [rimShadowLayer setShadowOpacity:1];
        [rimShadowLayer setFrame:yellowHighlightRect];
        [rimShadowLayer setCornerRadius:cornerRadius];
        [bounceLayer addSublayer:rimShadowLayer.get()];
        [bounceLayer setValue:rimShadowLayer.get() forKey:rimShadowLayerKey];
#endif

#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
        RetainPtr<CAGradientLayer> textLayer = adoptNS([[CAGradientLayer alloc] init]);
        [textLayer setColors:@[ (id)gradientLightColor.get(), (id)gradientDarkColor.get() ]];
#else
        RetainPtr<CALayer> textLayer = adoptNS([[CALayer alloc] init]);
#endif
        [textLayer setBackgroundColor:highlightColor.get()];
        [textLayer setBorderColor:borderColor.get()];
        [textLayer setBorderWidth:borderWidth];
        [textLayer setDelegate:[WebActionDisablingCALayerDelegate shared]];
        [textLayer setContents:(id)contentsImage.get()];

        FloatRect imageRect = textRect;
        imageRect.move(_textIndicator->textBoundingRectInWindowCoordinates().location() - _textIndicator->selectionRectInWindowCoordinates().location());
        [textLayer setContentsRect:CGRectMake(imageRect.x() / contentsImageLogicalSize.width(), imageRect.y() / contentsImageLogicalSize.height(), imageRect.width() / contentsImageLogicalSize.width(), imageRect.height() / contentsImageLogicalSize.height())];
        [textLayer setContentsGravity:kCAGravityCenter];
        [textLayer setContentsScale:_textIndicator->contentImageScaleFactor()];
        [textLayer setFrame:yellowHighlightRect];
        [textLayer setCornerRadius:cornerRadius];
        [bounceLayer setValue:textLayer.get() forKey:textLayerKey];
        [bounceLayer addSublayer:textLayer.get()];
    }

    self.layer.sublayers = bounceLayers.get();
    _bounceLayers = bounceLayers;

    return self;
}

- (void)presentWithCompletionHandler:(void(^)(void))completionHandler
{
    bool wantsCrossfade = _textIndicator->presentationTransition() == TextIndicatorPresentationTransition::BounceAndCrossfade;
    double animationDuration = wantsCrossfade ? bounceWithCrossfadeAnimationDuration : bounceAnimationDuration;
    RetainPtr<CAKeyframeAnimation> bounceAnimation = [CAKeyframeAnimation animationWithKeyPath:@"transform"];
    [bounceAnimation setValues:@[
        [NSValue valueWithCATransform3D:CATransform3DIdentity],
        [NSValue valueWithCATransform3D:CATransform3DMakeScale(midBounceScale, midBounceScale, 1)],
        [NSValue valueWithCATransform3D:CATransform3DIdentity]
        ]];
    [bounceAnimation setDuration:animationDuration];

    RetainPtr<CABasicAnimation> crossfadeAnimation;
    RetainPtr<CABasicAnimation> fadeShadowInAnimation;
    if (wantsCrossfade) {
        crossfadeAnimation = [CABasicAnimation animationWithKeyPath:@"contents"];
        RetainPtr<CGImageRef> contentsImage = _textIndicator->contentImage()->getCGImageRef();
        [crossfadeAnimation setToValue:(id)contentsImage.get()];
        [crossfadeAnimation setFillMode:kCAFillModeForwards];
        [crossfadeAnimation setRemovedOnCompletion:NO];
        [crossfadeAnimation setDuration:animationDuration];

        fadeShadowInAnimation = [CABasicAnimation animationWithKeyPath:@"shadowOpacity"];
        [fadeShadowInAnimation setFromValue:@0];
        [fadeShadowInAnimation setToValue:@1];
        [fadeShadowInAnimation setFillMode:kCAFillModeForwards];
        [fadeShadowInAnimation setRemovedOnCompletion:NO];
        [fadeShadowInAnimation setDuration:animationDuration];
    }

    [CATransaction begin];
    [CATransaction setCompletionBlock:completionHandler];
    for (CALayer* bounceLayer in _bounceLayers.get()) {
        [bounceLayer setOpacity:1];
        [bounceLayer addAnimation:bounceAnimation.get() forKey:@"bounce"];
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
    [self.layer addAnimation:fadeAnimation.get() forKey:@"fadeOut"];
    [CATransaction commit];
}

- (BOOL)isFlipped
{
    return YES;
}

@end

namespace WebCore {

TextIndicatorWindow::TextIndicatorWindow(NSView *targetView)
    : m_targetView(targetView)
    , m_startFadeOutTimer(RunLoop::main(), this, &TextIndicatorWindow::startFadeOutTimerFired)
{
}

TextIndicatorWindow::~TextIndicatorWindow()
{
    closeWindow();
}

void TextIndicatorWindow::setTextIndicator(PassRefPtr<TextIndicator> textIndicator, bool fadeOut, std::function<void ()> animationCompletionHandler)
{
    if (m_textIndicator == textIndicator)
        return;

    m_textIndicator = textIndicator;

    // Get rid of the old window.
    closeWindow();

    if (!m_textIndicator)
        return;

    NSRect contentRect = m_textIndicator->textBoundingRectInWindowCoordinates();

    CGFloat horizontalMargin = std::max(dropShadowBlurRadius * 2 + horizontalBorder, contentRect.size.width * 2);
    CGFloat verticalMargin = std::max(dropShadowBlurRadius * 2 + verticalBorder, contentRect.size.height * 2);

    contentRect = NSInsetRect(contentRect, -horizontalMargin, -verticalMargin);
    NSRect windowFrameRect = NSIntegralRect([m_targetView convertRect:contentRect toView:nil]);
    windowFrameRect = [[m_targetView window] convertRectToScreen:windowFrameRect];
    NSRect windowContentRect = [NSWindow contentRectForFrameRect:windowFrameRect styleMask:NSBorderlessWindowMask];

    m_textIndicatorWindow = adoptNS([[NSWindow alloc] initWithContentRect:windowContentRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:NO]);

    [m_textIndicatorWindow setBackgroundColor:[NSColor clearColor]];
    [m_textIndicatorWindow setOpaque:NO];
    [m_textIndicatorWindow setIgnoresMouseEvents:YES];

    m_textIndicatorView = adoptNS([[WebTextIndicatorView alloc] initWithFrame:NSMakeRect(0, 0, [m_textIndicatorWindow frame].size.width, [m_textIndicatorWindow frame].size.height) textIndicator:m_textIndicator margin:NSMakeSize(horizontalMargin, verticalMargin)]);
    [m_textIndicatorWindow setContentView:m_textIndicatorView.get()];

    [[m_targetView window] addChildWindow:m_textIndicatorWindow.get() ordered:NSWindowAbove];
    [m_textIndicatorWindow setReleasedWhenClosed:NO];

    if (m_textIndicator->presentationTransition() != TextIndicatorPresentationTransition::None) {
        [m_textIndicatorView presentWithCompletionHandler:[animationCompletionHandler] {
            animationCompletionHandler();
        }];
    }

    if (fadeOut)
        m_startFadeOutTimer.startOneShot(timeBeforeFadeStarts);
}

void TextIndicatorWindow::closeWindow()
{
    if (!m_textIndicatorWindow)
        return;

    m_startFadeOutTimer.stop();

    [[m_textIndicatorWindow parentWindow] removeChildWindow:m_textIndicatorWindow.get()];
    [m_textIndicatorWindow close];
    m_textIndicatorWindow = nullptr;
}

void TextIndicatorWindow::startFadeOutTimerFired()
{
    RetainPtr<NSWindow> indicatorWindow = m_textIndicatorWindow;
    [m_textIndicatorView hideWithCompletionHandler:[indicatorWindow] {
        [[indicatorWindow parentWindow] removeChildWindow:indicatorWindow.get()];
        [indicatorWindow close];
    }];
}

} // namespace WebCore

#endif // PLATFORM(MAC)
