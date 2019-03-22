/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, _WKApplicationManifestDisplayMode) {
    _WKApplicationManifestDisplayModeBrowser,
    _WKApplicationManifestDisplayModeMinimalUI,
    _WKApplicationManifestDisplayModeStandalone,
    _WKApplicationManifestDisplayModeFullScreen,
} WK_API_AVAILABLE(macos(10.13.4), ios(11.3));

WK_CLASS_AVAILABLE(macos(10.13.4), ios(11.3))
@interface _WKApplicationManifest : NSObject <NSSecureCoding>

@property (nonatomic, readonly, nullable, copy) NSString *name;
@property (nonatomic, readonly, nullable, copy) NSString *shortName;
@property (nonatomic, readonly, nullable, copy) NSString *applicationDescription;
@property (nonatomic, readonly, nullable, copy) NSURL *scope;
@property (nonatomic, readonly, copy) NSURL *startURL;
@property (nonatomic, readonly) _WKApplicationManifestDisplayMode displayMode;

+ (_WKApplicationManifest *)applicationManifestFromJSON:(NSString *)json manifestURL:(nullable NSURL *)manifestURL documentURL:(nullable NSURL *)documentURL;

@end

NS_ASSUME_NONNULL_END
