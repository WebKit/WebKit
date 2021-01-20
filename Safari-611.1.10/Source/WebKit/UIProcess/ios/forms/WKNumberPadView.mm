/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKNumberPadView.h"

#if PLATFORM(WATCHOS)

#import "PepperUICoreSPI.h"
#import "WKNumberPadViewController.h"

#import <WebCore/LocalizedStrings.h>
#import <pal/spi/cocoa/CoreTextSPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

static BOOL isRegularDeviceVariant()
{
    return [UIDevice currentDevice].puic_deviceVariant == PUICDeviceVariantRegular;
}

static CGFloat numberPadButtonSpacing()
{
    return isRegularDeviceVariant() ? 2.5 : 2;
}

static CGFloat numberPadBackgroundInsetAmount()
{
    return isRegularDeviceVariant() ? 1 : 1.25;
}

static CGFloat numberPadLabelFontSize()
{
    return [UIDevice currentDevice].puic_deviceVariant == PUICDeviceVariantCompact ? 24 : 28;
}

const CGFloat numberPadHighlightedWhiteColorValue = 0.38;
const CGFloat numberPadButtonBackgroundViewCornerRadius = 6;
const CGFloat numberPadButtonHighlightScaleFactor = 1.35;
const CGFloat numberPadHighlightAnimationDuration = 0.25;
const CGFloat numberPadNonHighlightedButtonAlpha = 0.5;
const CGFloat numberPadNonVisibleButtonScaleFactor = 0.001;
const CGFloat numberPadHighlightedButtonZPosition = 100;

typedef struct ColumnAndRow {
    NSUInteger column;
    NSUInteger row;
} ColumnAndRow;

static ColumnAndRow columnAndRowForPosition(WKNumberPadButtonPosition position)
{
    if (!position)
        return { 1, 3 };
    if (position == WKNumberPadButtonPositionBottomLeft)
        return { 0, 3 };
    if (position == WKNumberPadButtonPositionBottomRight)
        return { 2, 3 };
    return { (static_cast<NSUInteger>(position) - 1) % 3, (static_cast<NSUInteger>(position) - 1) / 3 };
}

static WKNumberPadButtonPosition positionForColumnAndRow(ColumnAndRow columnAndRow)
{
    NSUInteger column = columnAndRow.column;
    NSUInteger row = columnAndRow.row;
    if (row == 3) {
        if (!column)
            return WKNumberPadButtonPositionBottomLeft;
        if (column == 1)
            return WKNumberPadButtonPosition0;
        return WKNumberPadButtonPositionBottomRight;
    }
    return static_cast<WKNumberPadButtonPosition>(1 + (3 * row + column));
}

static BOOL shouldDrawBackgroundForKey(WKNumberPadKey key, WKNumberPadButtonMode mode)
{
    switch (key) {
    case WKNumberPadKeyAccept:
    case WKNumberPadKeyToggleMode:
    case WKNumberPadKeyNone:
        return NO;
    case WKNumberPadKeyPlus:
        return mode == WKNumberPadButtonModeAlternate;
    default:
        return YES;
    }
}

static UIColor *buttonTitleColorForKey(WKNumberPadKey key)
{
    return key == WKNumberPadKeyAccept ? [UIColor systemBlueColor] : [UIColor whiteColor];
}

static NSString *buttonTitleForKey(WKNumberPadKey key)
{
    switch (key) {
    case WKNumberPadKeyDash:
        return @"-";
    case WKNumberPadKeyAsterisk:
        return @"*";
    case WKNumberPadKeyOctothorpe:
        return @"#";
    case WKNumberPadKeyClosingParenthesis:
        return @")";
    case WKNumberPadKeyOpeningParenthesis:
        return @"(";
    case WKNumberPadKeyPlus:
        return @"+";
    case WKNumberPadKeyToggleMode:
        return @"…";
    case WKNumberPadKeyAccept:
        return WebCore::numberPadOKButtonTitle();
    case WKNumberPadKeyNone:
        return nil;
    default:
        return String::number(key);
    }
}

@interface WKNumberPadButton : UIButton

- (void)setVisible:(BOOL)visible duration:(NSTimeInterval)duration delay:(NSTimeInterval)startDelay;
- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated;

@property (nonatomic) CGFloat backgroundInsetAmount;
@property (nonatomic, readonly, getter=isVisible) BOOL visible;
@property (nonatomic) CGPoint subAnchor;

@property (nonatomic) WKNumberPadButtonMode buttonMode;
@property (nonatomic) WKNumberPadButtonPosition buttonPosition;

@property (nonatomic, readonly) WKNumberPadKey currentKey;
@property (nonatomic) WKNumberPadKey defaultKey;
@property (nonatomic) WKNumberPadKey alternateKey;

@end

@implementation WKNumberPadButton {
    BOOL _visible;
    RetainPtr<UIView> _backgroundView;
    RetainPtr<NSString> _defaultTitle;
    RetainPtr<NSString> _alternateTitle;
    RetainPtr<UIFont> _titleFont;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _buttonMode = WKNumberPadButtonModeDefault;
    _backgroundView = adoptNS([[UIView alloc] init]);
    [_backgroundView setBackgroundColor:[UIColor colorWithWhite:numberPadHighlightedWhiteColorValue alpha:numberPadNonHighlightedButtonAlpha]];
    [_backgroundView setUserInteractionEnabled:NO];
    [self insertSubview:_backgroundView.get() atIndex:0];
    _visible = YES;
    _backgroundInsetAmount = numberPadBackgroundInsetAmount();

    self.titleLabel.adjustsFontSizeToFitWidth = YES;

    return self;
}

- (BOOL)isVisible
{
    return _visible;
}

- (void)setVisible:(BOOL)visible duration:(NSTimeInterval)duration delay:(NSTimeInterval)startDelay
{
    [self setEnabled:visible];
    if (visible == _visible)
        return;

    auto applyBlock = [visible, protectedSelf = retainPtr(self)] {
        [protectedSelf setTransform:visible ? CGAffineTransformIdentity : CGAffineTransformMakeScale(numberPadNonVisibleButtonScaleFactor, numberPadNonVisibleButtonScaleFactor)];
        [protectedSelf titleLabel].alpha = visible ? 1 : 0;
    };

    if (duration > 0)
        [UIView animateWithDuration:duration delay:startDelay options:UIViewAnimationOptionAllowUserInteraction animations:applyBlock completion:nil];
    else
        applyBlock();

    _visible = visible;
}

- (void)setDefaultKey:(WKNumberPadKey)defaultKey
{
    if (_defaultKey == defaultKey)
        return;

    _defaultKey = defaultKey;

    if (self.buttonMode == WKNumberPadButtonModeDefault)
        [self _updateBackgroundView];

    [self setNeedsLayout];
}

- (void)setAlternateKey:(WKNumberPadKey)alternateKey
{
    if (_alternateKey == alternateKey)
        return;

    _alternateKey = alternateKey;

    if (self.buttonMode == WKNumberPadButtonModeAlternate)
        [self _updateBackgroundView];

    [self setNeedsLayout];
}

- (NSString *)defaultTitle
{
    return buttonTitleForKey(self.defaultKey);
}

- (NSString *)alternateTitle
{
    return buttonTitleForKey(self.alternateKey);
}

- (WKNumberPadKey)currentKey
{
    return _buttonMode == WKNumberPadButtonModeAlternate ? self.alternateKey : self.defaultKey;
}

- (void)_updateBackgroundView
{
    [_backgroundView setBackgroundColor:shouldDrawBackgroundForKey(self.currentKey, self.buttonMode) ? [UIColor colorWithWhite:numberPadHighlightedWhiteColorValue alpha:1] : [UIColor clearColor]];
    [_backgroundView setAlpha:self.isHighlighted ? 1 : numberPadNonHighlightedButtonAlpha];
}

- (CGPoint)subAnchor
{
    return [_backgroundView layer].anchorPoint;
}

- (void)setSubAnchor:(CGPoint)subAnchor
{
    [_backgroundView layer].anchorPoint = subAnchor;
}

- (void)setHighlighted:(BOOL)highlighted
{
    [self setHighlighted:highlighted animated:NO];
}

- (void)setHighlighted:(BOOL)highlighted animated:(BOOL)animated
{
    self.layer.zPosition = highlighted ? numberPadHighlightedButtonZPosition : 0;
    [super setHighlighted:highlighted];
    [self setNeedsLayout];
    if (!animated) {
        [self _updateBackgroundView];
        return;
    }

    [UIView animateWithDuration:numberPadHighlightAnimationDuration delay:0 options:UIViewAnimationOptionAllowUserInteraction | UIViewAnimationOptionBeginFromCurrentState animations:[protectedSelf = retainPtr(self)] {
        [protectedSelf layoutIfNeeded];
        [protectedSelf _updateBackgroundView];
    } completion:nil];
}

- (void)setEnabled:(BOOL)enabled
{
    [super setEnabled:enabled];
    self.alpha = enabled ? 1 : numberPadNonHighlightedButtonAlpha;
}

- (void)setButtonMode:(WKNumberPadButtonMode)buttonMode
{
    if (_buttonMode == buttonMode)
        return;

    _buttonMode = buttonMode;

    [self _updateBackgroundView];
    [self setNeedsLayout];
}

- (void)layoutSubviews
{
    CGRect bounds = self.bounds;

    UIView *titleView = self.titleLabel;
    titleView.transform = CGAffineTransformIdentity;

    UIView *imageView = self.imageView;
    imageView.transform = CGAffineTransformIdentity;

    if (_buttonMode == WKNumberPadButtonModeAlternate) {
        BOOL enabled = self.alternateKey != WKNumberPadKeyNone;
        [self setEnabled:enabled];
        // This matches behavior on iOS, wherein switching to symbol keys disables any keys that don't support alternatives, but leaves their titles unchanged.
        [self setTitle:buttonTitleForKey(enabled ? self.alternateKey : self.defaultKey) forState:UIControlStateNormal];
        [self setTitleColor:buttonTitleColorForKey(enabled ? self.alternateKey : self.defaultKey) forState:UIControlStateNormal];
    } else {
        [self setEnabled:self.defaultKey != WKNumberPadKeyNone];
        [self setTitle:self.defaultTitle forState:UIControlStateNormal];
        [self setTitleColor:buttonTitleColorForKey(self.defaultKey) forState:UIControlStateNormal];
    }

    [super layoutSubviews];

    [_backgroundView setTransform:CGAffineTransformIdentity];
    [_backgroundView setFrame:CGRectInset(bounds, _backgroundInsetAmount, _backgroundInsetAmount)];
    [_backgroundView layer].cornerRadius = numberPadButtonBackgroundViewCornerRadius;

    CGAffineTransform transform = self.isHighlighted && shouldDrawBackgroundForKey(self.currentKey, self.buttonMode) ? CGAffineTransformMakeScale(numberPadButtonHighlightScaleFactor, numberPadButtonHighlightScaleFactor) : CGAffineTransformIdentity;
    [_backgroundView setTransform:transform];
    titleView.transform = transform;
    imageView.transform = transform;

    CGRect backgroundBounds = [_backgroundView bounds];
    CGPoint contentCenter = [self convertPoint:CGPointMake(backgroundBounds.size.width / 2, backgroundBounds.size.height / 2) fromView:_backgroundView.get()];
    titleView.center = contentCenter;
    imageView.center = contentCenter;
}

@end

@implementation WKNumberPadView {
    RetainPtr<UITouch> _trackedTouch;
    RetainPtr<WKNumberPadButton> _highlightedButton;
    RetainPtr<UIView> _keypad;
    RetainPtr<UIView> _contentView;
    WeakObjCPtr<WKNumberPadViewController> _controller;
}

- (instancetype)initWithFrame:(CGRect)frame controller:(WKNumberPadViewController *)controller
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _controller = controller;
    _contentView = adoptNS([[UIView alloc] initWithFrame:self.bounds]);
    [_contentView setClipsToBounds:YES];
    [_contentView setBackgroundColor:[UIColor blackColor]];
    [self addSubview:_contentView.get()];
    [self _initKeypad];
    return self;
}

static WKNumberPadKey defaultKeyAtPosition(WKNumberPadButtonPosition position, WKNumberPadInputMode inputMode)
{
    switch (position) {
    case WKNumberPadButtonPositionBottomLeft:
        if (inputMode == WKNumberPadInputModeTelephone)
            return WKNumberPadKeyPlus;
        if (inputMode == WKNumberPadInputModeNumbersOnly)
            return WKNumberPadKeyNone;
        return WKNumberPadKeyToggleMode;
    case WKNumberPadButtonPositionBottomRight:
        return WKNumberPadKeyAccept;
    case WKNumberPadButtonPosition0:
        return WKNumberPadKey0;
    case WKNumberPadButtonPosition1:
        return WKNumberPadKey1;
    case WKNumberPadButtonPosition2:
        return WKNumberPadKey2;
    case WKNumberPadButtonPosition3:
        return WKNumberPadKey3;
    case WKNumberPadButtonPosition4:
        return WKNumberPadKey4;
    case WKNumberPadButtonPosition5:
        return WKNumberPadKey5;
    case WKNumberPadButtonPosition6:
        return WKNumberPadKey6;
    case WKNumberPadButtonPosition7:
        return WKNumberPadKey7;
    case WKNumberPadButtonPosition8:
        return WKNumberPadKey8;
    case WKNumberPadButtonPosition9:
        return WKNumberPadKey9;
    default:
        ASSERT_NOT_REACHED();
        return WKNumberPadKeyNone;
    }
}

static WKNumberPadKey alternateKeyAtPosition(WKNumberPadButtonPosition position)
{
    switch (position) {
    case WKNumberPadButtonPositionBottomLeft:
        return WKNumberPadKeyToggleMode;
    case WKNumberPadButtonPositionBottomRight:
        return WKNumberPadKeyAccept;
    case WKNumberPadButtonPosition0:
        return WKNumberPadKeyDash;
    case WKNumberPadButtonPosition4:
        return WKNumberPadKeyAsterisk;
    case WKNumberPadButtonPosition6:
        return WKNumberPadKeyPlus;
    case WKNumberPadButtonPosition7:
        return WKNumberPadKeyOpeningParenthesis;
    case WKNumberPadButtonPosition8:
        return WKNumberPadKeyOctothorpe;
    case WKNumberPadButtonPosition9:
        return WKNumberPadKeyClosingParenthesis;
    default:
        return WKNumberPadKeyNone;
    }
}

- (WKNumberPadButton *)_buttonForPosition:(WKNumberPadButtonPosition)position
{
    WKNumberPadButton *button = [[[WKNumberPadButton alloc] init] autorelease];
    button.defaultKey = defaultKeyAtPosition(position, [_controller inputMode]);
    button.alternateKey = alternateKeyAtPosition(position);
    button.buttonPosition = position;
    button.titleLabel.font = [UIFont systemFontOfSize:numberPadLabelFontSize() weight:UIFontWeightSemibold design:(NSString *)kCTFontUIFontDesignRounded];
    button.userInteractionEnabled = NO;
    return button;
}

- (void)_initKeypad
{
    UIView *view = _contentView.get();
    _keypad = adoptNS([[UIView alloc] init]);
    [_keypad setUserInteractionEnabled:YES];

    CGFloat xAnchors[] = { 0.0, 0.5, 1.0 };
    CGFloat yAnchors[] = { 0.0, 0.5, 0.5, 1.0 };

    for (unsigned x = 0; x < 3; ++x) {
        for (unsigned y = 0; y < 4; ++y) {
            WKNumberPadButton *button = [self _buttonForPosition:positionForColumnAndRow({ x, y })];
            button.subAnchor = CGPointMake(xAnchors[x], yAnchors[y]);
            [_keypad addSubview:button];
        }
    }
    [view addSubview:_keypad.get()];
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    [_contentView setFrame:self.bounds];

    CGFloat buttonSpacing = numberPadButtonSpacing();
    CGSize buttonSize = CGSizeMake((CGRectGetWidth(self.bounds) - 2 * buttonSpacing) / 3, (CGRectGetHeight(self.bounds) - 3 * buttonSpacing) / 4);

    [_keypad setFrame:self.bounds];
    [[_keypad subviews] enumerateObjectsUsingBlock:^(WKNumberPadButton *button, NSUInteger, BOOL *) {
        if (!button.visible)
            [button setVisible:YES duration:0 delay:0];

        auto columnAndRow = columnAndRowForPosition(button.buttonPosition);
        CGFloat insetAmount = buttonSpacing / 2;
        CGRect baseFrame = CGRectMake(columnAndRow.column * (buttonSize.width + buttonSpacing), columnAndRow.row * (buttonSize.height + buttonSpacing), buttonSize.width, buttonSize.height);
        [button setFrame:CGRectInset(baseFrame, -insetAmount, -insetAmount)];
        [button setBackgroundInsetAmount:insetAmount];
    }];
}

- (WKNumberPadButton *)_numberPadButtonForTouch:(UITouch *)touch
{
    CGPoint pointInViewCoordinates = [touch locationInView:self];
    for (WKNumberPadButton *button in [_keypad subviews]) {
        CGPoint pointInButtonCoordinates = [button convertPoint:pointInViewCoordinates fromView:self];
        if ([button pointInside:pointInButtonCoordinates withEvent:nil])
            return button.enabled ? button : nil;
    }

    return nil;
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    if (_trackedTouch)
        return;

    _trackedTouch = touches.anyObject;
    _highlightedButton = [self _numberPadButtonForTouch:_trackedTouch.get()];
    [_highlightedButton setHighlighted:YES];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        if (touch != _trackedTouch)
            continue;

        WKNumberPadButton *newHighlightedButton = [self _numberPadButtonForTouch:_trackedTouch.get()];
        if (newHighlightedButton == _highlightedButton)
            return;

        [_highlightedButton setHighlighted:NO];
        _highlightedButton = newHighlightedButton;
        [_highlightedButton setHighlighted:YES];
        return;
    }
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        if (touch != _trackedTouch)
            continue;

        _trackedTouch = nil;

        if (!_highlightedButton)
            return;

        auto selectedKey = [_highlightedButton currentKey];
        [_controller didSelectKey:selectedKey];
        [_highlightedButton setHighlighted:NO animated:YES];

        if (selectedKey == WKNumberPadKeyToggleMode || [_highlightedButton buttonMode] == WKNumberPadButtonModeAlternate)
            [self toggleAlternateKeys];

        _highlightedButton = nil;
        return;
    }
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for (UITouch *touch in touches) {
        if (touch != _trackedTouch)
            continue;

        _trackedTouch = nil;
        [_highlightedButton setHighlighted:NO animated:YES];
        _highlightedButton = nil;
        return;
    }
}

- (void)toggleAlternateKeys
{
    for (WKNumberPadButton *button in [_keypad subviews]) {
        if (button.buttonMode == WKNumberPadButtonModeAlternate)
            button.buttonMode = WKNumberPadButtonModeDefault;
        else
            button.buttonMode = WKNumberPadButtonModeAlternate;
    }
}

@end

#endif // PLATFORM(WATCHOS)
