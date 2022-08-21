/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "AccessibilityObjectInterface.h"
#import "FontPlatformData.h"
#import <CoreGraphics/CoreGraphics.h>
#import <variant>
#import <wtf/RefPtr.h>
#import <wtf/WeakPtr.h>

namespace WebCore {
struct AccessibilitySearchCriteria;
class AccessibilityObject;
class AXIsolatedObject;
class Document;
class IntRect;
class FloatPoint;
class HTMLTextFormControlElement;
class Path;
class VisiblePosition;
}

// Tokens used to denote attributes in NSAttributedStrings.
static NSString * const UIAccessibilityTokenBlockquoteLevel = @"UIAccessibilityTokenBlockquoteLevel";
static NSString * const UIAccessibilityTokenHeadingLevel = @"UIAccessibilityTokenHeadingLevel";
static NSString * const UIAccessibilityTokenFontName = @"UIAccessibilityTokenFontName";
static NSString * const UIAccessibilityTokenFontFamily = @"UIAccessibilityTokenFontFamily";
static NSString * const UIAccessibilityTokenFontSize = @"UIAccessibilityTokenFontSize";
static NSString * const UIAccessibilityTokenBold = @"UIAccessibilityTokenBold";
static NSString * const UIAccessibilityTokenItalic = @"UIAccessibilityTokenItalic";
static NSString * const UIAccessibilityTokenUnderline = @"UIAccessibilityTokenUnderline";
static NSString * const UIAccessibilityTokenLanguage = @"UIAccessibilityTokenLanguage";
static NSString * const UIAccessibilityTokenAttachment = @"UIAccessibilityTokenAttachment";

static NSString * const UIAccessibilityTextAttributeContext = @"UIAccessibilityTextAttributeContext";
static NSString * const UIAccessibilityTextualContextSourceCode = @"UIAccessibilityTextualContextSourceCode";

bool AXAttributedStringRangeIsValid(NSAttributedString *, const NSRange&);
void AXAttributedStringSetFont(NSMutableAttributedString *, CTFontRef, const NSRange&);

@interface WebAccessibilityObjectWrapperBase : NSObject {
    WebCore::AccessibilityObject* m_axObject;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    WebCore::AXIsolatedObject* m_isolatedObject;
    // To be accessed only on the main thread.
    bool m_isolatedObjectInitialized;
#endif

    WebCore::AXID _identifier;
}

- (id)initWithAccessibilityObject:(WebCore::AccessibilityObject*)axObject;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
- (void)attachIsolatedObject:(WebCore::AXIsolatedObject*)isolatedObject;
- (BOOL)hasIsolatedObject;
#endif

- (void)detach;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
- (void)detachIsolatedObject:(WebCore::AccessibilityDetachmentType)detachmentType;
#endif

@property (nonatomic, assign) WebCore::AXID identifier;

// FIXME: unified these two methods into one.
#if PLATFORM(MAC)
// Updates the underlying object and accessibility hierarchy , and returns the
// corresponding AXCoreObject.
- (WebCore::AXCoreObject*)updateObjectBackingStore;
#else
- (BOOL)_prepareAccessibilityCall;
#endif

// This can be either an AccessibilityObject or an AXIsolatedObject
- (WebCore::AXCoreObject*)axBackingObject;

- (NSArray<NSDictionary *> *)lineRectsAndText;

// These are pre-fixed with base so that AppKit does not end up calling into these directly (bypassing safety checks).
- (NSString *)baseAccessibilityHelpText;
- (NSArray<NSString *> *)baseAccessibilitySpeechHint;

- (NSString *)ariaLandmarkRoleDescription;

- (id)attachmentView;
// Used to inform an element when a notification is posted for it. Used by tests.
- (void)accessibilityPostedNotification:(NSString *)notificationName;
- (void)accessibilityPostedNotification:(NSString *)notificationName userInfo:(NSDictionary *)userInfo;

- (CGPathRef)convertPathToScreenSpace:(const WebCore::Path&)path;

- (CGRect)convertRectToSpace:(const WebCore::FloatRect&)rect space:(WebCore::AccessibilityConversionSpace)space;
- (NSArray *)contentForSimpleRange:(const WebCore::SimpleRange&)range attributed:(BOOL)attributed;

// Math related functions
- (NSArray *)accessibilityMathPostscriptPairs;
- (NSArray *)accessibilityMathPrescriptPairs;

- (NSRange)accessibilityVisibleCharacterRange;

- (NSDictionary<NSString *, id> *)baseAccessibilityResolvedEditingStyles;

extern WebCore::AccessibilitySearchCriteria accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(const NSDictionary *);

extern NSArray *makeNSArray(const WebCore::AXCoreObject::AccessibilityChildrenVector&);
extern NSRange makeNSRange(std::optional<WebCore::SimpleRange>);
extern std::optional<WebCore::SimpleRange> makeDOMRange(WebCore::Document*, NSRange);

#if PLATFORM(IOS_FAMILY)
- (id)_accessibilityWebDocumentView;
#endif

@end
