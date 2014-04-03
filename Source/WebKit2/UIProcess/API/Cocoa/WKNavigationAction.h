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

@class WKFrameInfo;

/*! @enum WKNavigationType
 @abstract the type of action that triggered a possible navigation.
 @constant WKNavigationTypeLinkActivated    A link with an href activated by the user.
 @constant WKNavigationTypeFormSubmitted    A form was submitted.
 @constant WKNavigationTypeBackForward      The user requested an item from the back-forward list.
 @constant WKNavigationTypeReload           The user reloaded a page.
 @constant WKNavigationTypeFormResubmitted  A form as resubmitted (for example by going back, forward or reloading).
 @constant WKNavigationTypeOther            Navigation is taking place for some other reason.
 */
typedef NS_ENUM(NSInteger, WKNavigationType) {
    WKNavigationTypeLinkActivated,
    WKNavigationTypeFormSubmitted,
    WKNavigationTypeBackForward,
    WKNavigationTypeReload,
    WKNavigationTypeFormResubmitted,
    WKNavigationTypeOther = -1,
};


/*! Contains information about an action that may cause a navigation, used for making policy decisions.
 */
WK_API_CLASS
@interface WKNavigationAction : NSObject

/*! @abstract Represents the frame that is requesting the navigation.
 */
@property (nonatomic, readonly) WKFrameInfo *sourceFrame;

/*! @abstract Represents the target frame. If this is a new window navigation, targetFrame will be nil.
 */
@property (nonatomic, readonly) WKFrameInfo *targetFrame;

/*! @abstract The type of the navigation.
 */
@property (nonatomic, readonly) WKNavigationType navigationType;

/*! @abstract The NSURLRequest of the navigation.
 */
@property (nonatomic, readonly) NSURLRequest *request;

@end

#endif
