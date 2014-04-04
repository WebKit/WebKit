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

@class WKPreferences;
@class WKProcessPool;
@class WKUserContentController;

/*! A @link WKWebViewConfiguration @/link is a collection of properties used to initialize a web
 view.
 @helps Contains properties used to configure a @link WKWebView @/link.
 */
WK_API_CLASS
@interface WKWebViewConfiguration : NSObject <NSCopying>

/*! @abstract The process pool from which the Web Content process for the view should come.
 @discussion When the @link WKWebView @/link is initialized with the configuration, a new Web
 Content process from the specified pool will be created for it, or an existing process in
 that pool will be used.
 When this property is set to nil, a unique process pool will be created for each
 @link WKWebView @/link initialized with the configuration.
*/
@property (nonatomic, strong) WKProcessPool *processPool;

/*! @abstract The preferences that should be used by web views created with this configuration.
 @discussion When this property is set to nil, a unique preferences object will be created for each
 @link WKWebView @/link initialized with the configuration.
*/
@property (nonatomic, strong) WKPreferences *preferences;

/*! @abstract The user content controller that should be used by web views created with this configuration.
 @discussion When this property is set to nil, a unique user content controller object will be created for each
 @link WKWebView @/link initialized with the configuration.
*/
@property (nonatomic, strong) WKUserContentController *userContentController;

@end

#endif
