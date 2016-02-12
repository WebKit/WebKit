/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "InteractionInformationAtPosition.h"

#import "ArgumentCodersCF.h"
#import "Arguments.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/DataDetectorsCoreSPI.h>
#import <WebCore/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(DataDetectorsCore)
SOFT_LINK_CLASS(DataDetectorsCore, DDScannerResult)

namespace WebKit {

#if PLATFORM(IOS)

void InteractionInformationAtPosition::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << point;
    encoder << nodeAtPositionIsAssistedNode;
    encoder << isSelectable;
    encoder << isNearMarkedText;
    encoder << touchCalloutEnabled;
    encoder << isLink;
    encoder << isImage;
    encoder << isAnimatedImage;
    encoder << isElement;
    encoder << url;
    encoder << imageURL;
    encoder << title;
    encoder << bounds;
    encoder << linkIndicator;

    ShareableBitmap::Handle handle;
    if (image)
        image->createHandle(handle, SharedMemory::Protection::ReadOnly);
    encoder << handle;
#if ENABLE(DATA_DETECTION)
    encoder << isDataDetectorLink;
    if (isDataDetectorLink) {
        encoder << dataDetectorIdentifier;
        RetainPtr<NSMutableData> data = adoptNS([[NSMutableData alloc] init]);
        RetainPtr<NSKeyedArchiver> archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
        [archiver setRequiresSecureCoding:YES];
        [archiver encodeObject:dataDetectorResults.get() forKey:@"dataDetectorResults"];
        [archiver finishEncoding];
        
        IPC::encode(encoder, reinterpret_cast<CFDataRef>(data.get()));        
    }
#endif
}

bool InteractionInformationAtPosition::decode(IPC::ArgumentDecoder& decoder, InteractionInformationAtPosition& result)
{
    if (!decoder.decode(result.point))
        return false;

    if (!decoder.decode(result.nodeAtPositionIsAssistedNode))
        return false;

    if (!decoder.decode(result.isSelectable))
        return false;

    if (!decoder.decode(result.isNearMarkedText))
        return false;

    if (!decoder.decode(result.touchCalloutEnabled))
        return false;

    if (!decoder.decode(result.isLink))
        return false;

    if (!decoder.decode(result.isImage))
        return false;

    if (!decoder.decode(result.isAnimatedImage))
        return false;
    
    if (!decoder.decode(result.isElement))
        return false;

    if (!decoder.decode(result.url))
        return false;

    if (!decoder.decode(result.imageURL))
        return false;

    if (!decoder.decode(result.title))
        return false;

    if (!decoder.decode(result.bounds))
        return false;

    if (!decoder.decode(result.linkIndicator))
        return false;

    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;

    if (!handle.isNull())
        result.image = ShareableBitmap::create(handle, SharedMemory::Protection::ReadOnly);

#if ENABLE(DATA_DETECTION)
    if (!decoder.decode(result.isDataDetectorLink))
        return false;
    
    if (result.isDataDetectorLink) {
        if (!decoder.decode(result.dataDetectorIdentifier))
            return false;
        RetainPtr<CFDataRef> data;
        if (!IPC::decode(decoder, data))
            return false;
        
        RetainPtr<NSKeyedUnarchiver> unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:(NSData *)data.get()]);
        [unarchiver setRequiresSecureCoding:YES];
        @try {
            result.dataDetectorResults = [unarchiver decodeObjectOfClasses:[NSSet setWithArray:@[ [NSArray class], getDDScannerResultClass()] ] forKey:@"dataDetectorResults"];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to decode NSArray of DDScanResult: %@", exception);
            return false;
        }
        
        [unarchiver finishDecoding];
    }
#endif

    return true;
}
#endif

}
