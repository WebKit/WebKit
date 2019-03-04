/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

/*! A WKPreferences object encapsulates the preference settings for a web
 view. The preferences object associated with a web view is specified by
 its web view configuration.
 */
WK_CLASS_AVAILABLE(macosx(10.10), ios(8.0))
@interface WKPreferences : NSObject <NSSecureCoding>

/*! @abstract The minimum font size in points.
 @discussion The default value is 0.
 */
@property (nonatomic) CGFloat minimumFontSize;

/*! @abstract A Boolean value indicating whether JavaScript is enabled.
 @discussion The default value is YES.
 */
@property (nonatomic) BOOL javaScriptEnabled;

/*! @abstract A Boolean value indicating whether JavaScript can open
 windows without user interaction.
 @discussion The default value is NO in iOS and YES in OS X.
 */
@property (nonatomic) BOOL javaScriptCanOpenWindowsAutomatically;

#if !TARGET_OS_IPHONE
/*! @abstract A Boolean value indicating whether Java is enabled.
 @discussion The default value is NO.
 */
@property (nonatomic) BOOL javaEnabled;

/*! @abstract A Boolean value indicating whether plug-ins are enabled.
 @discussion The default value is NO.
 */
@property (nonatomic) BOOL plugInsEnabled;

/*!
 @property tabFocusesLinks
 @abstract If tabFocusesLinks is YES, the tab key will focus links and form controls.
 The Option key temporarily reverses this preference.
 */
@property (nonatomic) BOOL tabFocusesLinks WK_API_AVAILABLE(macosx(10.12.3));
#endif

@end
