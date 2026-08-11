/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKFoundation.h>

NS_ASSUME_NONNULL_BEGIN

@protocol _WKTranslationDelegate <NSObject>

@optional

/*! @abstract Translates strings that WebKit is about to hand to assistive technologies.
 @param strings The strings to translate, in the order they will be announced.
 @param targetLocaleIdentifier The BCP 47 tag of the language the page is being presented in.
 @param completionHandler Must be called exactly once, on the main thread, with an array parallel to
 @c strings. Pass nil, or an array whose count differs from @c strings, to indicate that no
 translation is available; the original strings are then used.
 @discussion Called only while -_displayedTranslationLocaleIdentifier is non-nil. Core-AAM requires
 that announcements be conveyed in the language the user is seeing rather than the language of the
 original content, so WebKit withholds the announcement -- and every announcement queued behind it --
 until @c completionHandler runs. That wait is bounded. If the handler is not called within an
 internal timeout, the original strings are announced instead so that a slow or unavailable
 translation service can never silently drop an accessibility announcement.
 */
- (void)_webView:(WKWebView *)webView translateAccessibilityAnnouncementStrings:(NSArray<NSString *> *)strings targetLocaleIdentifier:(NSString *)targetLocaleIdentifier completionHandler:(void (^)(NSArray<NSString *> * _Nullable translatedStrings))completionHandler WK_API_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA));

@end

NS_ASSUME_NONNULL_END
