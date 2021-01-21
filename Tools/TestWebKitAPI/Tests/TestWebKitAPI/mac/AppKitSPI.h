/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSInspectorBar.h>
#import <AppKit/NSInspectorBarItemController.h>
#import <AppKit/NSInspectorBar_Private.h>
#import <AppKit/NSTextInputClient_Private.h>
#import <AppKit/NSWindow_Private.h>

#else

@protocol NSTextInputClient_Async
- (void)selectedRangeWithCompletionHandler:(void(^)(NSRange selectedRange))completionHandler;
- (void)markedRangeWithCompletionHandler:(void(^)(NSRange markedRange))completionHandler;
- (void)hasMarkedTextWithCompletionHandler:(void(^)(BOOL hasMarkedText))completionHandler;
- (void)attributedSubstringForProposedRange:(NSRange)range completionHandler:(void(^)(NSAttributedString *, NSRange actualRange))completionHandler;
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

#endif // PLATFORM(MAC)
