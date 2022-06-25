/*
 * Copyright (C) 2003 Apple Inc.  All rights reserved.
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

#ifndef WebDownload_h
#define WebDownload_h

#import <Foundation/Foundation.h>
#import <WebKitLegacy/WebKitAvailability.h>

#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
#import <CFNetwork/CFNSURLConnection.h>
#elif !TARGET_OS_IPHONE || (defined(USE_APPLE_INTERNAL_SDK) && USE_APPLE_INTERNAL_SDK)
#import <Foundation/NSURLDownload.h>
#else
#import <WebKitLegacy/NSURLDownloadSPI.h>
#endif

#if TARGET_OS_IPHONE
#import <WebKitLegacy/WAKAppKitStubs.h>
#endif

#if !TARGET_OS_IPHONE
@class NSWindow;
#endif
@class WebDownloadInternal;

/*!
    @class WebDownload
    @discussion A WebDownload works just like an NSURLDownload, with
    one extra feature: if you do not implement the
    authentication-related delegate methods, it will automatically
    prompt for authentication using the standard WebKit authentication
    panel, as either a sheet or window. It provides no extra methods,
    but does have one additional delegate method.
*/
WEBKIT_CLASS_DEPRECATED_MAC(10_4, 10_14)
@interface WebDownload : NSURLDownload
{
@package
    WebDownloadInternal *_webInternal;
}

@end

/*!
    @protocol WebDownloadDelegate
    @discussion The WebDownloadDelegate delegate has one extra method used to choose
    the right window when automatically prompting with a sheet.
*/
WEBKIT_DEPRECATED_MAC(10_4, 10_14)
@protocol WebDownloadDelegate <NSURLDownloadDelegate>

@optional

/*!
    @method downloadWindowForAuthenticationSheet:
*/
#if TARGET_OS_IPHONE
- (WAKWindow *)downloadWindowForAuthenticationSheet:(WebDownload *)download;
#else
- (NSWindow *)downloadWindowForAuthenticationSheet:(WebDownload *)download;
#endif

@end

#endif /* WebDownload_h */
