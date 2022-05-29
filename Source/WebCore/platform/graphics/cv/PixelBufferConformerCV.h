/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "DestinationColorSpace.h"
#include "FourCC.h"
#include <wtf/RetainPtr.h>

typedef struct CGColorSpace *CGColorSpaceRef;
typedef struct CGImage* CGImageRef;
typedef struct OpaqueVTPixelTransferSession* VTPixelTransferSessionRef;
typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct __CVPixelBufferPool* CVPixelBufferPoolRef;


namespace WebCore {

class PixelBufferConformerCV {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT PixelBufferConformerCV(FourCC&& pixelFormat, const DestinationColorSpace&);
    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> convert(CVPixelBufferRef);
    WEBCORE_EXPORT RetainPtr<CGImageRef> createImageFromPixelBuffer(CVPixelBufferRef);
    static WEBCORE_EXPORT RetainPtr<CGImageRef> imageFrom32BGRAPixelBuffer(RetainPtr<CVPixelBufferRef>&&, CGColorSpaceRef);

    const DestinationColorSpace& destinationColorSpace() const { return m_destinationColorSpace; }

private:
    bool isConformantPixelBuffer(CVPixelBufferRef) const;

    FourCC m_pixelFormat;
    DestinationColorSpace m_destinationColorSpace;
    RetainPtr<VTPixelTransferSessionRef> m_pixelTransferSession;
    size_t m_lastWidth { 0 };
    size_t m_lastHeight { 0 };
    RetainPtr<CVPixelBufferPoolRef> m_pixelBufferPool;
};

}
