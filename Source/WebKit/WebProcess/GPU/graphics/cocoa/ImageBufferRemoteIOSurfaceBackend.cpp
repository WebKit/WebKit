/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include "ImageBufferRemoteIOSurfaceBackend.h"

#if HAVE(IOSURFACE)

#include "Logging.h"
#include <WebCore/GraphicsContextCG.h>
#include <WebCore/ImageBufferIOSurfaceBackend.h>
#include <WebCore/PixelBuffer.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

static TextStream& operator<<(TextStream& ts, const ImageBufferBackendHandle& handle)
{
    WTF::switchOn(handle,
        [&] (const ShareableBitmap::Handle& handle) {
            if (handle.isNull())
                ts << "null";
            else
                ts << "ShareableBitmap::Handle " << &handle;
        },
        [&] (const MachSendRight& machSendRight) {
            ts << "MachSendRight " << machSendRight.sendRight();
        }
#if ENABLE(CG_DISPLAY_LIST_BACKED_IMAGE_BUFFER)
        , [&] (const CGDisplayList& handle) {
            ts << "CGDisplayList handle " << &handle;
        }
#endif
    );
    return ts;
}

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferRemoteIOSurfaceBackend);

IntSize ImageBufferRemoteIOSurfaceBackend::calculateSafeBackendSize(const Parameters& parameters)
{
    return ImageBufferIOSurfaceBackend::calculateSafeBackendSize(parameters);
}

size_t ImageBufferRemoteIOSurfaceBackend::calculateMemoryCost(const Parameters& parameters)
{
    return ImageBufferIOSurfaceBackend::calculateMemoryCost(parameters);
}

size_t ImageBufferRemoteIOSurfaceBackend::calculateExternalMemoryCost(const Parameters& parameters)
{
    return ImageBufferIOSurfaceBackend::calculateExternalMemoryCost(parameters);
}

std::unique_ptr<ImageBufferRemoteIOSurfaceBackend> ImageBufferRemoteIOSurfaceBackend::create(const Parameters& parameters, ImageBufferBackendHandle handle)
{
    if (!std::holds_alternative<MachSendRight>(handle)) {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    return makeUnique<ImageBufferRemoteIOSurfaceBackend>(parameters, WTFMove(handle));
}

ImageBufferBackendHandle ImageBufferRemoteIOSurfaceBackend::createBackendHandle(SharedMemory::Protection) const
{
    if (!std::holds_alternative<MachSendRight>(m_handle)) {
        RELEASE_ASSERT_NOT_REACHED();
        return { };
    }

    return MachSendRight { std::get<MachSendRight>(m_handle) };
}

ImageBufferBackendHandle ImageBufferRemoteIOSurfaceBackend::takeBackendHandle(SharedMemory::Protection)
{
    return std::exchange(m_handle, { });
}

void ImageBufferRemoteIOSurfaceBackend::setBackendHandle(ImageBufferBackendHandle&& handle)
{
    m_handle = WTFMove(handle);
}

void ImageBufferRemoteIOSurfaceBackend::clearBackendHandle()
{
    m_handle = { };
}

bool ImageBufferRemoteIOSurfaceBackend::hasBackendHandle() const
{
    return std::holds_alternative<MachSendRight>(m_handle);
}

GraphicsContext& ImageBufferRemoteIOSurfaceBackend::context()
{
    RELEASE_ASSERT_NOT_REACHED();
    return *(GraphicsContext*)nullptr;
}

IntSize ImageBufferRemoteIOSurfaceBackend::backendSize() const
{
    return calculateBackendSize(m_parameters);
}

unsigned ImageBufferRemoteIOSurfaceBackend::bytesPerRow() const
{
    IntSize backendSize = calculateBackendSize(m_parameters);
    return ImageBufferIOSurfaceBackend::calculateBytesPerRow(backendSize);
}

RefPtr<NativeImage> ImageBufferRemoteIOSurfaceBackend::copyNativeImage(BackingStoreCopy)
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

void ImageBufferRemoteIOSurfaceBackend::getPixelBuffer(const IntRect&, PixelBuffer&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void ImageBufferRemoteIOSurfaceBackend::putPixelBuffer(const PixelBuffer&, const IntRect&, const IntPoint&, AlphaPremultiplication)
{
    RELEASE_ASSERT_NOT_REACHED();
}

String ImageBufferRemoteIOSurfaceBackend::debugDescription() const
{
    TextStream stream;
    stream << "ImageBufferRemoteIOSurfaceBackend " << this << " handle " << m_handle << " " << m_volatilityState;
    return stream.release();
}


} // namespace WebKit

#endif // HAVE(IOSURFACE)
