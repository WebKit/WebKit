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

#if WK_API_ENABLED

NS_ASSUME_NONNULL_BEGIN

@class _WKWebsiteDataSize;

WK_EXTERN NSString * const _WKWebsiteDataTypeHSTSCache WK_AVAILABLE(10_11, 9_0);
WK_EXTERN NSString * const _WKWebsiteDataTypeMediaKeys WK_AVAILABLE(10_11, 9_0);
WK_EXTERN NSString * const _WKWebsiteDataTypeSearchFieldRecentSearches WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);
WK_EXTERN NSString * const _WKWebsiteDataTypeResourceLoadStatistics WK_AVAILABLE(WK_MAC_TBA, WK_IOS_TBA);


#if !TARGET_OS_IPHONE
WK_EXTERN NSString * const _WKWebsiteDataTypePlugInData WK_AVAILABLE(10_11, NA);
#endif

@interface WKWebsiteDataRecord (WKPrivate)

@property (nullable, nonatomic, readonly) _WKWebsiteDataSize *_dataSize;

@end

NS_ASSUME_NONNULL_END

#endif
