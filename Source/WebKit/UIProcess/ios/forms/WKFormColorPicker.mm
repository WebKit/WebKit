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
#include "WKFormColorPicker.h"

#if ENABLE(INPUT_TYPE_COLOR) && PLATFORM(IOS_FAMILY)

#import "AssistedNodeInformation.h"
#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WebPageProxy.h"

#import <WebCore/Color.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(PencilKit)
SOFT_LINK_CLASS(PencilKit, PKColorMatrixView)

static const CGFloat additionalKeyboardAffordance = 80;
static const CGFloat colorSelectionIndicatorBorderWidth = 4;
static const CGFloat colorSelectionIndicatorCornerRadius = 9;
static const CGFloat pickerWidthForPopover = 280;
static const CGFloat topColorMatrixPadding = 5;
#if ENABLE(DATALIST_ELEMENT)
static const size_t maxColorSuggestions = 12;
#endif

using namespace WebKit;

#pragma mark - PKColorMatrixView

@interface PKColorMatrixView
+ (NSArray<NSArray<UIColor *> *> *)defaultColorMatrix;
@end

#pragma mark - WKColorButton

@interface WKColorButton : UIButton
@property (nonatomic, strong) UIColor *color;

+ (instancetype)colorButtonWithColor:(UIColor *)color;
@end

@implementation WKColorButton

+ (instancetype)colorButtonWithColor:(UIColor *)color
{
    WKColorButton *colorButton = [WKColorButton buttonWithType:UIButtonTypeCustom];
    colorButton.color = color;
    colorButton.backgroundColor = color;
    return colorButton;
}

@end

#pragma mark - WKColorMatrixView

@interface WKColorMatrixView : UIView {
    RetainPtr<NSArray<NSArray<UIColor *> *>> _colorMatrix;
    RetainPtr<NSArray<NSArray<WKColorButton *> *>> _colorButtons;
}

@property (nonatomic, weak) id<WKColorMatrixViewDelegate> delegate;

- (instancetype)initWithFrame:(CGRect)frame colorMatrix:(NSArray<NSArray<UIColor *> *> *)matrix;
@end

@implementation WKColorMatrixView

- (instancetype)initWithFrame:(CGRect)frame
{
    return [self initWithFrame:frame colorMatrix:[getPKColorMatrixViewClass() defaultColorMatrix]];
}

- (instancetype)initWithFrame:(CGRect)frame colorMatrix:(NSArray<NSArray<UIColor *> *> *)matrix
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    _colorMatrix = matrix;

    NSMutableArray *colorButtons = [NSMutableArray array];
    for (NSUInteger row = 0; row < [_colorMatrix count]; row++) {
        NSMutableArray *buttons = [NSMutableArray array];
        for (NSUInteger col = 0; col < [_colorMatrix.get()[0] count]; col++) {
            WKColorButton *button = [WKColorButton colorButtonWithColor:_colorMatrix.get()[row][col]];
            [button addTarget:self action:@selector(colorButtonTapped:) forControlEvents:UIControlEventTouchUpInside];
            [buttons addObject:button];
            [self addSubview:button];
        }
        RetainPtr<NSArray> colorButtonsRow = buttons;
        [colorButtons addObject:colorButtonsRow.get()];
    }
    _colorButtons = colorButtons;

    return self;
}

- (void)layoutSubviews
{
    [super layoutSubviews];

    CGSize matrixSize = self.bounds.size;
    CGFloat numRows = [_colorMatrix count];
    CGFloat numCols = [_colorMatrix.get()[0] count];
    CGFloat buttonHeight = matrixSize.height / numRows;
    CGFloat buttonWidth = matrixSize.width / numCols;

    for (NSUInteger row = 0; row < numRows; row++) {
        for (NSUInteger col = 0; col < numCols; col++) {
            WKColorButton *button = _colorButtons.get()[row][col];
            button.frame = CGRectMake(col * buttonWidth, row * buttonHeight, buttonWidth, buttonHeight);
        }
    }

    [self.delegate colorMatrixViewDidLayoutSubviews:self];
}

- (void)colorButtonTapped:(WKColorButton *)colorButton
{
    [self.delegate colorMatrixView:self didTapColorButton:colorButton];
}

@end

#pragma mark - WKFormColorPicker

@implementation WKColorPicker {
    WKContentView *_view;
    RetainPtr<UIView> _colorPicker;

    RetainPtr<UIView> _colorSelectionIndicator;
    RetainPtr<CAShapeLayer> _colorSelectionIndicatorBorder;

    RetainPtr<WKColorMatrixView> _topColorMatrix;
    RetainPtr<WKColorMatrixView> _mainColorMatrix;
    WeakObjCPtr<WKColorButton> _selectedColorButton;

    RetainPtr<UIPanGestureRecognizer> _colorPanGR;
}

+ (NSArray<NSArray<UIColor *> *> *)defaultTopColorMatrix
{
    return @[ @[ UIColor.redColor, UIColor.orangeColor, UIColor.yellowColor, UIColor.greenColor, UIColor.cyanColor, UIColor.blueColor, UIColor.magentaColor, UIColor.purpleColor, UIColor.brownColor, UIColor.whiteColor, UIColor.grayColor, UIColor.blackColor ] ];
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    _view = view;

    CGSize colorPickerSize;
    if (currentUserInterfaceIdiomIsPad())
        colorPickerSize = CGSizeMake(pickerWidthForPopover, pickerWidthForPopover);
    else {
        CGSize keyboardSize = [UIKeyboard defaultSizeForInterfaceOrientation:[UIApp interfaceOrientation]];
        colorPickerSize = CGSizeMake(keyboardSize.width, keyboardSize.height + additionalKeyboardAffordance);
    }

    _colorPicker = adoptNS([[UIView alloc] initWithFrame:CGRectMake(0, 0, colorPickerSize.width, colorPickerSize.height)]);

    CGFloat totalRows = [[getPKColorMatrixViewClass() defaultColorMatrix] count] + 1;
    CGFloat swatchHeight = (colorPickerSize.height - topColorMatrixPadding) / totalRows;

    _mainColorMatrix = adoptNS([[WKColorMatrixView alloc] initWithFrame:CGRectMake(0, swatchHeight + topColorMatrixPadding, colorPickerSize.width, swatchHeight * (totalRows - 1))]);
    [_mainColorMatrix setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [_mainColorMatrix setDelegate:self];
    [_colorPicker addSubview:_mainColorMatrix.get()];

    NSArray<NSArray<UIColor *> *> *topColorMatrix = [[self class] defaultTopColorMatrix];

#if ENABLE(DATALIST_ELEMENT)
    size_t numColorSuggestions = view.assistedNodeInformation.suggestedColors.size();
    if (numColorSuggestions) {
        NSMutableArray<UIColor *> *colors = [NSMutableArray array];
        for (size_t i = 0; i < std::min(numColorSuggestions, maxColorSuggestions); i++) {
            WebCore::Color color = view.assistedNodeInformation.suggestedColors[i];
            [colors addObject:[UIColor colorWithCGColor:cachedCGColor(color)]];
        }
        topColorMatrix = @[ colors ];
    }
#endif

    _topColorMatrix = adoptNS([[WKColorMatrixView alloc] initWithFrame:CGRectMake(0, 0, colorPickerSize.width, swatchHeight) colorMatrix:topColorMatrix]);
    [_topColorMatrix setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    [_topColorMatrix setDelegate:self];
    [_colorPicker addSubview:_topColorMatrix.get()];

    _colorSelectionIndicator = adoptNS([[UIView alloc] initWithFrame:CGRectZero]);
    [_colorSelectionIndicator setClipsToBounds:YES];
    [_colorPicker addSubview:_colorSelectionIndicator.get()];

    _colorSelectionIndicatorBorder = adoptNS([[CAShapeLayer alloc] init]);
    [_colorSelectionIndicatorBorder setLineWidth:colorSelectionIndicatorBorderWidth];
    [_colorSelectionIndicatorBorder setStrokeColor:[UIColor whiteColor].CGColor];
    [_colorSelectionIndicatorBorder setFillColor:[UIColor clearColor].CGColor];
    [[_colorSelectionIndicator layer] addSublayer:_colorSelectionIndicatorBorder.get()];

    _colorPanGR = adoptNS([[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(didPanColors:)]);
    [_colorPicker addGestureRecognizer:_colorPanGR.get()];

    return self;
}

- (void)drawSelectionIndicatorForColorButton:(WKColorButton *)colorButton
{
    _selectedColorButton = colorButton;

    CGRect frame = [_colorPicker convertRect:colorButton.bounds fromView:colorButton];
    [_colorSelectionIndicator setFrame:frame];

    UIRectCorner roundCorner = 0;
    if (currentUserInterfaceIdiomIsPad()) {
        CGRect colorPickerBounds = [_colorPicker bounds];

        bool minXEqual = std::abs(CGRectGetMinX(frame) - CGRectGetMinX(colorPickerBounds)) < FLT_EPSILON;
        bool minYEqual = std::abs(CGRectGetMinY(frame) - CGRectGetMinY(colorPickerBounds)) < FLT_EPSILON;
        bool maxXEqual = std::abs(CGRectGetMaxX(frame) - CGRectGetMaxX(colorPickerBounds)) < FLT_EPSILON;
        bool maxYEqual = std::abs(CGRectGetMaxY(frame) - CGRectGetMaxY(colorPickerBounds)) < FLT_EPSILON;

        // On iPad, round the corners of the indicator that border the corners of the picker, to match the popover.
        if (minXEqual && minYEqual)
            roundCorner |= UIRectCornerTopLeft;
        if (maxXEqual && minYEqual)
            roundCorner |= UIRectCornerTopRight;
        if (minXEqual && maxYEqual)
            roundCorner |= UIRectCornerBottomLeft;
        if (maxXEqual && maxYEqual)
            roundCorner |= UIRectCornerBottomRight;
    }

    UIBezierPath *cornerMaskPath = [UIBezierPath bezierPathWithRoundedRect:colorButton.bounds byRoundingCorners:roundCorner cornerRadii:CGSizeMake(colorSelectionIndicatorCornerRadius, colorSelectionIndicatorCornerRadius)];
    [_colorSelectionIndicatorBorder setFrame:colorButton.bounds];
    [_colorSelectionIndicatorBorder setPath:cornerMaskPath.CGPath];
}

- (void)setControlValueFromUIColor:(UIColor *)uiColor
{
    WebCore::Color color(uiColor.CGColor);
    [_view page]->setAssistedNodeValue(color.serialized());
}

#pragma mark WKFormControl

- (UIView *)controlView
{
    return _colorPicker.get();
}

- (void)controlBeginEditing
{
}

- (void)controlEndEditing
{
}

#pragma mark WKColorMatrixViewDelegate

- (void)colorMatrixViewDidLayoutSubviews:(WKColorMatrixView *)matrixView
{
    if ([_selectedColorButton superview] == matrixView)
        [self drawSelectionIndicatorForColorButton:_selectedColorButton.get().get()];
}

- (void)colorMatrixView:(WKColorMatrixView *)matrixView didTapColorButton:(WKColorButton *)colorButton
{
    if (_selectedColorButton.get().get() == colorButton)
        return;

    [self drawSelectionIndicatorForColorButton:colorButton];
    [self setControlValueFromUIColor:colorButton.color];
}

#pragma mark UIPanGestureRecognizer

- (void)didPanColors:(UIGestureRecognizer *)gestureRecognizer
{
    CGPoint location = [gestureRecognizer locationInView:gestureRecognizer.view];
    UIView *view = [gestureRecognizer.view hitTest:location withEvent:nil];
    if ([view isKindOfClass:[WKColorButton class]]) {
        WKColorButton *colorButton = (WKColorButton *)view;
        if (_selectedColorButton.get().get() == colorButton)
            return;

        [self drawSelectionIndicatorForColorButton:colorButton];
        [self setControlValueFromUIColor:colorButton.color];
    }
}

@end

#endif // ENABLE(INPUT_TYPE_COLOR) && PLATFORM(IOS_FAMILY)
