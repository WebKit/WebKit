/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

/*! @constant WKErrorDomain Indicates a WebKit error. */
WK_EXTERN NSString * const WKErrorDomain WK_AVAILABLE(10_10, 8_0);

/*! @enum WKErrorCode
 @abstract Constants used by NSError to indicate errors in the WebKit domain.
 @constant WKErrorUnknown                       Indicates that an unknown error occurred.
 @constant WKErrorWebContentProcessTerminated   Indicates that the Web Content process was terminated.
 @constant WKErrorWebViewInvalidated            Indicates that the WKWebView was invalidated.
 @constant WKErrorJavaScriptExceptionOccurred   Indicates that a JavaScript exception occurred.
 */
typedef NS_ENUM(NSInteger, WKErrorCode) {
    WKErrorUnknown = 1,
    WKErrorWebContentProcessTerminated,
    WKErrorWebViewInvalidated,
    WKErrorJavaScriptExceptionOccurred,
} WK_ENUM_AVAILABLE(10_10, 8_0);

#endif
