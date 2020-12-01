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
#include "ImageBufferShareableUnmappedIOSurfaceBackend.h"

#if HAVE(IOSURFACE)

#include <WebCore/GraphicsContextCG.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferShareableUnmappedIOSurfaceBackend);

std::unique_ptr<ImageBufferShareableUnmappedIOSurfaceBackend> ImageBufferShareableUnmappedIOSurfaceBackend::create(const FloatSize& logicalSize, const IntSize& internalSize, float resolutionScale, ColorSpace colorSpace, PixelFormat pixelFormat, ImageBufferBackendHandle handle)
{
    if (!WTF::holds_alternative<MachSendRight>(handle)) {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    return makeUnique<ImageBufferShareableUnmappedIOSurfaceBackend>(logicalSize, internalSize, resolutionScale, colorSpace, pixelFormat, WTFMove(handle));
}

ImageBufferBackendHandle ImageBufferShareableUnmappedIOSurfaceBackend::createImageBufferBackendHandle() const
{
    return WTF::get<MachSendRight>(m_handle).copySendRight();
}

GraphicsContext& ImageBufferShareableUnmappedIOSurfaceBackend::context() const
{
    RELEASE_ASSERT_NOT_REACHED();
    return *(GraphicsContext*)nullptr;
}

RefPtr<NativeImage> ImageBufferShareableUnmappedIOSurfaceBackend::copyNativeImage(BackingStoreCopy) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

RefPtr<Image> ImageBufferShareableUnmappedIOSurfaceBackend::copyImage(BackingStoreCopy, PreserveResolution) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

void ImageBufferShareableUnmappedIOSurfaceBackend::draw(GraphicsContext&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void ImageBufferShareableUnmappedIOSurfaceBackend::drawPattern(GraphicsContext&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, const ImagePaintingOptions&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

String ImageBufferShareableUnmappedIOSurfaceBackend::toDataURL(const String&, Optional<double>, PreserveResolution) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

Vector<uint8_t> ImageBufferShareableUnmappedIOSurfaceBackend::toData(const String&, Optional<double>) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

Vector<uint8_t> ImageBufferShareableUnmappedIOSurfaceBackend::toBGRAData() const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

RefPtr<ImageData> ImageBufferShareableUnmappedIOSurfaceBackend::getImageData(AlphaPremultiplication, const IntRect&) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

void ImageBufferShareableUnmappedIOSurfaceBackend::putImageData(AlphaPremultiplication, const ImageData&, const IntRect&, const IntPoint&, AlphaPremultiplication)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // HAVE(IOSURFACE)
