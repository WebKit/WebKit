/*
 * Copyright (C) 2020-2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NativeImage.h"

#include "FloatRect.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderingMode.h"
#include <wtf/Locker.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NativeImage);

#if !USE(CG) && !USE(SKIA)
RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage, std::optional<GainMap>&& gainMap)
{
    if (!platformImage)
        return nullptr;
    return adoptRef(*new NativeImage(WTF::move(platformImage), WTF::move(gainMap)));
}

RefPtr<NativeImage> NativeImage::create(PlatformImagePtr&& platformImage)
{
    return create(WTF::move(platformImage), std::nullopt);
}

RefPtr<NativeImage> NativeImage::createTransient(PlatformImagePtr&& platformImage)
{
    return create(WTF::move(platformImage));
}
#endif

NativeImage::NativeImage() = default;

#if !USE(SKIA)
NativeImage::NativeImage(PlatformImagePtr&& platformImage, std::optional<GainMap>&& gainMap)
    : m_platformImage(WTF::move(platformImage))
    , m_gainMap(WTF::move(gainMap))
{
    computeHeadroom();
}
#endif

NativeImage::~NativeImage()
{
    for (CheckedRef observer : m_observers)
        observer->willDestroyNativeImage(*this);
}

PlatformImagePtr NativeImage::platformImage() const
{
    Locker locker { m_lock };
    return m_platformImage;
}

const std::optional<GainMap>& NativeImage::gainMap() const
{
    return m_gainMap;
}

bool NativeImage::hasHDRContent() const
{
    return colorSpace().usesITUR_2100TF();
}

void NativeImage::replacePlatformImage(PlatformImagePtr&& platformImage) const
{
    ASSERT(platformImage);
    Locker locker { m_lock };
    m_platformImage = WTF::move(platformImage);
    // Intention is that the contents do not change, so properties are not recomputed.
}

#if !USE(CG)
size_t NativeImage::sizeInBytes() const
{
    return size().area() * sizeof(uint32_t);
}

void NativeImage::computeHeadroom() const
{
}

RefPtr<NativeImage> NativeImage::rotatedImage(ImageOrientation orientation)
{
    IntSize sizeForRotation = orientation.usesWidthAsHeight() ? size().transposedSize() : size();

    // FIXME: This preserves neither the pixelFormat nor the colorSapace of the original NativeImage.
    RefPtr buffer = ImageBuffer::create(sizeForRotation, RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    GraphicsContext& context = buffer->context();
    context.drawNativeImage(*this, { { }, sizeForRotation }, { { }, sizeForRotation }, { orientation });

    return ImageBuffer::sinkIntoNativeImage(WTF::move(buffer));
}

#endif

} // namespace WebCore
