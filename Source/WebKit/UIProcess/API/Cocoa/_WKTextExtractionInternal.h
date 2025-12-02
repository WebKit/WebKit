/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#import <WebKit/_WKJSHandle.h>
#import <WebKit/_WKTextExtraction.h>

NS_ASSUME_NONNULL_BEGIN

// This is equivalent to USE(APPLE_INTERNAL_SDK) || (!PLATFORM(WATCH) && !PLATFORM(APPLETV)
#if (defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)) || (!TARGET_OS_WATCH && !TARGET_OS_TV)

@interface _WKTextExtractionConfiguration ()

/*!
 Whether to merge adjacent runs of text into paragraphs.
 This also combines links and editable containers into a single text item.
 Defaults to `NO`.
 */
@property (nonatomic) BOOL mergeParagraphs;

/*!
 Ignores transparent (or nearly-transparent) subtrees.
 Defaults to `NO`.
 */
@property (nonatomic) BOOL skipNearlyTransparentContent;

/*!
 Iterates over all custom node attributes added via -addClientAttribute:value:forNode:.
 */
- (void)forEachClientNodeAttribute:(void(NS_NOESCAPE ^)(NSString *attribute, NSString *value, _WKJSHandle *))block;

/*!
 Only include visible text content, excluding all DOM attributes and element types.
 Takes precedence over `includeURLs`, `includeRects`, etc.
 Defaults to `NO`.
 */
@property (nonatomic) BOOL onlyIncludeVisibleText;

@end

@interface _WKTextExtractionResult ()

- (instancetype)initWithTextContent:(NSString *)textContent filteredOutAnyText:(BOOL)filteredOutAnyText;

@end

@interface _WKTextExtractionInteraction ()

@property (nonatomic, readonly) BOOL hasSetLocation;

@end

@interface _WKTextExtractionInteractionResult ()

- (instancetype)initWithErrorDescription:(nullable NSString *)errorDescription;

@end

#endif // (defined __has_include && __has_include(<CoreFoundation/CFPriv.h>)) || (!TARGET_OS_WATCH && !TARGET_OS_TV)

typedef NS_ENUM(NSInteger, WKTextExtractionContainer) {
    WKTextExtractionContainerRoot,
    WKTextExtractionContainerViewportConstrained,
    WKTextExtractionContainerList,
    WKTextExtractionContainerListItem,
    WKTextExtractionContainerBlockQuote,
    WKTextExtractionContainerArticle,
    WKTextExtractionContainerSection,
    WKTextExtractionContainerNav,
    WKTextExtractionContainerButton,
    WKTextExtractionContainerCanvas,
    WKTextExtractionContainerSubscript,
    WKTextExtractionContainerSuperscript,
    WKTextExtractionContainerGeneric
};

typedef NS_OPTIONS(NSUInteger, WKTextExtractionEventListenerTypes) {
    WKTextExtractionEventListenerTypeNone      = 0,
    WKTextExtractionEventListenerTypeClick     = 1 << 0,
    WKTextExtractionEventListenerTypeHover     = 1 << 1,
    WKTextExtractionEventListenerTypeTouch     = 1 << 2,
    WKTextExtractionEventListenerTypeWheel     = 1 << 3,
    WKTextExtractionEventListenerTypeKeyboard  = 1 << 4,
};

typedef NS_ENUM(NSInteger, WKTextExtractionEditableType) {
    WKTextExtractionEditablePlainTextOnly,
    WKTextExtractionEditableRichText,
};

@interface WKTextExtractionLink : NSObject
- (instancetype)initWithURL:(NSURL *)url range:(NSRange)range;
@property (nonatomic, readonly) NSURL *url;
@property (nonatomic, readonly) NSRange range;
@end

@interface WKTextExtractionEditable : NSObject
- (instancetype)initWithLabel:(NSString *)label placeholder:(NSString *)placeholder isSecure:(BOOL)isSecure isFocused:(BOOL)isFocused;
@property (nonatomic, readonly) NSString *label;
@property (nonatomic, readonly) NSString *placeholder;
@property (nonatomic, readonly, getter=isSecure) BOOL secure;
@property (nonatomic, readonly, getter=isFocused) BOOL focused;
@end

@interface WKTextExtractionItem : NSObject
@property (nonatomic, readonly) NSArray<WKTextExtractionItem *> *children;
@property (nonatomic, readonly) CGRect rectInWebView;
@property (nonatomic, readonly) WKTextExtractionEventListenerTypes eventListeners;
@property (nonatomic, readonly) NSDictionary<NSString *, NSString *> *ariaAttributes;
@property (nonatomic, readonly) NSString *accessibilityRole;
@property (nonatomic, readonly, nullable) NSString *nodeIdentifier; // Replace with an UI-side node handle when that's available.
@end

@interface WKTextExtractionContainerItem : WKTextExtractionItem
- (instancetype)initWithContainer:(WKTextExtractionContainer)container rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) WKTextExtractionContainer container;
@end

@interface WKTextExtractionLinkItem : WKTextExtractionItem
- (instancetype)initWithTarget:(NSString *)target url:(nullable NSURL *)url rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) NSString *target;
@property (nonatomic, readonly, nullable) NSURL *url;
@end

@interface WKTextExtractionContentEditableItem : WKTextExtractionItem
- (instancetype)initWithContentEditableType:(WKTextExtractionEditableType)contentEditableType isFocused:(BOOL)isFocused rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) WKTextExtractionEditableType contentEditableType;
@property (nonatomic, readonly, getter=isFocused) BOOL focused;
@end

@interface WKTextExtractionTextFormControlItem : WKTextExtractionItem
- (instancetype)initWithEditable:(WKTextExtractionEditable *)editable controlType:(NSString *)controlType autocomplete:(NSString *)autocomplete isReadonly:(BOOL)isReadonly isDisabled:(BOOL)isDisabled isChecked:(BOOL)isChecked rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) NSString *label;
@property (nonatomic, readonly) NSString *placeholder;
@property (nonatomic, readonly, getter=isSecure) BOOL secure;
@property (nonatomic, readonly, getter=isFocused) BOOL focused;
@property (nonatomic, readonly) NSString *controlType;
@property (nonatomic, readonly) NSString *autocomplete;
@property (nonatomic, readonly, getter=isReadonly) BOOL readonly;
@property (nonatomic, readonly, getter=isDisabled) BOOL disabled;
@property (nonatomic, readonly, getter=isChecked) BOOL checked;
@end

@interface WKTextExtractionTextItem : WKTextExtractionItem
- (instancetype)initWithContent:(NSString *)content selectedRange:(NSRange)selectedRange links:(NSArray<WKTextExtractionLink *> *)links editable:(WKTextExtractionEditable * _Nullable)editable rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) NSArray<WKTextExtractionLink *> *links;
@property (nonatomic, readonly, nullable) WKTextExtractionEditable *editable;
@property (nonatomic) NSRange selectedRange;
@property (nonatomic, copy) NSString *content;
@end

@interface WKTextExtractionScrollableItem : WKTextExtractionItem
- (instancetype)initWithContentSize:(CGSize)contentSize rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) CGSize contentSize;
@end

@interface WKTextExtractionSelectItem : WKTextExtractionItem
- (instancetype)initWithSelectedValues:(NSArray<NSString *> *)selectedValues supportsMultiple:(BOOL)supportsMultiple rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) NSArray<NSString *> *selectedValues;
@property (nonatomic, readonly) BOOL supportsMultiple;
@end

@interface WKTextExtractionImageItem : WKTextExtractionItem
- (instancetype)initWithName:(NSString *)name altText:(NSString *)altText rectInWebView:(CGRect)rectInWebView children:(NSArray<WKTextExtractionItem *> *)children eventListeners:(WKTextExtractionEventListenerTypes)eventListeners ariaAttributes:(NSDictionary<NSString *, NSString *> *)ariaAttributes accessibilityRole:(NSString *)accessibilityRole nodeIdentifier:(nullable NSString *)nodeIdentifier;
@property (nonatomic, readonly) NSString *name;
@property (nonatomic, readonly) NSString *altText;
@end

NS_ASSUME_NONNULL_END

