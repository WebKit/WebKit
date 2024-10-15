/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

WTF_EXTERN_C_BEGIN
typedef struct __GSKeyboard* GSKeyboardRef;
WTF_EXTERN_C_END

#if USE(APPLE_INTERNAL_SDK)

#import <Foundation/NSGeometry.h>
#import <UIKit/NSParagraphStyle_Private.h>
#import <UIKit/NSTextAlternatives.h>
#import <UIKit/NSTextAttachment_Private.h>
#import <UIKit/NSTextList.h>
#import <UIKit/UIApplicationSceneConstants.h>
#import <UIKit/UIApplication_Private.h>
#import <UIKit/UIColor_Private.h>
#import <UIKit/UIFocusRingStyle.h>
#import <UIKit/UIFont_Private.h>
#import <UIKit/UIInterface_Private.h>
#import <UIKit/UIPasteboard_Private.h>
#import <UIKit/UIScreen_Private.h>
#import <UIKit/UITextEffectsWindow.h>
#import <UIKit/UIViewController_Private.h>
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#import <UIKit/NSItemProvider+UIKitAdditions_Private.h>

@interface UIApplication ()
+ (UIApplicationSceneClassicMode)_classicMode;
- (GSKeyboardRef)_hardwareKeyboard;
- (CGFloat)_iOSMacScale;
@end


#else // USE(APPLE_INTERNAL_SDK)

#import <UIKit/UIKit.h>

#if ENABLE(DRAG_SUPPORT)
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#endif

typedef NS_ENUM(NSInteger, NSRectEdge);

typedef NS_ENUM(NSInteger, UIApplicationSceneClassicMode) {
    UIApplicationSceneClassicModeOriginalPad = 4,
};

typedef enum {
    UIFontTraitPlain       = 0x00000000,
    UIFontTraitItalic      = 0x00000001, // 1 << 0
    UIFontTraitBold        = 0x00000002, // 1 << 1
    UIFontTraitThin        = (1 << 2),
    UIFontTraitLight       = (1 << 3),
    UIFontTraitUltraLight  = (1 << 4)
} UIFontTrait;

@interface NSTextAttachment ()
- (id)initWithFileWrapper:(NSFileWrapper *)fileWrapper;
@property (strong) NSString *accessibilityLabel;
@end

@interface NSTextAlternatives : NSObject
@property (readonly) NSArray<NSString *> *alternativeStrings;
@end

@interface UIApplication ()
- (BOOL)_isClassic;
+ (UIApplicationSceneClassicMode)_classicMode;
- (GSKeyboardRef)_hardwareKeyboard;
- (void)_setIdleTimerDisabled:(BOOL)disabled forReason:(NSString *)reason;
@end

static const UIUserInterfaceIdiom UIUserInterfaceIdiomWatch = (UIUserInterfaceIdiom)4;

@interface UIColor ()

+ (UIColor *)systemBlueColor;
+ (UIColor *)systemBrownColor;
+ (UIColor *)systemGrayColor;
+ (UIColor *)systemGreenColor;
+ (UIColor *)systemIndigoColor;
+ (UIColor *)systemOrangeColor;
+ (UIColor *)systemPinkColor;
+ (UIColor *)systemPurpleColor;
+ (UIColor *)systemRedColor;
+ (UIColor *)systemTealColor;
+ (UIColor *)systemYellowColor;

+ (UIColor *)systemBackgroundColor;
+ (UIColor *)secondarySystemBackgroundColor;
+ (UIColor *)tertiarySystemBackgroundColor;

+ (UIColor *)systemFillColor;
+ (UIColor *)secondarySystemFillColor;
+ (UIColor *)tertiarySystemFillColor;
+ (UIColor *)quaternarySystemFillColor;

+ (UIColor *)systemGroupedBackgroundColor;
+ (UIColor *)secondarySystemGroupedBackgroundColor;
+ (UIColor *)tertiarySystemGroupedBackgroundColor;

+ (UIColor *)labelColor;
+ (UIColor *)secondaryLabelColor;
+ (UIColor *)tertiaryLabelColor;
+ (UIColor *)quaternaryLabelColor;

+ (UIColor *)placeholderTextColor;

+ (UIColor *)separatorColor;
+ (UIColor *)opaqueSeparatorColor;

+ (UIColor *)_disambiguated_due_to_CIImage_colorWithCGColor:(CGColorRef)cgColor;

- (CGFloat)alphaComponent;

@end

@interface UIFont ()

+ (UIFont *)fontWithFamilyName:(NSString *)familyName traits:(UIFontTrait)traits size:(CGFloat)fontSize;

@end

typedef NS_ENUM(NSInteger, _UIDataOwner) {
    _UIDataOwnerUndefined,
    _UIDataOwnerUser,
    _UIDataOwnerEnterprise,
    _UIDataOwnerShared,
};

@interface UIPasteboard ()
+ (void)_performAsDataOwner:(_UIDataOwner)dataOwner block:(void(^ NS_NOESCAPE)(void))block;
@end

@interface UIScreen ()

@property (nonatomic, readonly) CGRect _referenceBounds;

@end

@interface UIFocusRingStyle : NSObject
+ (CGFloat)borderThickness;
+ (CGFloat)maxAlpha;
+ (CGFloat)alphaThreshold;
@end

@interface UIApplicationRotationFollowingWindow : UIWindow
@end

@interface UIAutoRotatingWindow : UIApplicationRotationFollowingWindow
@end

@interface UITextEffectsWindow : UIAutoRotatingWindow
+ (UITextEffectsWindow *)sharedTextEffectsWindowForWindowScene:(UIWindowScene *)windowScene;
@end

#endif // USE(APPLE_INTERNAL_SDK)

@interface UIColor (IPI)
+ (UIColor *)tableCellDefaultSelectionTintColor;
@end

typedef NS_ENUM(NSUInteger, NSTextBlockValueType) {
    NSTextBlockAbsoluteValueType    = 0, // Absolute value in points
    NSTextBlockPercentageValueType  = 1 // Percentage value (out of 100)
};

typedef NS_ENUM(NSUInteger, NSTextBlockDimension) {
    NSTextBlockWidth            = 0,
    NSTextBlockMinimumWidth     = 1,
    NSTextBlockMaximumWidth     = 2,
    NSTextBlockHeight           = 4,
    NSTextBlockMinimumHeight    = 5,
    NSTextBlockMaximumHeight    = 6
};

typedef NS_ENUM(NSInteger, NSTextBlockLayer) {
    NSTextBlockPadding  = -1,
    NSTextBlockBorder   =  0,
    NSTextBlockMargin   =  1
};

typedef NS_ENUM(NSUInteger, NSTextTableLayoutAlgorithm) {
    NSTextTableAutomaticLayoutAlgorithm = 0,
    NSTextTableFixedLayoutAlgorithm     = 1
};

typedef NS_ENUM(NSUInteger, NSTextBlockVerticalAlignment) {
    NSTextBlockTopAlignment         = 0,
    NSTextBlockMiddleAlignment      = 1,
    NSTextBlockBottomAlignment      = 2,
    NSTextBlockBaselineAlignment    = 3
};

typedef NS_ENUM(NSUInteger, NSTextTabType) {
    NSLeftTabStopType = 0,
    NSRightTabStopType,
    NSCenterTabStopType,
    NSDecimalTabStopType
};

@interface NSColor : UIColor
+ (id)colorWithCalibratedRed:(CGFloat)red green:(CGFloat)green blue:(CGFloat)blue alpha:(CGFloat)alpha;
@end

@interface NSTextTab ()
- (id)initWithType:(NSTextTabType)type location:(CGFloat)loc;
- (instancetype)initWithTextAlignment:(NSTextAlignment)alignment location:(CGFloat)loc options:(NSDictionary<NSTextTabOptionKey, id> *)options;
@end

@interface NSTextBlock : NSObject
- (void)setValue:(CGFloat)val type:(NSTextBlockValueType)type forDimension:(NSTextBlockDimension)dimension;
- (void)setBackgroundColor:(UIColor *)color;
- (UIColor *)backgroundColor;
- (void)setBorderColor:(UIColor *)color; // Convenience method sets all edges at once
- (void)setBorderColor:(UIColor *)color forEdge:(NSRectEdge)edge;
- (void)setVerticalAlignment:(NSTextBlockVerticalAlignment)alignment;
- (void)setWidth:(CGFloat)val type:(NSTextBlockValueType)type forLayer:(NSTextBlockLayer)layer edge:(NSRectEdge)edge;
- (CGFloat)valueForDimension:(NSTextBlockDimension)dimension;
- (CGFloat)widthForLayer:(NSTextBlockLayer)layer edge:(NSRectEdge)edge;
- (NSTextBlockVerticalAlignment)verticalAlignment;
@end

@interface NSTextBlock (Internal)
- (void)_takeValuesFromTextBlock:(NSTextBlock *)other;
@end

@interface NSTextTable : NSTextBlock
- (NSColor *)borderColorForEdge:(NSRectEdge)edge;
- (NSTextTableLayoutAlgorithm)layoutAlgorithm;
- (void)setNumberOfColumns:(NSUInteger)numCols;
- (void)setCollapsesBorders:(BOOL)flag;
- (void)setHidesEmptyCells:(BOOL)flag;
- (void)setLayoutAlgorithm:(NSTextTableLayoutAlgorithm)algorithm;
- (NSUInteger)numberOfColumns;
- (void)release;
- (BOOL)collapsesBorders;
- (BOOL)hidesEmptyCells;
@end

@interface NSTextTableBlock : NSTextBlock
- (NSColor *)borderColorForEdge:(NSRectEdge)edge;
- (id)initWithTable:(NSTextTable *)table startingRow:(NSInteger)row rowSpan:(NSInteger)rowSpan startingColumn:(NSInteger)col columnSpan:(NSInteger)colSpan; // Designated initializer
- (NSTextTable *)table;
- (NSInteger)startingColumn;
- (NSInteger)startingRow;
- (NSUInteger)numberOfColumns;
- (NSInteger)columnSpan;
- (NSInteger)rowSpan;
@end

@interface NSParagraphStyle ()
- (NSInteger)headerLevel;
- (NSArray<NSTextBlock *> *)textBlocks;
@end

@interface NSMutableParagraphStyle ()
- (void)setHeaderLevel:(NSInteger)level;
- (void)setTextBlocks:(NSArray<NSTextBlock *> *)array;
@end

#endif // PLATFORM(IOS_FAMILY)
