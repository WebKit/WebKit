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

#if PLATFORM(WATCHOS)

#import "UIKitSPI.h"

#if USE(APPLE_INTERNAL_SDK)

#import <PepperUICore/PUICActionController.h>
#import <PepperUICore/PUICActionController_Private.h>
#import <PepperUICore/PUICActionGroup.h>
#import <PepperUICore/PUICApplication_Private.h>
#import <PepperUICore/PUICCrownInputSequencer.h>
#import <PepperUICore/PUICCrownInputSequencer_Private.h>
#import <PepperUICore/PUICPickerView.h>
#import <PepperUICore/PUICPickerView_Private.h>
#import <PepperUICore/PUICQuickboardArouetViewController.h>
#import <PepperUICore/PUICQuickboardLanguageController.h>
#import <PepperUICore/PUICQuickboardListViewController.h>
#import <PepperUICore/PUICQuickboardListViewControllerSubclass.h>
#import <PepperUICore/PUICQuickboardListViewSpecs.h>
#import <PepperUICore/PUICQuickboardViewController.h>
#import <PepperUICore/PUICQuickboardViewController_Private.h>
#import <PepperUICore/PUICResources.h>
#import <PepperUICore/PUICStatusBarAppContextView.h>
#import <PepperUICore/PUICTableView.h>
#import <PepperUICore/PUICTableViewCell.h>
#import <PepperUICore/UIDevice+PUICAdditions.h>
#import <PepperUICore/UIScrollView+PUICAdditionsPrivate.h>

#if HAVE(QUICKBOARD_COLLECTION_VIEWS)
#import <PepperUICore/PUICQuickboardListCollectionViewItemCell.h>
#endif

#else // USE(APPLE_INTERNAL_SDK)

NS_ASSUME_NONNULL_BEGIN

extern NSString * const PUICStatusBarNavigationBackButtonPressedNotification;
extern NSString * const PUICStatusBarTitleTappedNotification;

typedef NS_ENUM(NSInteger, PUICCrownInputScrollDirection) {
    PUICCrownInputScrollDirectionNone,
    PUICCrownInputScrollDirectionHorizontal,
    PUICCrownInputScrollDirectionVertical,
};

typedef NS_ENUM(NSInteger, PUICDeviceVariant) {
    PUICDeviceVariantCompact = 0,
    PUICDeviceVariantRegular = 1,
    PUICDeviceVariant394h = 2,
    PUICDeviceVariant448h = 3,
};

@interface UIDevice (PUICAdditions)
- (PUICDeviceVariant)puic_deviceVariant;
@end

typedef NS_ENUM(NSInteger, PUICDictationMode) {
    PUICDictationModeText,
    PUICDictationModeTextAndAudio,
    PUICDictationModeAudio,
    PUICDictationModePerson,
    PUICDictationModeLocation
};

@protocol PUICApplicationStatusBarProperties <NSObject>
@property (nonatomic, copy) NSString *title;
@property (nonatomic, strong) UIColor *titleColor;
@property (nonatomic, assign, getter=isTitleInteractive) bool titleInteractive;
@property (nonatomic, assign) bool showNavigationUI;
@property (nonatomic, assign) bool navUIBackButtonDisabled;
- (BOOL) commitChangesAnimated:(BOOL) animated;
@end

@protocol PUICStatusBarAppContextViewDataSource <NSObject>
@end

@interface PUICApplicationStatusBarItem : NSObject <PUICApplicationStatusBarProperties, PUICStatusBarAppContextViewDataSource, NSSecureCoding, NSCopying>
@end

@interface UIViewController (PUICApplicationStatusBarAdditions)
@property (nonatomic, readonly, getter=puic_applicationStatusBarItem) PUICApplicationStatusBarItem *applicationStatusBarItem;
@end

@interface PUICResources : NSObject
+ (UIImage *)imageNamed:(NSString *)imageName inBundle:(NSBundle *)bundle shouldCache:(BOOL)shouldCache;
@end

@interface PUICStatusBarGlobalContextViewAssertion : NSObject
@end

@interface PUICStatusBar : UIStatusBar
@end

@interface PUICApplication : UIApplication
+ (instancetype)sharedPUICApplication;
- (PUICStatusBarGlobalContextViewAssertion *)_takeStatusBarGlobalContextAssertionAnimated:(BOOL)animated;
- (PUICStatusBar *)_puicStatusBar;
@end

typedef void (^PUICQuickboardCompletionBlock)(NSAttributedString * _Nullable);

@interface PUICQuickboardSpecs : NSObject
@end
@interface PUICQuickboardListViewSpecs : PUICQuickboardSpecs
@property (assign, nonatomic) CGFloat defaultButtonHeight;
@end

@interface PUICButton : UIButton
@end

@interface PUICQuickboardListTrayButton : PUICButton
- (instancetype)initWithFrame:(CGRect)frame tintColor:(nullable UIColor *)tintColor defaultHeight:(CGFloat)defaultHeight;
@end

@interface PUICTableViewCell : UITableViewCell
@property (nonatomic, getter=isRadioSectionCell) BOOL radioSectionCell;
- (void)configureForText:(NSString *)text width:(CGFloat)width;
@end

@interface PUICQuickboardListItemCell : PUICTableViewCell
@property (nonatomic, readonly) UILabel *itemLabel;
@end

@interface PUICTableView : UITableView
@end

@interface PUICTextInputContext : NSObject <UITextInputTraits, NSSecureCoding>

@property (nonatomic, strong) NSString *requestingApplicationBundleID;
@property (nonatomic, strong) NSString *initialText;
@property (nonatomic, strong, nullable) UIColor *tintColor;
@property (nonatomic, strong) UITextContentType secondaryTextContentType;
@property (nonatomic, assign) NSInteger minimumLength;
@property (nonatomic, assign) UIReturnKeyType returnKeyType;
@property (nonatomic, strong) NSAttributedString *attributedHeaderText;

@end

@class PUICQuickboardController;
@protocol PUICQuickboardControllerDelegate <NSObject>
- (void)quickboardController:(PUICQuickboardController *)quickboardController textInputValueDidChange:(NSAttributedString *)attributedText;
@end

@interface PUICQuickboardController : NSObject
@property (nonatomic, weak) id<PUICQuickboardControllerDelegate> delegate;
@property (nonatomic, strong) PUICTextInputContext *textInputContext;
@property (nonatomic, weak) UIViewController *quickboardPresentingViewController;
@end

@protocol PUICQuickboardController <NSObject>
@property (nonatomic, readonly) NSString *primaryLanguage;
@property (nonatomic, readonly) BOOL animatesSelectionToDestinationView;
- (void)dismissViewControllerAnimated:(BOOL)animated completion:(nullable void(^)(void))completion;
@end

@protocol PUICQuickboardViewControllerDelegate <NSObject>
- (void)quickboard:(id<PUICQuickboardController>)quickboard textEntered:(NSAttributedString *)attributedText;
- (void)quickboard:(id<PUICQuickboardController>)quickboard languageDidChange:(NSString *)languageCode;
- (void)quickboardInputCancelled:(id<PUICQuickboardController>)quickboard;
@end

@protocol PUICQuickboardTransition <NSObject>
@end

@interface PUICQuickboardViewController : UIViewController <PUICQuickboardController>
@property (nonatomic, weak) id<PUICQuickboardViewControllerDelegate> delegate;
@property (nonatomic, strong) UIView *contentView;
@property (nonatomic, readonly) UIButton *cancelButton;
@property (nonatomic, assign) NSInteger minTextLengthForEnablingAccept;
@property (nonatomic, assign) BOOL showsAcceptButton;
- (instancetype)initWithDelegate:(id<PUICQuickboardViewControllerDelegate>)delegate NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithCoder:(NSCoder *)aDecoder NS_DESIGNATED_INITIALIZER;
@end

@interface PUICQuickboardViewController (Transitions) <PUICQuickboardTransition>
- (void)addContentViewAnimations:(BOOL)isPresenting;
@end

@interface PUICQuickboardListViewController : PUICQuickboardViewController
@property (nonatomic, readonly) PUICTableView *listView;
@property (strong, nonatomic, readonly) PUICQuickboardListViewSpecs *specs;
@property (nonatomic, copy) UITextContentType textContentType;
- (void)reloadListItems;
- (void)reloadHeaderContentView;
@end

@interface PUICQuickboardArouetViewController : PUICQuickboardViewController
@property (nonatomic, copy, nullable) UITextContentType textContentType;
- (void)setInputText:(nullable NSString *)inputText selectionRange:(NSRange)selectionRange;
@end

typedef NS_ENUM(NSInteger, PUICPickerViewStyle) {
    PUICPickerViewStyleList,
    PUICPickerViewStyleStack,
    PUICPickerViewStyleSequence,
};

@protocol PUICPickerViewDelegate <NSObject>
@end
@protocol PUICPickerViewDataSource <NSObject>
@end

@interface PUICPickerView : UIView
@property (nonatomic) NSInteger selectedIndex;
@property (nonatomic, weak) id<PUICPickerViewDataSource> dataSource;
@property (nonatomic, weak) id<PUICPickerViewDelegate> delegate;
- (instancetype)initWithStyle:(PUICPickerViewStyle)style NS_DESIGNATED_INITIALIZER;
- (UIView *)dequeueReusableItemView;
- (void)reloadData;
@end

@protocol PUICDictationViewControllerDelegate <NSObject>
@end
@interface PUICDictationViewController : PUICQuickboardViewController
- (instancetype)initWithDelegate:(id<PUICQuickboardViewControllerDelegate>)delegate dictationMode:(PUICDictationMode)dictationMode NS_DESIGNATED_INITIALIZER;
@end

@interface PUICQuickboardViewController (ExposeHeader)
@property (nonatomic, readonly) UIView *headerView;
@end

@interface PUICActionItem : NSObject
@end

typedef NS_ENUM(NSInteger, PUICActionStyle) {
    PUICActionStyleAutomatic = 0,
    PUICActionStyleList = 1,
    PUICActionStyleGrid = 2,
};

@interface PUICActionGroup : NSObject
- (instancetype)initWithActionItems:(NSArray *)actionItems actionStyle:(PUICActionStyle)actionStyle;
@end

@class PUICQuickboardLanguageController;
@protocol PUICQuickboardLanguageControllerDelegate
- (void)languageControllerDidChangePrimaryLanguage:(PUICQuickboardLanguageController *)languageController;
@end

@interface PUICQuickboardLanguageController : NSObject
@property (nonatomic, readonly) NSString *primaryLanguage;
- (PUICActionItem *)languageSelectionActionItemForViewController:(UIViewController<PUICQuickboardLanguageControllerDelegate> *)viewController;
@end

@interface PUICQuickboardViewController (QuickboardLanguageSupport)
@property (nonatomic, strong) PUICQuickboardLanguageController *languageController;
- (void)primaryLanguageDidChange:(NSString *)primaryLanguage;
@end

@protocol PUICCrownInputSequencerDelegate <NSObject>
@end

@interface PUICCrownInputSequencer : NSObject
@property (nonatomic, weak) id<PUICCrownInputSequencerDelegate> delegate;
@property (nonatomic) double start;
@property (nonatomic) double end;
@property (nonatomic) BOOL useWideIdleCheck;
@property (nonatomic) double screenSpaceMultiplier;
@property (nonatomic) double offset;
@property (nonatomic, readonly, getter=isIdle) BOOL idle;
@property (nonatomic, getter=isRubberBandingEnabled) BOOL rubberBandingEnabled;

- (void)setOffset:(double)offset suppressIndicatorVisibilityChanges:(BOOL)suppressIndicatorVisibilityChanges;
- (void)updateWithCrownInputEvent:(UIEvent *)crownInputEvent;
- (void)stopVelocityTrackingAndDecelerationImmediately;
@end

@interface UIScrollView (PUICAdditions) <PUICCrownInputSequencerDelegate>
- (CGPoint)_puic_contentOffsetForCrownInputSequencerOffset:(double)sequencerOffset;
@property (nonatomic, getter=puic_crownInputScrollDirection, setter=puic_setCrownInputScrollDirection:) PUICCrownInputScrollDirection crownInputScrollDirection;
@end

@interface PUICActionController : NSObject
- (instancetype)initWithActionGroup:(PUICActionGroup *)actionGroup NS_DESIGNATED_INITIALIZER;
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)

#endif // PLATFORM(WATCHOS)
