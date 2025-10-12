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

@class WKWebView;

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface _WKTextExtractionConfiguration : NSObject

/*!
 Element extraction is constrained to this rect (in the web view's coordinate space).
 Extracted elements must intersect with this rect, to be included.
 The default value is `.null`, which includes all elements.
 */
@property (nonatomic) CGRect targetRect;

@end

typedef NS_ENUM(NSInteger, _WKTextExtractionAction) {
    _WKTextExtractionActionClick,
    _WKTextExtractionActionSelectText,
    _WKTextExtractionActionSelectMenuItem,
    _WKTextExtractionActionTextInput,
    _WKTextExtractionActionKeyPress,
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
