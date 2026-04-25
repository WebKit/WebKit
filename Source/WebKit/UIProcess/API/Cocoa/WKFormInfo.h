/*
* Copyright (C) 2026 Apple Inc. All rights reserved.
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

@class WKFrameInfo;

/*! @abstract A WKFormInfo object contains information about an in-progress form submission happening in a WKWebView
 @discussion An instance of this class is a transient, data-only object;
 it does not uniquely identify a form across multiple delegate method calls.
 */

NS_SWIFT_UI_ACTOR
WK_CLASS_AVAILABLE(macos(WK_MAC_TBA), ios(WK_IOS_TBA), visionos(WK_XROS_TBA))
@interface WKFormInfo : NSObject

/*! @abstract The frame where the form is being submitted will cause a navigation.
 */
@property (nonatomic, readonly) WKFrameInfo *targetFrame;

/*! @abstract The frame that caused the form submission.
 */
@property (nonatomic, readonly) WKFrameInfo *sourceFrame;

/*! @abstract The URL that the frame is being navigated to.
 */
@property (nonatomic, readonly) NSURL *submissionURL;

/*! @abstract The HTTP method used to submit the form; generally either @"GET" or @"POST".
 */
@property (nonatomic, readonly) NSString *httpMethod;

/*! @abstract A dictionary of the form values that will be submitted during the navigation.
 */
@property (nonatomic, readonly) NSDictionary<NSString *, NSString *> *formValues;

@end

NS_ASSUME_NONNULL_END
