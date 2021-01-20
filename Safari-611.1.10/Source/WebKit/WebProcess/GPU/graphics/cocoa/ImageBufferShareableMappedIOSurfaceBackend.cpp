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

#include "config.h"
#include "ImageBufferShareableMappedIOSurfaceBackend.h"

#if ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)

#include <WebCore/GraphicsContextCG.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferShareableMappedIOSurfaceBackend);

std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> ImageBufferShareableMappedIOSurfaceBackend::create(const Parameters& parameters, const HostWindow*)
{
    IntSize backendSize = calculateBackendSize(parameters.logicalSize, parameters.resolutionScale);
    if (backendSize.isEmpty())
        return nullptr;

    auto surface = IOSurface::create(backendSize, backendSize, cachedCGColorSpace(parameters.colorSpace), IOSurface::formatForPixelFormat(parameters.pixelFormat));
    if (!surface)
        return nullptr;

    RetainPtr<CGContextRef> cgContext = surface->ensurePlatformContext();
    if (!cgContext)
        return nullptr;

    CGContextClearRect(cgContext.get(), FloatRect(FloatPoint::zero(), backendSize));

    return makeUnique<ImageBufferShareableMappedIOSurfaceBackend>(parameters, WTFMove(surface));
}

std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> ImageBufferShareableMappedIOSurfaceBackend::create(const Parameters& parameters, ImageBufferBackendHandle handle)
{
    if (!WTF::holds_alternative<MachSendRight>(handle)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto surface = IOSurface::createFromSendRight(WTFMove(WTF::get<MachSendRight>(handle)), cachedCGColorSpace(parameters.colorSpace));
    if (!surface)
        return nullptr;

    return makeUnique<ImageBufferShareableMappedIOSurfaceBackend>(parameters, WTFMove(surface));
}

ImageBufferBackendHandle ImageBufferShareableMappedIOSurfaceBackend::createImageBufferBackendHandle() const
{
    return ImageBufferBackendHandle(m_surface->createSendRight());
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
