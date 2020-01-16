/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

NS_ASSUME_NONNULL_BEGIN

/*!
A WKContentWorld object allows you to seperate your application's interaction with content displayed in a WKWebView into different roles that cannot interfere with one another.
*/
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA))
@interface _WKContentWorld : NSObject

+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/*! @abstract Retrieve the main world that page content itself uses.
 @discussion When interacting with page content in a WKWebView using the page content world you can disrupt the operation of page content (e.g. by conflicting with variable names in JavaScript set by the web page content itself).
*/
@property (class, nonatomic, readonly) _WKContentWorld *pageContentWorld;

/*! @abstract Retrieve the default non-page content world for API clients.
 @discussion Interacting with page content in a WKWebView using a content world different from the page content world enables using DOM and built-in DOM APIs
 without conflicting with other aspects of the page content (e.g. JavaScript from the web page content itself)
*/
@property (class, nonatomic, readonly) _WKContentWorld *defaultClientWorld;

/*! @abstract Retrieves a named content world for API users.
 @param name The name of the WKContentWorld to retrieve.
 @discussion By interacting with page content in a WKWebView using a non-main content world you can interact with the DOM and built-in DOM APIs
 without conflicting with other aspects of the page content (e.g. JavaScript from the web content itself)
 Repeated calls with the same name will retrieve the same WKContentWorld instance.
 Each named content world is distinct from all other named content worlds, the defaultClientWorld, and the pageContentWorld.
 The name can be used to keep distinct worlds identifiable anywhere a world might be surfaced in a user interface.
 For example, the different worlds used in your application will be surfaced by name in the WebKit Web Inspector.
*/
+ (_WKContentWorld *)worldWithName:(NSString *)name NS_REFINED_FOR_SWIFT;

/*! @abstract The name of the WKContentWorld
 @discussion The pageContentWorld and defaultClientWorld instances will have a nil name.
 All other instances will have the non-nil name they were accessed by.
*/
@property (nullable, nonatomic, readonly, copy) NSString *name;

@end

NS_ASSUME_NONNULL_END
