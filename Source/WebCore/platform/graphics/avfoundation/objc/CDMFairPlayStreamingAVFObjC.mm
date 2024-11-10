/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CDMFairPlayStreaming.h"

#if ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)

#import "SharedBuffer.h"
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/Ref.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebCore {

Vector<Ref<SharedBuffer>> CDMPrivateFairPlayStreaming::keyIDsForRequest(AVContentKeyRequest *request)
{
    if (auto *identiferStr = dynamic_objc_cast<NSString>(request.identifier))
        return { SharedBuffer::create([identiferStr dataUsingEncoding:NSUTF8StringEncoding]) };
    if (auto *identifierData = dynamic_objc_cast<NSData>(request.identifier))
        return { SharedBuffer::create(identifierData) };
    if (request.initializationData) {
        if (auto sinfKeyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsSinf(SharedBuffer::create(request.initializationData)))
            return WTFMove(sinfKeyIDs.value());
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
        if (auto mptsKeyIDs = CDMPrivateFairPlayStreaming::extractKeyIDsMpts(SharedBuffer::create(request.initializationData)))
            return WTFMove(mptsKeyIDs.value());
#endif
    }
    return { };
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA) && HAVE(AVCONTENTKEYSESSION)
