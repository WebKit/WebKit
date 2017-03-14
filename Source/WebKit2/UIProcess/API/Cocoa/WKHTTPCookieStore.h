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

#import <WebKit/WKFoundation.h>

#if WK_API_ENABLED

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*!
 A WKHTTPCookieStore object allows managing the HTTP cookies associated with a particular WKWebsiteDataStore.
 */
WK_CLASS_AVAILABLE(macosx(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface WKHTTPCookieStore : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*! @abstract Fetches all stored cookies.
 @param completionHandler A block to invoke with the fetched cookies.
 */
- (void)fetchCookies:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler;

/*! @abstract Fetches all of the stored cookies for the given URL.
 @param completionHandler A block to invoke with the fetched cookies.
 */
- (void)fetchCookiesForURL:(NSURL *)url completionHandler:(void (^)(NSArray<NSHTTPCookie *> *))completionHandler;

/*! @abstract Set a cookie.
 @param cookie The cookie to set.
 @param completionHandler A block to invoke once the cookie has been stored.
 */
- (void)setCookie:(NSHTTPCookie *)cookie completionHandler:(nullable void (^)())completionHandler;

/*! @abstract Adds an array cookies to the cookie store, following the cookie accept policy.
 @param cookies The cookies to set.
 @param URL The URL from which the cookies were sent.
 @param mainDocumentURL The main document URL to be used as a base for the "same domain as main document" policy.
 @param completionHandler A block to invoke once the cookies have been stored.
 */
- (void)setCookies:(NSArray<NSHTTPCookie *> *)cookies forURL:(NSURL *)url mainDocumentURL:(nullable NSURL *)mainDocumentURL completionHandler:(nullable void (^)())completionHandler;

/*! @abstract Delete the specified cookie.
 @param completionHandler A block to invoke once the cookie has been deleted.
 */
- (void)deleteCookie:(NSHTTPCookie *)cookie completionHandler:(nullable void (^)())completionHandler;

/*! @abstract Delete all cookies from the cookie storage since the provided date.
 @param date The date after which set cookies should be removed.
 @param completionHandler A block to invoke once the cookies have been deleted.
 */
- (void)removeCookiesSinceDate:(NSDate *)date completionHandler:(nullable void (^)())completionHandler;

/*! @abstract Sets the cookie accept policy preference of the receiver.
 @param policy The cookie accept policy to set.
 @param completionHandler A block to invoke once the policy has been set.
 */
- (void)setCookieAcceptPolicy:(NSHTTPCookieAcceptPolicy)policy completionHandler:(nullable void (^)())completionHandler;

/*! @abstract Fetches the cookie accept policy preference of the receiver.
 @param completionHandler A block to invoke with the fetched policy.
 */
- (void)fetchCookieAcceptPolicy:(void (^)(NSHTTPCookieAcceptPolicy))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif
