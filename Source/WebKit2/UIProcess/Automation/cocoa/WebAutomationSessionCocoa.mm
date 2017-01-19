/*
 * Copyright (C) 2016, 2017 Apple Inc. All rights reserved.
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
#import "WebAutomationSession.h"

#if PLATFORM(COCOA)

#if PLATFORM(IOS)
#include <ImageIO/CGImageDestination.h>
#include <MobileCoreServices/UTCoreTypes.h>
#endif

using namespace WebCore;

namespace WebKit {

String WebAutomationSession::platformGetBase64EncodedPNGData(const ShareableBitmap::Handle& imageDataHandle)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(imageDataHandle, SharedMemory::Protection::ReadOnly);
    RetainPtr<CGImageRef> cgImage = bitmap->makeCGImage();
    RetainPtr<NSMutableData> imageData = adoptNS([[NSMutableData alloc] init]);
    RetainPtr<CGImageDestinationRef> destination = adoptCF(CGImageDestinationCreateWithData((CFMutableDataRef)imageData.get(), kUTTypePNG, 1, 0));
    if (!destination)
        return String();

    CGImageDestinationAddImage(destination.get(), cgImage.get(), 0);
    CGImageDestinationFinalize(destination.get());

    return [imageData base64EncodedStringWithOptions:0];
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
