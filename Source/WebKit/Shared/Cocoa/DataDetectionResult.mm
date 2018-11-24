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

#import "config.h"
#import "DataDetectionResult.h"

#import "ArgumentCodersCF.h"
#import "WebCoreArgumentCoders.h"
#import <pal/spi/cocoa/DataDetectorsCoreSPI.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(DataDetectorsCore)
SOFT_LINK_CLASS(DataDetectorsCore, DDScannerResult)

namespace WebKit {

#if ENABLE(DATA_DETECTION)

void DataDetectionResult::encode(IPC::Encoder& encoder) const
{
    auto archiver = secureArchiver();
    [archiver encodeObject:results.get() forKey:@"dataDetectorResults"];
    IPC::encode(encoder, (__bridge CFDataRef)archiver.get().encodedData);
}

std::optional<DataDetectionResult> DataDetectionResult::decode(IPC::Decoder& decoder)
{
    RetainPtr<CFDataRef> data;
    if (!IPC::decode(decoder, data))
        return std::nullopt;

    DataDetectionResult result;
    auto unarchiver = secureUnarchiverFromData((__bridge NSData *)data.get());
    @try {
        result.results = [unarchiver decodeObjectOfClasses:[NSSet setWithArray:@[ [NSArray class], getDDScannerResultClass()] ] forKey:@"dataDetectorResults"];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode NSArray of DDScanResult: %@", exception);
        return std::nullopt;
    }
    
    [unarchiver finishDecoding];
    return { WTFMove(result) };
}
#endif

}
