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
#include "ImageBufferShareableIOSurfaceBackend.h"

#if HAVE(IOSURFACE)

#include <WebCore/GraphicsContextCG.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBufferShareableIOSurfaceBackend);

std::unique_ptr<ImageBufferShareableIOSurfaceBackend> ImageBufferShareableIOSurfaceBackend::create(const Parameters& parameters, ImageBufferBackendHandle handle)
{
    if (!WTF::holds_alternative<MachSendRight>(handle)) {
        RELEASE_ASSERT_NOT_REACHED();
        return nullptr;
    }

    return makeUnique<ImageBufferShareableIOSurfaceBackend>(parameters, WTFMove(handle));
}

ImageBufferBackendHandle ImageBufferShareableIOSurfaceBackend::createImageBufferBackendHandle() const
{
    return WTF::get<MachSendRight>(m_handle).copySendRight();
}

GraphicsContext& ImageBufferShareableIOSurfaceBackend::context() const
{
    RELEASE_ASSERT_NOT_REACHED();
    return *(GraphicsContext*)nullptr;
}

RefPtr<NativeImage> ImageBufferShareableIOSurfaceBackend::copyNativeImage(BackingStoreCopy) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

RefPtr<Image> ImageBufferShareableIOSurfaceBackend::copyImage(BackingStoreCopy, PreserveResolution) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

void ImageBufferShareableIOSurfaceBackend::draw(GraphicsContext&, const FloatRect&, const FloatRect&, const ImagePaintingOptions&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

void ImageBufferShareableIOSurfaceBackend::drawPattern(GraphicsContext&, const FloatRect&, const FloatRect&, const AffineTransform&, const FloatPoint&, const FloatSize&, const ImagePaintingOptions&)
{
    RELEASE_ASSERT_NOT_REACHED();
}

String ImageBufferShareableIOSurfaceBackend::toDataURL(const String&, Optional<double>, PreserveResolution) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

Vector<uint8_t> ImageBufferShareableIOSurfaceBackend::toData(const String&, Optional<double>) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

Vector<uint8_t> ImageBufferShareableIOSurfaceBackend::toBGRAData() const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

RefPtr<ImageData> ImageBufferShareableIOSurfaceBackend::getImageData(AlphaPremultiplication, const IntRect&) const
{
    RELEASE_ASSERT_NOT_REACHED();
    return { };
}

void ImageBufferShareableIOSurfaceBackend::putImageData(AlphaPremultiplication, const ImageData&, const IntRect&, const IntPoint&, AlphaPremultiplication)
{
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebKit

#endif // HAVE(IOSURFACE)
