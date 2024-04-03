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
#import "CoreIPCCVPixelBufferRef.h"

#import <WebCore/IOSurface.h>

#import <WebCore/CoreVideoSoftLink.h>

namespace WebKit {
using namespace WebCore;
MachSendRight CoreIPCCVPixelBufferRef::sendRightFromPixelBuffer(const RetainPtr<CVPixelBufferRef>& pixelBuffer)
{
    auto surface = CVPixelBufferGetIOSurface(pixelBuffer.get());
    return MachSendRight::adopt(IOSurfaceCreateMachPort(surface));
}

RetainPtr<CVPixelBufferRef> CoreIPCCVPixelBufferRef::toCF() const
{
    RetainPtr<CVPixelBufferRef> pixelBuffer;
    if (!m_sendRight)
        return pixelBuffer;
    {
        auto surface = adoptCF(IOSurfaceLookupFromMachPort(m_sendRight.sendRight()));
        if (!surface)
            return nullptr;
        CVPixelBufferRef rawBuffer = nullptr;
        auto status = CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, surface.get(), nullptr, &rawBuffer);
        if (status != noErr || !rawBuffer)
            return nullptr;
        pixelBuffer = adoptCF(rawBuffer);
    }
    return pixelBuffer;
}

} // namespace WebKit
