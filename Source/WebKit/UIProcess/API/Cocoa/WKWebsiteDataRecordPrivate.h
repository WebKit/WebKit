/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#import <WebKit/WKWebsiteDataRecord.h>

NS_ASSUME_NONNULL_BEGIN

@class _WKWebsiteDataSize;

WK_EXTERN NSString * const _WKWebsiteDataTypeHSTSCache WK_API_AVAILABLE(macosx(10.11), ios(9.0));
WK_EXTERN NSString * const _WKWebsiteDataTypeMediaKeys WK_API_AVAILABLE(macosx(10.11), ios(9.0));
WK_EXTERN NSString * const _WKWebsiteDataTypeSearchFieldRecentSearches WK_API_AVAILABLE(macosx(10.12), ios(10.0));
WK_EXTERN NSString * const _WKWebsiteDataTypeResourceLoadStatistics WK_API_AVAILABLE(macosx(10.12), ios(10.0));
WK_EXTERN NSString * const _WKWebsiteDataTypeCredentials WK_API_AVAILABLE(macosx(10.13), ios(11.0));


#if !TARGET_OS_IPHONE
WK_EXTERN NSString * const _WKWebsiteDataTypePlugInData WK_API_AVAILABLE(macosx(10.11));
#endif

@interface WKWebsiteDataRecord (WKPrivate)

@property (nullable, nonatomic, readonly) _WKWebsiteDataSize *_dataSize;

- (NSArray<NSString *> *)_originsStrings WK_API_AVAILABLE(macosx(10.14), ios(12.0));

@end

NS_ASSUME_NONNULL_END
