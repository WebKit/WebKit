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

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@class WKFrameInfo;
@class WKWebView;
@class _WKJSHandle;

typedef NS_OPTIONS(NSUInteger, _WKTextExtractionFilterOptions) {
    _WKTextExtractionFilterNone = 0,
    _WKTextExtractionFilterTextRecognition = 1 << 0,
    _WKTextExtractionFilterClassifier = 1 << 1,
    _WKTextExtractionFilterRules = 1 << 2,
    _WKTextExtractionFilterAll = NSUIntegerMax,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

typedef NS_ENUM(NSInteger, _WKTextExtractionNodeIdentifierInclusion) {
    _WKTextExtractionNodeIdentifierInclusionNone = 0,
    _WKTextExtractionNodeIdentifierInclusionEditableOnly,
    _WKTextExtractionNodeIdentifierInclusionInteractive,
    _WKTextExtractionNodeIdentifierInclusionAllContainers,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

typedef NS_ENUM(NSInteger, _WKTextExtractionOutputFormat) {
    _WKTextExtractionOutputFormatTextTree = 0,
    _WKTextExtractionOutputFormatHTML,
    _WKTextExtractionOutputFormatMarkdown,
    _WKTextExtractionOutputFormatJSON,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

#define WK_TEXT_EXTRACTION_HAS_DATA_DETECTOR_TYPES 1

typedef NS_OPTIONS(NSUInteger, _WKTextExtractionDataDetectorTypes) {
    _WKTextExtractionDataDetectorNone               = 0,
    _WKTextExtractionDataDetectorMoney              = 1 << 0,
    _WKTextExtractionDataDetectorAddress            = 1 << 1,
    _WKTextExtractionDataDetectorCalendarEvent      = 1 << 2,
    _WKTextExtractionDataDetectorTrackingNumber     = 1 << 3,
    _WKTextExtractionDataDetectorAll                = NSUIntegerMax,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

typedef NS_ENUM(NSInteger, _WKTextExtractionWordLimitPolicy) {
    _WKTextExtractionWordLimitPolicyAlways,
    _WKTextExtractionWordLimitPolicyDiscretionary,
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface _WKTextExtractionConfiguration : NSObject

@property (nonatomic, class, copy, readonly) _WKTextExtractionConfiguration *configurationForVisibleTextOnly NS_SWIFT_NAME(visibleTextOnly);

/*!
 Output format to use when collating extracted elements into the final text output.
 The default value is `.textTree`, which produces at most 1 element and text node per line,
 and uses indentation to represent DOM hierarchy.
 */
@property (nonatomic) _WKTextExtractionOutputFormat outputFormat;

/*!
 Element extraction is constrained to this rect (in the web view's coordinate space).
 Extracted elements must intersect with this rect, to be included.
 The default value is `.null`, which includes all elements.
 */
@property (nonatomic) CGRect targetRect;

/*!
 Include URL attribute values, such as `href` or `src` on links or images.
 The default value is `YES`.
 */
@property (nonatomic) BOOL includeURLs;

/*!
 Automatically include bounding rects for all text nodes.
 The default value is `YES`.
 */
@property (nonatomic) BOOL includeRects;

/*!
 Policy determining which nodes should be uniquely identified in the output.
 `.none`          	Prevents collection of any identifiers.
 `.editableOnly`    Limits collection of identifiers to editable elements and form controls.
 `.interactive`     Collects identifiers for all buttons, links, and other interactive elements.
 `.allContainers`   All containers (excludes text items).
 The default value is `.interactive`.
 */
@property (nonatomic) _WKTextExtractionNodeIdentifierInclusion nodeIdentifierInclusion;

/*!
 Include information about event listeners.
 The default value is `YES`.
 */
@property (nonatomic) BOOL includeEventListeners;

/*!
 Include accessibility attributes (e.g. `role`, `aria-label`).
 The default value is `YES`.
 */
@property (nonatomic) BOOL includeAccessibilityAttributes;

/*!
 Include text content underneath form controls that have been modified via AutoFill.
 The default value is `YES`.
 */
@property (nonatomic) BOOL includeTextInAutoFilledControls;

/*!
 Max number of words to include per paragraph; remaining text is truncated with an ellipsis (…).
 The default value is `NSUIntegerMax`.
 */
@property (nonatomic) NSUInteger maxWordsPerParagraph;

/*!
 Policy around when to enforce the max number of words to include per paragraph.
 `.always`          Always enforce word limits per paragraph.
 `.discretionary`   Limits may be ignored in cases where the total length of output text size is small.
 The default value is `.always`.
 */
@property (nonatomic) _WKTextExtractionWordLimitPolicy maxWordsPerParagraphPolicy;

/*!
 If specified, text extraction is limited to the subtree of this node.
 The default value is `nil`.
 */
@property (nonatomic, copy, nullable) _WKJSHandle *targetNode;

/*!
 If specified, these DOM nodes and their subtrees will be skipped during extraction.
 The default value is an empty array.
 */
@property (nonatomic, copy) NSArray<_WKJSHandle *> *nodesToSkip;

/*!
 If specified, text extraction includes content from these frames in addition to
 content in the main frame and same-origin subframes (which are included by default).
 The default value is an empty array.
 */
@property (nonatomic, copy, null_resettable) NSArray<WKFrameInfo *> *additionalFrames;

/*!
 Client-specified attributes and values to add when extracting DOM nodes.
 Will appear as "attribute=value" in text extraction output.
 */
- (void)addClientAttribute:(NSString *)attributeName value:(NSString *)attributeValue forNode:(_WKJSHandle *)node;

/*!
 A mapping of strings to replace in text extraction output.
 Each key represents a string that should be replaced, and the corresponding
 value represents the string to replace it with.
 The default value is `nil`.
 */
@property (nonatomic, copy, nullable) NSDictionary<NSString *, NSString *> *replacementStrings;

/*!
 Filters to apply when extracting text.
 Defaults to `_WKTextExtractionFilterAll`.
 */
@property (nonatomic) _WKTextExtractionFilterOptions filterOptions;

/*!
 Automatically shorten extracted URLs by removing or replacing parts of each URL.
 The default value is `NO`.
 */
@property (nonatomic) BOOL shortenURLs;

/*!
 Automatically run data detectors for the given types, and limit extraction output
 to text around the most prominent matches.
 The default value is `.none`.
 */
@property (nonatomic) _WKTextExtractionDataDetectorTypes dataDetectorTypes;

@end

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface _WKTextExtractionResult : NSObject

@property (nonatomic, readonly) NSString *textContent;

/*!
 Set to `YES` if and only if any output text was filtered out as a result
 of `_WKTextExtractionFilterOptions` or the maximum paragraph word limit.
 */
@property (nonatomic, readonly) BOOL filteredOutAnyText;

/*!
 A map of shortened URL strings to their original URLs; only populated when
 `shortenURLs` is set when performing text extraction.
 */
@property (nonatomic, readonly) NSDictionary<NSString *, NSURL *> *shortenedURLs;

/*!
 Asynchronously map a node identifier string (corresponding to a `uid` in
 text extraction output) to a corresponding JS handle to the node.
 @param nodeIdentifier  The ID of the node to extract, or the ID of the node to search if `searchText` is additionally specified.
 @param searchText      Rendered text to search inside the document or node corresponding to `nodeIdentifier`. The resulting element will fully contain this text.
 At least one of `nodeIdentifier` or `searchText` must be specified.
 */
- (void)requestJSHandleForNodeIdentifier:(nullable NSString *)nodeIdentifier searchText:(nullable NSString *)searchText completionHandler:(void (^)(_WKJSHandle * _Nullable))completionHandler;

@end

typedef NS_ENUM(NSInteger, _WKTextExtractionAction) {
    _WKTextExtractionActionClick,
    _WKTextExtractionActionSelectText,
    _WKTextExtractionActionSelectMenuItem,
    _WKTextExtractionActionTextInput,
    _WKTextExtractionActionKeyPress,
    _WKTextExtractionActionHighlightText,
    _WKTextExtractionActionScrollBy
} WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
WK_SWIFT_UI_ACTOR
NS_REQUIRES_PROPERTY_DEFINITIONS
@interface _WKTextExtractionInteraction : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithAction:(_WKTextExtractionAction)action NS_DESIGNATED_INITIALIZER;

- (void)debugDescriptionInWebView:(WKWebView *)webView completionHandler:(void (^)(NSString * _Nullable, NSError * _Nullable))completionHandler;

@property (nonatomic, readonly) _WKTextExtractionAction action;
@property (nonatomic, copy, nullable) NSString *nodeIdentifier;
@property (nonatomic, copy, nullable) NSString *text;
@property (nonatomic) BOOL replaceAll;
@property (nonatomic) BOOL scrollToVisible;
@property (nonatomic) CGSize scrollDelta;

// Must be within the visible bounds of the web view.
@property (nonatomic) CGPoint location;

@end

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
WK_SWIFT_UI_ACTOR
NS_REQUIRES_PROPERTY_DEFINITIONS
@interface _WKTextExtractionInteractionResult : NSObject

@property (nonatomic, readonly, nullable) NSError *error;

@end

NS_HEADER_AUDIT_END(nullability, sendability)
