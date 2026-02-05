/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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

#pragma once

DECLARE_SYSTEM_HEADER

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSImage_Private.h>
#import <AppKit/NSInspectorBar.h>
#import <AppKit/NSInspectorBarItemController.h>
#import <AppKit/NSInspectorBar_Private.h>
#import <AppKit/NSMenu_Private.h>
#import <AppKit/NSScrollViewSeparatorTrackingAdapter_Private.h>
#import <AppKit/NSTextInputClient_Private.h>
#import <AppKit/NSView_UnifiedLayout.h>
#import <AppKit/NSWindow_Private.h>

#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
#import <AppKit/NSScrollPocket_Private.h>
#endif

#else

@class NSTextPlaceholder;

@protocol NSTextInputClient_Async
- (void)selectedRangeWithCompletionHandler:(void(^)(NSRange selectedRange))completionHandler;
- (void)markedRangeWithCompletionHandler:(void(^)(NSRange markedRange))completionHandler;
- (void)hasMarkedTextWithCompletionHandler:(void(^)(BOOL hasMarkedText))completionHandler;
- (void)attributedSubstringForProposedRange:(NSRange)range completionHandler:(void(^)(NSAttributedString *, NSRange actualRange))completionHandler;
- (void)firstRectForCharacterRange:(NSRange)range completionHandler:(void(^)(NSRect firstRect, NSRange actualRange))completionHandler;

@optional

- (void)insertTextPlaceholderWithSize:(CGSize)size completionHandler:(void (^)(NSTextPlaceholder *))completionHandler;

- (void)removeTextPlaceholder:(NSTextPlaceholder *)placeholder willInsertText:(BOOL)willInsertText completionHandler:(void (^)(void))completionHandler;
@end

@protocol NSInspectorBarClient <NSObject>
@property (readonly) NSArray<NSString *> *inspectorBarItemIdentifiers;
@property (readonly) NSWindow *window;
@end

@interface NSInspectorBar : NSObject
+ (Class)standardItemControllerClass;
+ (NSArray<NSString *> *)standardTextItemIdentifiers;
@property (strong) id <NSInspectorBarClient> client;
@property (getter=isVisible) BOOL visible;
@end

NSString * const NSInspectorBarFontFamilyItemIdentifier = @"NSInspectorBarFontFamilyItemIdentifier";
NSString * const NSInspectorBarFontSizeItemIdentifier = @"NSInspectorBarFontSizeItemIdentifier";
NSString * const NSInspectorBarTextForegroundColorItemIdentifier = @"NSInspectorBarTextForegroundColorItemIdentifier";
NSString * const NSInspectorBarTextBackgroundColorItemIdentifier = @"NSInspectorBarTextBackgroundColorItemIdentifier";
NSString * const NSInspectorBarFontStyleItemIdentifier = @"NSInspectorBarFontStyleItemIdentifier";
NSString * const NSInspectorBarTextAlignmentItemIdentifier = @"NSInspectorBarTextAlignmentItemIdentifier";

@interface __NSInspectorBarItemController : NSObject
- (instancetype)initWithInspectorBar:(NSInspectorBar *)bar NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
- (void)updateSelectedAttributes;
- (void)fontSizeItemWasClicked:(NSNumber *)size;
@property (readonly) NSPopUpButton *stylePopup;
@property (readonly) NSPopUpButton *fontFamilyPopup;
@property (readonly) NSComboBox *fontSizeComboBox;
@property (readonly) NSColorWell *foregroundColorWell;
@property (readonly) NSColorWell *backgroundColorWell;
@property (readonly) NSSegmentedControl *textStyleSwitches;
@end

@interface NSWindow (NSInspectorBarSupport)
- (NSInspectorBar *)inspectorBar;
- (void)setInspectorBar:(NSInspectorBar *)bar;
@end

@protocol NSScrollViewSeparatorTrackingAdapter
@property (readonly) NSRect scrollViewFrame;
@property (readonly) BOOL hasScrolledContentsUnderTitlebar;
@end

@interface NSMenu (SPI)
@property (readonly) NSView *_presentingView;
@end

@interface NSScrollPocket : NSView
@property (copy, nullable) NSColor *captureColor;
@property BOOL prefersSolidColorHardPocket;
@end

@interface NSImage (SPI)
@property (readonly, getter=_isSymbolImage) BOOL _symbolImage;
@end

@interface NSView (_NSConstraintBasedLayoutEmbedding)
@property (readonly) BOOL _wantsConstraintBasedLayout;
@end

#if HAVE(NSVIEW_CORNER_CONFIGURATION)

@interface _NSCornerRadius : NSObject
@property (class, copy, readonly) _NSCornerRadius *containerConcentricRadius;
+ (_NSCornerRadius *)fixedRadius:(CGFloat)radius;
@end

@interface NSViewCornerRadii : NSObject
@property CGFloat topLeft;
@property CGFloat topRight;
@property CGFloat bottomLeft;
@property CGFloat bottomRight;
@property (copy) CALayerCornerCurve cornerCurve;
@end

@interface NSViewCornerConfiguration : NSObject
+ (NSViewCornerConfiguration *)configurationWithRadius:(_NSCornerRadius *)radius;
+ (instancetype)configurationWithTopLeftRadius:(nullable _NSCornerRadius *)topLeftRadius topRightRadius:(nullable _NSCornerRadius *)topRightRadius bottomLeftRadius:(nullable _NSCornerRadius *)bottomLeftRadius bottomRightRadius:(nullable _NSCornerRadius *)bottomRightRadius;
@end

@interface NSView (NSViewCornerConfiguration)
@property (nullable, readonly) NSViewCornerRadii *_effectiveCornerRadii;
@property (readonly, nullable, copy) NSViewCornerConfiguration *_cornerConfiguration;
- (void)_viewDidChangeEffectiveCornerRadii;
- (void)_invalidateCornerConfiguration;
@end

#endif

#endif

@protocol NSTextInputClient_Async_Staging_44648564
@optional
- (void)typingAttributesWithCompletionHandler:(void(^)(NSDictionary<NSString *, id> *))completionHandler;
@end

@interface NSInspectorBar (IPI)
- (NSFont *)convertFont:(NSFont *)font;
- (NSDictionary *)convertAttributes:(NSDictionary *)attributes;
@end

@interface __NSInspectorBarItemController (IPI)
- (void)_fontPopupAction:(id)sender;
- (void)_fontStyleAction:(id)sender;
- (void)_colorAction:(id)sender;
- (void)menuNeedsUpdate:(NSMenu *)menu;
@end

@interface NSFontOptions : NSObject
+ (instancetype)sharedFontOptions;
@end

@interface NSFontPanel (IPI)
@property (nonatomic, readonly) NSString *_selectedFaceName;
- (void)_chooseFace:(id)sender;
@end

@interface NSFontEffectsBox : NSBox
@end

#endif // PLATFORM(MAC)
