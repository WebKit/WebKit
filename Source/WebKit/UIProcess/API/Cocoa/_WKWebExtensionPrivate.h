/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#import <WebKit/_WKWebExtension.h>

NS_ASSUME_NONNULL_BEGIN

@interface _WKWebExtension ()

- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest;
- (instancetype)_initWithManifestDictionary:(NSDictionary<NSString *, id> *)manifest resources:(nullable NSDictionary<NSString *, id> *)resources NS_DESIGNATED_INITIALIZER;

- (instancetype)_initWithResources:(NSDictionary<NSString *, id> *)resources NS_DESIGNATED_INITIALIZER;

/*! @abstract A Boolean value indicating whether the extension use modules for the background content. */
@property (readonly, nonatomic) BOOL _backgroundContentUsesModules;

/*!
 @abstract Checks if the extension has script or stylesheet content that can be injected into the specified URL.
 @param url The webpage URL to check.
 @result Returns `YES` if the extension has content that can be injected by matching the `url` against the extension's requested match patterns.
 @discussion The extension will still need to be loaded and have granted website permissions for its content to actually be injected.
 */
- (BOOL)_hasStaticInjectedContentForURL:(NSURL *)url;

@end

NS_ASSUME_NONNULL_END
