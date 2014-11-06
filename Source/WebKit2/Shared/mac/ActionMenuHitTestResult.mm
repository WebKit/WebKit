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
#import "ActionMenuHitTestResult.h"

#import "ArgumentCodersCF.h"
#import "ArgumentDecoder.h"
#import "ArgumentEncoder.h"
#import "TextIndicator.h"
#import "WebCoreArgumentCoders.h"
#import <WebCore/DataDetectorsSPI.h>

namespace WebKit {

void ActionMenuHitTestResult::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << hitTestLocationInViewCooordinates;
    encoder << hitTestResult;
    encoder << lookupText;

    ShareableBitmap::Handle handle;

    // FIXME: We should consider sharing the raw original resource data so that metadata and whatnot are preserved.
    if (image)
        image->createHandle(handle, SharedMemory::ReadOnly);

    encoder << handle;

    bool hasActionContext = actionContext;
    encoder << hasActionContext;
    if (hasActionContext) {
        RetainPtr<NSMutableData> data = adoptNS([[NSMutableData alloc] init]);
        RetainPtr<NSKeyedArchiver> archiver = adoptNS([[NSKeyedArchiver alloc] initForWritingWithMutableData:data.get()]);
        [archiver setRequiresSecureCoding:YES];
        [archiver encodeObject:actionContext.get() forKey:@"actionContext"];
        [archiver finishEncoding];

        IPC::encode(encoder, reinterpret_cast<CFDataRef>(data.get()));

        encoder << detectedDataBoundingBox;

        bool hasTextIndicator = detectedDataTextIndicator;
        encoder << hasTextIndicator;
        if (hasTextIndicator)
            encoder << detectedDataTextIndicator->data();
    }
}

bool ActionMenuHitTestResult::decode(IPC::ArgumentDecoder& decoder, ActionMenuHitTestResult& actionMenuHitTestResult)
{
    if (!decoder.decode(actionMenuHitTestResult.hitTestLocationInViewCooordinates))
        return false;

    if (!decoder.decode(actionMenuHitTestResult.hitTestResult))
        return false;

    if (!decoder.decode(actionMenuHitTestResult.lookupText))
        return false;

    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;

    if (!handle.isNull())
        actionMenuHitTestResult.image = ShareableBitmap::create(handle, SharedMemory::ReadOnly);

    bool hasActionContext;
    if (!decoder.decode(hasActionContext))
        return false;

    if (hasActionContext) {
        RetainPtr<CFDataRef> data;
        if (!IPC::decode(decoder, data))
            return false;

        RetainPtr<NSKeyedUnarchiver> unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingWithData:(NSData *)data.get()]);
        [unarchiver setRequiresSecureCoding:YES];
        @try {
            actionMenuHitTestResult.actionContext = [unarchiver decodeObjectOfClass:getDDActionContextClass() forKey:@"actionContext"];
        } @catch (NSException *exception) {
            LOG_ERROR("Failed to decode DDActionContext: %@", exception);
            return false;
        }
        
        [unarchiver finishDecoding];

        if (!decoder.decode(actionMenuHitTestResult.detectedDataBoundingBox))
            return false;

        bool hasTextIndicator;
        if (!decoder.decode(hasTextIndicator))
            return false;

        if (hasTextIndicator) {
            TextIndicator::Data indicatorData;
            if (!decoder.decode(indicatorData))
                return false;

            actionMenuHitTestResult.detectedDataTextIndicator = TextIndicator::create(indicatorData);
        }
    }

    return true;
}
    
} // namespace WebKit
