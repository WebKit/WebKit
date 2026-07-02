/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ImageBufferAllocator.h"

#include "ByteArrayPixelBuffer.h"
#include "Float16ArrayPixelBuffer.h"
#include "ImageBuffer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageBufferAllocator);

ImageBufferAllocator::ImageBufferAllocator() = default;
ImageBufferAllocator::~ImageBufferAllocator() = default;

RefPtr<ImageBuffer> ImageBufferAllocator::createImageBuffer(const FloatSize& size, const DestinationColorSpace& colorSpace, RenderingMode renderingMode) const
{
    return ImageBuffer::create(size, renderingMode, RenderingPurpose::Unspecified, 1, colorSpace, PixelFormat::BGRA8);
}

RefPtr<PixelBuffer> ImageBufferAllocator::createPixelBuffer(const PixelBufferFormat& format, const IntSize& size) const
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (format.pixelFormat == PixelFormat::RGBA16F)
        return Float16ArrayPixelBuffer::tryCreate(format, size);
#endif
    return ByteArrayPixelBuffer::tryCreate(format, size);
}

} // namespace WebCore
