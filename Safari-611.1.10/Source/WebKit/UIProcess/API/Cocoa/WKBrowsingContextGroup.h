/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <WebKit/WKUserScriptInjectionTime.h>

WK_CLASS_DEPRECATED_WITH_REPLACEMENT("WKUserContentController and WKPreferences", macos(10.10, 10.14.4), ios(8.0, 12.2))
@interface WKBrowsingContextGroup : NSObject

- (id)initWithIdentifier:(NSString *)identifier;

/* User Content */

- (void)addUserStyleSheet:(NSString *)source baseURL:(NSURL *)baseURL whitelistedURLPatterns:(NSArray *)whitelist blacklistedURLPatterns:(NSArray *)blacklist mainFrameOnly:(BOOL)mainFrameOnly WK_API_DEPRECATED_WITH_REPLACEMENT("addUserStyleSheet:baseURL:includeMatchPatternStrings:excludeMatchPatternStrings:mainFrameOnly:", macos(10.10, WK_MAC_TBA), ios(8.0, WK_IOS_TBA));
- (void)addUserStyleSheet:(NSString *)source baseURL:(NSURL *)baseURL includeMatchPatternStrings:(NSArray<NSString *> *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray<NSString *> *)excludeMatchPatternStrings mainFrameOnly:(BOOL)mainFrameOnly;

- (void)removeAllUserStyleSheets;

- (void)addUserScript:(NSString *)source baseURL:(NSURL *)baseURL whitelistedURLPatterns:(NSArray *)whitelist blacklistedURLPatterns:(NSArray *)blacklist injectionTime:(_WKUserScriptInjectionTime)injectionTime mainFrameOnly:(BOOL)mainFrameOnly WK_API_DEPRECATED_WITH_REPLACEMENT("addUserScript:baseURL:includeMatchPatternStrings:excludeMatchPatternStrings:injectionTime:mainFrameOnly:", macos(10.10, WK_MAC_TBA), ios(8.0, WK_IOS_TBA));
- (void)addUserScript:(NSString *)source baseURL:(NSURL *)baseURL includeMatchPatternStrings:(NSArray<NSString *> *)includeMatchPatternStrings excludeMatchPatternStrings:(NSArray<NSString *> *)excludeMatchPatternStrings injectionTime:(_WKUserScriptInjectionTime)injectionTime mainFrameOnly:(BOOL)mainFrameOnly;
- (void)removeAllUserScripts;


/* Settings */

/* Setting to control whether JavaScript is enabled.
   Default: YES
*/
@property BOOL allowsJavaScript;

/* Setting to control whether plug-ins are enabled.
   Default: YES
*/
@property BOOL allowsPlugIns;

/* Setting to control whether private browsing is enabled.
 Default: NO
 */
@property BOOL privateBrowsingEnabled;

@end
