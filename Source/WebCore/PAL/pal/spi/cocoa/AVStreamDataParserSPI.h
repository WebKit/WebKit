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

#if !__has_feature(modules)

DECLARE_SYSTEM_HEADER

#include <CoreMedia/CoreMedia.h>

#if USE(APPLE_INTERNAL_SDK)
#import <AVFoundation/AVStreamDataParser.h>
#else

NS_ASSUME_NONNULL_BEGIN

@protocol AVStreamDataParserOutputHandling;
@class AVStreamDataParserInternal;

@protocol AVStreamDataParserOutputHandling <NSObject>
@optional
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didParseStreamDataAsAsset:(AVAsset *)asset withDiscontinuity:(BOOL)withDiscontinuity;
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didFailToParseStreamDataWithError:(NSError *)err;
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideMediaData:(CMSampleBufferRef)mediaData forTrackID:(CMPersistentTrackID)trackID mediaType:(AVMediaType)mediaType flags:(AVStreamDataParserOutputMediaDataFlags)flags;
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didReachEndOfTrackWithTrackID:(CMPersistentTrackID)trackID mediaType:(AVMediaType)mediaType;
- (void)streamDataParserWillProvideContentKeyRequestInitializationData:(AVStreamDataParser *)streamDataParser forTrackID:(CMPersistentTrackID)trackID;
- (void)streamDataParser:(AVStreamDataParser *)streamDataParser didProvideContentKeyRequestInitializationData:(NSData *)contentKeyRequestInitializationData forTrackID:(CMPersistentTrackID)trackID;

@end

@interface AVStreamDataParser (AVStreamDataParserTypeSupport)
+ (NSString *)outputMIMECodecParameterForInputMIMECodecParameter:(NSString *)inputMIMECodecParameter;

@end

@interface AVStreamDataParser (AVStreamDataParserContentKeyEligibility) <AVContentKeyRecipient>
@end

NS_ASSUME_NONNULL_END

#endif

#endif // !__has_feature(modules)
