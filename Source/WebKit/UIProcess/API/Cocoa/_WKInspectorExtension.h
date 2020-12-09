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

#pragma once

#import <WebKit/WKFoundation.h>

#if TARGET_OS_OSX

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

WK_CLASS_AVAILABLE(macos(WK_MAC_TBA))
@interface _WKInspectorExtension : NSObject

- (instancetype)new NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

/**
 * @abstract Creates a new tab in the Web Inspector interface for this extension.
 * @param tabName A localized display name for the tab.
 * @param tabIconURL The location of an image resource to use for display in the created tab's title.
 * @param sourceURL The location of the main resource to load in the new tab's iframe browsing context.
 * @param completionHandler The completion handler to be called when creating a tab succeeds or fails.
 */
- (void)createTabWithName:(NSString *)tabName tabIconURL:(NSURL *)tabIconURL sourceURL:(NSURL *)sourceURL completionHandler:(void(^)(NSError * _Nullable, NSString * _Nullable inspectorTabID))completionHandler;

@property (readonly, nonatomic) NSString *extensionID;

@end

NS_ASSUME_NONNULL_END

#endif // TARGET_OS_OSX
