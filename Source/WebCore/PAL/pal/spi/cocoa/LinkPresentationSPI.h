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

#pragma once

#if !PLATFORM(WATCHOS) || USE(APPLE_INTERNAL_SDK)
#import <LinkPresentation/LinkPresentation.h>
#else
#import <Foundation/Foundation.h>
@interface LPLinkMetadata : NSObject<NSCopying, NSSecureCoding>
@property (nonatomic, retain) NSURL *URL;
@property (nonatomic, copy) NSString *title;
@end

@interface LPMetadataProvider : NSObject
@end
#endif

#if USE(APPLE_INTERNAL_SDK)

#import <LinkPresentation/LPMetadata.h>
#import <LinkPresentation/LPMetadataProviderPrivate.h>

#else

@interface LPLinkMetadata ()
- (void)_setIncomplete:(BOOL)incomplete;
@end

@interface LPMetadataProvider ()
- (LPLinkMetadata *)_startFetchingMetadataForURL:(NSURL *)URL completionHandler:(void(^)(NSError *))completionHandler;
@end

#endif // USE(APPLE_INTERNAL_SDK)
