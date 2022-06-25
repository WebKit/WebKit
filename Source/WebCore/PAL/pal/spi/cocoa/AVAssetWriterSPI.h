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

#if USE(APPLE_INTERNAL_SDK)

#import <AVFoundation/AVAssetWriter_Private.h>

#else

NS_ASSUME_NONNULL_BEGIN

@interface AVFragmentedMediaDataReport : NSObject
@end

#if !HAVE(AVASSETWRITERDELEGATE_API)
@protocol AVAssetWriterDelegate <NSObject>
@optional
- (void)assetWriter:(AVAssetWriter *)assetWriter didProduceFragmentedHeaderData:(NSData *)fragmentedHeaderData;
- (void)assetWriter:(AVAssetWriter *)assetWriter didProduceFragmentedMediaData:(NSData *)fragmentedMediaData fragmentedMediaDataReport:(AVFragmentedMediaDataReport *)fragmentedMediaDataReport;
@end
#endif

@interface AVAssetWriter ()
- (nullable instancetype)initWithFileType:(NSString * _Nullable)outputFileType error:(NSError * _Nullable * _Nullable)outError;
- (void)flush;
#if !HAVE(AVASSETWRITERDELEGATE_API)
@property (weak, nullable) id <AVAssetWriterDelegate> delegate SPI_AVAILABLE(macos(10.15), ios(13.0), tvos(13.0), watchos(6.0));
#endif
@end

NS_ASSUME_NONNULL_END

#endif // USE(APPLE_INTERNAL_SDK)
