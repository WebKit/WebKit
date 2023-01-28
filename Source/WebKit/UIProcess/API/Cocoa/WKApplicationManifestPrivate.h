/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import <WebKit/WKApplicationManifest.h>
#import <WebKit/_WKApplicationManifest.h>

NS_ASSUME_NONNULL_BEGIN

@interface WKApplicationManifest (WKPrivate)

@property (nonatomic, readonly, nullable, copy) NSString *_name;
@property (nonatomic, readonly, nullable, copy) NSString *_shortName;
@property (nonatomic, readonly, nullable, copy) NSString *_applicationDescription;
@property (nonatomic, readonly, nullable, copy) NSURL *_scope;
@property (nonatomic, readonly, copy) NSURL *_startURL;
@property (nonatomic, readonly, copy) NSURL *_manifestId;
@property (nonatomic, readonly) _WKApplicationManifestDisplayMode _displayMode;
@property (nonatomic, readonly, copy) NSArray<_WKApplicationManifestIcon *> *_icons;

#if TARGET_OS_IPHONE
@property (nonatomic, readonly, nullable, copy) UIColor *_themeColor;
#else
@property (nonatomic, readonly, nullable, copy) NSColor *_themeColor;
#endif

@end

NS_ASSUME_NONNULL_END
