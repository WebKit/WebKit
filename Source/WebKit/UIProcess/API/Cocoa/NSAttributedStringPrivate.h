/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import <WebKit/NSAttributedString.h>

@class WKNavigation;
@class WKWebView;

NS_ASSUME_NONNULL_BEGIN

/*!
 @discussion Private extension of @link //apple_ref/occ/NSAttributedString NSAttributedString @/link to
 translate HTML content into attributed strings using WebKit.
 */
@interface NSAttributedString (WKPrivate)

/*!
 @abstract Converts the contents loaded by a content loader block into an attributed string.
 @param options Document attributes for interpreting the document contents.
 NSTextSizeMultiplierDocumentOption, and NSTimeoutDocumentOption are supported option keys.
 @param contentLoader A block to invoke when content needs to be loaded in the supplied
 @link WKWebView @/link. A @link WKNavigation @/link for the main frame must be returned.
 @param completionHandler A block to invoke when the translation completes or fails.
 @discussion The completionHandler is passed the attributed string result along with any
 document-level attributes, or an error.
 */
+ (void)_loadFromHTMLWithOptions:(NSDictionary<NSAttributedStringDocumentReadingOptionKey, id> *)options contentLoader:(WKNavigation *(^)(WKWebView *))loadWebContent completionHandler:(NSAttributedStringCompletionHandler)completionHandler;

@end

NS_ASSUME_NONNULL_END
