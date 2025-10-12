/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#include "Logging.h"
#include <WebCore/GraphicsContextCG.h>
#include <WebCore/IOSurfacePool.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ImageBufferShareableMappedIOSurfaceBackend);

std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> ImageBufferShareableMappedIOSurfaceBackend::create(const Parameters& parameters, const ImageBufferCreationContext& creationContext)
{
    IntSize backendSize = calculateSafeBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    auto surface = IOSurface::create(RefPtr { creationContext.surfacePool }.get(), backendSize, parameters.colorSpace, IOSurface::nameForRenderingPurpose(parameters.purpose), convertToIOSurfaceFormat(parameters.bufferFormat.pixelFormat), parameters.bufferFormat.useLosslessCompression);
    if (!surface)
        return nullptr;
    if (creationContext.resourceOwner)
        surface->setOwnershipIdentity(creationContext.resourceOwner);

    RetainPtr<CGContextRef> cgContext = surface->createPlatformContext();
    if (!cgContext)
        return nullptr;

    CGContextClearRect(cgContext.get(), FloatRect(FloatPoint::zero(), backendSize));

    return std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> { new ImageBufferShareableMappedIOSurfaceBackend { parameters, WTFMove(surface), WTFMove(cgContext), 0, creationContext.surfacePool.get() } };
}

std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> ImageBufferShareableMappedIOSurfaceBackend::create(const Parameters& parameters, ImageBufferBackendHandle handle)
{
    if (!std::holds_alternative<MachSendRight>(handle)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto surface = IOSurface::createFromSendRight(WTFMove(std::get<MachSendRight>(handle)));
    if (!surface)
        return nullptr;
    auto cgContext = surface->createPlatformContext();
    if (!cgContext)
        return nullptr;

    ASSERT(surface->colorSpace() == parameters.colorSpace);
    return std::unique_ptr<ImageBufferShareableMappedIOSurfaceBackend> { new ImageBufferShareableMappedIOSurfaceBackend { parameters, WTFMove(surface), WTFMove(cgContext), 0, nullptr } };
}

std::optional<ImageBufferBackendHandle> ImageBufferShareableMappedIOSurfaceBackend::createBackendHandle(SharedMemory::Protection) const
{
    return ImageBufferBackendHandle(m_surface->createSendRight());
}

String ImageBufferShareableMappedIOSurfaceBackend::debugDescription() const
{
    TextStream stream;
    stream << "ImageBufferShareableMappedIOSurfaceBackend " << this << " " << ValueOrNull(m_surface.get());
    return stream.release();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
