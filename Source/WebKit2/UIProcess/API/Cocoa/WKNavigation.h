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

#import <Foundation/Foundation.h>
#import <WebKit2/WKFoundation.h>

#if WK_API_ENABLED

/*! WKNavigation objects are returned from  the @link WKWebView @/link load methods and are passed to the 
 @link WKNavigationDelegate @/link methods, and can be used to track page loads from start to finish.
 */
WK_API_CLASS
@interface WKNavigation : NSObject

/*! @abstract The initial NSURLRequest used to perform the navigation.
 */
@property (nonatomic, readonly) NSURLRequest *initialRequest;

/*! @abstract The current request of the navigation. This request may be different from the request returned by initialRequest if server side redirects have happened.
 */
@property (nonatomic, readonly) NSURLRequest *request;

/* @abstract The NSURLResponse for the navigation or nil if no response has been received yet.
 */
@property (nonatomic, readonly) NSURLResponse *response;

/* @abstract The page load error if the page failed to load, nil otherwise.
 */
@property (nonatomic, readonly) NSError *error;

@end

#endif
