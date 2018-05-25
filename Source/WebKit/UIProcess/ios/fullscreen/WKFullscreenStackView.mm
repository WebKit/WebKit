/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(FULLSCREEN_API) && PLATFORM(IOS)
#include "WKFullscreenStackView.h"

#import "UIKitSPI.h"
#import <QuartzCore/CAFilter.h>
#import <UIKit/UIVisualEffectView.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>

static NSArray<UIVisualEffect *>* reducedTransparencyEffects()
{
    static NeverDestroyed<RetainPtr<NSArray<UIVisualEffect *>>> effects;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        effects.get() = @[[UIVisualEffect effectCompositingColor:[UIColor colorWithRed:(43.0 / 255.0) green:(46.0 / 255.0) blue:(48.0 / 255.0) alpha:1.0] withMode:UICompositingModeNormal alpha:1.0]];
    });
    return effects.get().get();
}

static NSArray<UIVisualEffect *>* normalTransparencyEffects()
{
    static NeverDestroyed<RetainPtr<NSArray<UIVisualEffect *>>> effects;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        effects.get() = @[
            [UIVisualEffect effectCompositingColor:[UIColor blackColor] withMode:UICompositingModeNormal alpha:0.55],
            [UIBlurEffect effectWithBlurRadius:UIRoundToScreenScale(17.5, [UIScreen mainScreen])],
            [UIColorEffect colorEffectSaturate:1.8],
            [UIVisualEffect effectCompositingColor:[UIColor whiteColor] withMode:UICompositingModeNormal alpha:0.14]
        ];
    });
    return effects.get().get();
}

@interface UIVisualEffectView (WebKitPrivate)
@property (nonatomic, readwrite, copy) NSArray<UIVisualEffect *> *backgroundEffects;
@end

@interface WKFullscreenStackView ()
@property (nonatomic, readonly) UIStackView *_stackView;
@property (nonatomic, readonly) UIVisualEffectView *_visualEffectView;
@property (nonatomic, assign) UIVisualEffectView *secondaryMaterialOverlayView;
@property (nonatomic, retain) NSArray<NSLayoutConstraint *> *secondaryMaterialOverlayViewConstraints;
@end

@implementation WKFullscreenStackView

#pragma mark - Class Methods
+ (NSArray<UIVisualEffect *> *)baseEffects
{
    if (UIAccessibilityIsReduceTransparencyEnabled())
        return reducedTransparencyEffects();
    return normalTransparencyEffects();
}

+ (void)configureView:(UIView *)view forTintEffectWithColor:(UIColor *)tintColor filterType:(NSString *)filterType
{
    if ([view isKindOfClass:[UILabel class]]) {
        [(UILabel *)view setTextColor:tintColor];
        [view setTintColor:tintColor];
        [[view layer] setCompositingFilter:[CAFilter filterWithType:kCAFilterPlusL]];
        return;
    }

    _UIVisualEffectTintLayerConfig *tintLayerConfig = [_UIVisualEffectTintLayerConfig layerWithTintColor:tintColor filterType:filterType];
    [[[_UIVisualEffectConfig configWithContentConfig:tintLayerConfig] contentConfig] configureLayerView:view];
}

+ (void)configureView:(UIView *)view withBackgroundFillOfColor:(UIColor *)fillColor opacity:(CGFloat)opacity filter:(NSString *)filter
{
    _UIVisualEffectLayerConfig *baseLayerConfig = [_UIVisualEffectLayerConfig layerWithFillColor:fillColor opacity:opacity filterType:filter];
    [[[_UIVisualEffectConfig configWithContentConfig:baseLayerConfig] contentConfig] configureLayerView:view];
}

+ (UIVisualEffectView *)secondaryMaterialOverlayView
{
    UIVisualEffectView *secondaryMaterialOverlayView = [[UIVisualEffectView alloc] initWithEffect:nil];
    [secondaryMaterialOverlayView setUserInteractionEnabled:NO];
    [secondaryMaterialOverlayView setBackgroundEffects:@[[UIVisualEffect effectCompositingColor:[UIColor blackColor] withMode:UICompositingModePlusDarker alpha:0.06]]];
    return [secondaryMaterialOverlayView autorelease];
}

#pragma mark - External Interface

+ (void)applyPrimaryGlyphTintToView:(UIView *)view
{
    [self configureView:view forTintEffectWithColor:[UIColor colorWithWhite:1.0 alpha:0.75] filterType:kCAFilterPlusL];
}

+ (void)applySecondaryGlyphTintToView:(UIView *)view
{
    [self configureView:view forTintEffectWithColor:[UIColor colorWithWhite:1.0 alpha:0.55] filterType:kCAFilterPlusL];
}

- (instancetype)initWithArrangedSubviews:(NSArray<UIView *> *)arrangedSubviews axis:(UILayoutConstraintAxis)axis
{
    self = [self initWithFrame:CGRectMake(0, 0, 100, 100)];

    if (!self)
        return nil;

    [self setClipsToBounds:YES];

    _visualEffectView = [[UIVisualEffectView alloc] initWithEffect:nil];
    [self addSubview:_visualEffectView];
    [_visualEffectView setBackgroundEffects:[[self class] baseEffects]];

    _stackView = [[UIStackView alloc] initWithArrangedSubviews:arrangedSubviews];
    [_stackView setAxis:axis];
    [_stackView setLayoutMarginsRelativeArrangement:YES];
    [_stackView setInsetsLayoutMarginsFromSafeArea:NO];
    [self insertSubview:_stackView above:_visualEffectView];

    [self _setArrangedSubviews:arrangedSubviews axis:axis];

    return self;
}

- (void)dealloc
{
    [_targetViewForSecondaryMaterialOverlay release];
    [_visualEffectView release];
    [_stackView release];
    [_secondaryMaterialOverlayViewConstraints release];
    [super dealloc];
}

- (void)setTargetViewForSecondaryMaterialOverlay:(UIView *)targetViewForSecondaryMaterialOverlay
{
    if (_targetViewForSecondaryMaterialOverlay == targetViewForSecondaryMaterialOverlay)
        return;

    _targetViewForSecondaryMaterialOverlay = [targetViewForSecondaryMaterialOverlay retain];
    [self setNeedsUpdateConstraints];
}

- (UIView *)contentView
{
    return [_visualEffectView contentView];
}

#pragma mark - Internal Interface

@synthesize _stackView=_stackView;
@synthesize _visualEffectView=_visualEffectView;

- (void)_setArrangedSubviews:(NSArray<UIView *> *)arrangedSubviews axis:(UILayoutConstraintAxis)axis
{
    for (UIView *view in [_stackView arrangedSubviews])
        [view removeFromSuperview];

    for (UIView *view in arrangedSubviews)
        [_stackView addArrangedSubview:view];

    [_stackView setAxis:axis];
}

#pragma mark - UIView Overrides

- (void)setBounds:(CGRect)bounds
{
    CGSize oldSize = [self bounds].size;

    [super setBounds:bounds];

    if (!CGSizeEqualToSize(oldSize, bounds.size))
        [self _setContinuousCornerRadius:((CGRectGetHeight(bounds) > 40.0) ? 16.0 : 8.0)];
}

- (void)updateConstraints
{
    if ([_stackView translatesAutoresizingMaskIntoConstraints] || [_visualEffectView translatesAutoresizingMaskIntoConstraints]) {
        [_visualEffectView setTranslatesAutoresizingMaskIntoConstraints:NO];
        [_stackView setTranslatesAutoresizingMaskIntoConstraints:NO];

        NSArray<NSLayoutConstraint *> *constraints = @[
            [[_visualEffectView leadingAnchor] constraintEqualToAnchor:[self leadingAnchor]],
            [[_visualEffectView topAnchor] constraintEqualToAnchor:[self topAnchor]],
            [[_visualEffectView trailingAnchor] constraintEqualToAnchor:[self trailingAnchor]],
            [[_visualEffectView bottomAnchor] constraintEqualToAnchor:[self bottomAnchor]],
            [[_stackView leadingAnchor] constraintEqualToAnchor:[self leadingAnchor]],
            [[_stackView topAnchor] constraintEqualToAnchor:[self topAnchor]],
            [[_stackView trailingAnchor] constraintEqualToAnchor:[self trailingAnchor]],
            [[_stackView bottomAnchor] constraintEqualToAnchor:[self bottomAnchor]],
        ];

        for (NSLayoutConstraint *constraint in constraints)
            [constraint setPriority:(UILayoutPriorityRequired - 1)];

        [NSLayoutConstraint activateConstraints:constraints];
    }

    if ([[self targetViewForSecondaryMaterialOverlay] isDescendantOfView:self]) {
        if (!_secondaryMaterialOverlayView) {
            [self setSecondaryMaterialOverlayView:[[self class] secondaryMaterialOverlayView]];
            [self addSubview:[self secondaryMaterialOverlayView]];
        }

        if ([[self secondaryMaterialOverlayView] isDescendantOfView:self] && !_secondaryMaterialOverlayViewConstraints) {
            [[self secondaryMaterialOverlayView] setTranslatesAutoresizingMaskIntoConstraints:NO];
            NSArray<NSLayoutConstraint *> *constraints = @[
                [[_secondaryMaterialOverlayView centerXAnchor] constraintEqualToAnchor:[_targetViewForSecondaryMaterialOverlay centerXAnchor]],
                [[_secondaryMaterialOverlayView centerYAnchor] constraintEqualToAnchor:[_targetViewForSecondaryMaterialOverlay centerYAnchor]],
                [[_secondaryMaterialOverlayView widthAnchor] constraintEqualToAnchor:[_targetViewForSecondaryMaterialOverlay widthAnchor]],
                [[_secondaryMaterialOverlayView heightAnchor] constraintEqualToAnchor:[_targetViewForSecondaryMaterialOverlay heightAnchor]]
            ];
            [self setSecondaryMaterialOverlayViewConstraints:constraints];
            [NSLayoutConstraint activateConstraints:[self secondaryMaterialOverlayViewConstraints]];
        }
    } else
        [_secondaryMaterialOverlayView removeFromSuperview];

    [super updateConstraints];
}

@end

#endif // ENABLE(FULLSCREEN_API) && PLATFORM(IOS)
