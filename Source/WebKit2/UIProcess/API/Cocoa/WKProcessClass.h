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

@class WKProcessClassConfiguration;

/*! A WKProcessClass defines a distinct type—or category—of Web Content process.
 A @link WKWebView @/link specifies the WKProcessClass of the Web
 Content process backing it through its @link WKWebViewConfiguration @/link.
 @helperclass @link WKProcessClassConfiguration @/link
 Used to configure @link WKProcessClass @/link instances.
 */
WK_API_CLASS
@interface WKProcessClass : NSObject

/*! @abstract A copy of the configuration with which the @link WKProcessClass @/link was
        initialized.
*/
@property (nonatomic, readonly) WKProcessClassConfiguration *configuration;

/*!
    @abstract Returns an instance initialized with the specified configuration.
    @param configuration The configuration for the new instance.
    @result An initialized instance, or nil if the object could not be initialized.
    @discussion This is a designated initializer. You can use @link -init @/link to initialize an
        instance with the default configuration.

        The initializer copies
        @link //apple_ref/doc/methodparam/WKProcessClass/initWithConfiguration:/configuration
        configuration@/link, so mutating it after initialization has no effect on the
        @link WKProcessClass @/link instance.
*/
- (instancetype)initWithConfiguration:(WKProcessClassConfiguration *)configuration WK_DESIGNATED_INITIALIZER;

@end

#endif
