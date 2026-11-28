/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <CoreVideo/CoreVideo.h>
#include <WebCore/IntSize.h>
#include <WebCore/PlatformVideoColorSpace.h>
#include <optional>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

typedef struct OpaqueVTPixelTransferSession* VTPixelTransferSessionRef;

namespace WebCore {

class PixelBufferConverter final {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(PixelBufferConverter, WEBCORE_EXPORT);
public:
    enum class UsePixelBufferPool : bool { No, Yes };
    enum class UseIOSurface : bool { No, Yes };
    explicit PixelBufferConverter(UseIOSurface useIOSurface, UsePixelBufferPool usePixelBufferPool = UsePixelBufferPool::Yes)
        : m_useIOSurface(useIOSurface)
        , m_usePixelBufferPool(usePixelBufferPool)
    {
    }
    WEBCORE_EXPORT ~PixelBufferConverter();

    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> convert(CVPixelBufferRef, OSType destinationFormat, const PlatformVideoColorSpace& = { });

    // The nearest Y'CbCr format with alpha that VideoToolbox reads and handles properly (rdar://193713488,
    // rdar://192541461); formats without alpha map to themselves.
    WEBCORE_EXPORT static OSType nearestCVPixelFormat(OSType);
    // The Y'CbCr format with the subsampling, bit depth and range of pixelFormat and an alpha plane at its bit depth, or
    // 16-bit where there is none; formats with alpha map to themselves. It can be a format WebKit registers with CoreVideo.
    WEBCORE_EXPORT static std::optional<OSType> pixelFormatWithAlpha(OSType pixelFormat);
    WEBCORE_EXPORT static std::optional<OSType> pixelFormatWithoutAlpha(OSType);

private:
    struct Pool {
        RetainPtr<CVPixelBufferPoolRef> pool;
        OSType pixelFormat { 0 };
        IntSize size;
    };
    RetainPtr<CVPixelBufferRef> createPixelBuffer(Pool&, OSType, CVPixelBufferRef source);
    RetainPtr<CVPixelBufferRef> copyPlanes(CVPixelBufferRef, OSType destinationFormat, Pool&);
    RetainPtr<CVPixelBufferRef> convertWithVideoToolbox(CVPixelBufferRef, OSType destinationFormat, const PlatformVideoColorSpace&);
    RetainPtr<VTPixelTransferSessionRef> transferSession(const PlatformVideoColorSpace&);

    const UseIOSurface m_useIOSurface;
    const UsePixelBufferPool m_usePixelBufferPool;
    Pool m_outputPool;
    Pool m_colorPlanesPool;
    RetainPtr<VTPixelTransferSessionRef> m_transferSession;
    PlatformVideoColorSpace m_transferSessionColorSpace;
};

} // namespace WebCore
