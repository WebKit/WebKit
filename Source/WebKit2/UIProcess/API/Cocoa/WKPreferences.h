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

#include <WebKit2/WKPreferencesRef.h>

#ifdef __OBJC__

#import <Foundation/Foundation.h>
#import <WebKit2/WKFoundation.h>

#if WK_API_ENABLED

/*! WKPreferences encapsulates the preferences you can change for one or more WKWebViews. 
 A @link WKWebView @/link can specify which WKPreferences object it uses through its @link WKWebViewConfiguration @/link.
 */
WK_API_CLASS
@interface WKPreferences : NSObject

/*! @abstract Returns an initialized WKPreferences object.
 @param userDefaultsKeyPrefix The user defaults key prefix.
 @discussion If the userDefaultsKeyPrefix argument is non-nil, it is prepended to the keys used to store preferences
 in the user defaults database. If the argument is nil, the preferences object won't save anything to the user defaults database.
 */
- (instancetype)initWithUserDefaultsKeyPrefix:(NSString *)userDefaultsKeyPrefix WK_DESIGNATED_INITIALIZER;

/*! @abstract The user defaults key prefix.
 */
@property (nonatomic, readonly) NSString *userDefaultsKeyPrefix;

/*! @abstract The minimum font size in points. Defaults to 0.
 */
@property (nonatomic) CGFloat minimumFontSize;

/*! @abstract Whether JavaScript is enabled. Defaults to YES.
 */
@property (nonatomic, getter=isJavaScriptEnabled) BOOL javaScriptEnabled;

/*! @abstract Whether JavaScript can open windows without user interaction. Defaults to NO on iOS and YES on OS X.
 */
@property (nonatomic) BOOL javaScriptCanOpenWindowsAutomatically;

/*! @abstract Whether the WKWebView suppresses content rendering until it is fully loaded into memory. Defaults to NO.
 */
@property (nonatomic) BOOL suppressesIncrementalRendering;

#if TARGET_OS_IPHONE
/*! @abstract Whether HTML5 videos play inline or use the native full-screen controller. Defaults to NO.
 */
@property (nonatomic) BOOL allowsInlineMediaPlayback;

/*! @abstract Whether HTML5 videos can play automatically or require the user to start playing them. Defaults to YES.
 */
@property (nonatomic) BOOL mediaPlaybackRequiresUserAction;

/*! @abstract Whether AirPlay is allowed. Defaults to YES.
 */
@property (nonatomic) BOOL mediaPlaybackAllowsAirPlay;

#endif

#if !TARGET_OS_IPHONE
/*! @abstract Whether Java is enabled. Defaults to YES.
 */
@property (nonatomic, getter=isJavaEnabled) BOOL javaEnabled;

/*! abstract Whether plug-ins are enabled. Defaults to YES.
 */
@property (nonatomic, getter=arePlugInsEnabled) BOOL plugInsEnabled;
#endif

@end

#endif // WK_API_ENABLED

#endif // defined(__OBJC__)
