/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageData.h"

#include "ExceptionOr.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static CheckedUint32 computeDataSize(const IntSize& size, ImageDataPixelFormat pixelFormat)
{
    return PixelBuffer::computePixelComponentCount(toPixelFormat(pixelFormat), size);
}

PredefinedColorSpace ImageData::computeColorSpace(std::optional<ImageDataSettings> settings, PredefinedColorSpace defaultColorSpace)
{
    if (settings && settings->colorSpace)
        return *settings->colorSpace;
    return defaultColorSpace;
}

static ImageDataPixelFormat computePixelFormat(std::optional<ImageDataSettings> settings, ImageDataPixelFormat defaultPixelFormat = ImageDataPixelFormat::RgbaUnorm8)
{
    return settings ? settings->pixelFormat : defaultPixelFormat;
}

Ref<ImageData> ImageData::create(Ref<ByteArrayPixelBuffer>&& pixelBuffer, std::optional<ImageDataPixelFormat> overridingPixelFormat)
{
    auto colorSpace = toPredefinedColorSpace(pixelBuffer->format().colorSpace);
    return adoptRef(*new ImageData(pixelBuffer->size(), pixelBuffer->takeData(), *colorSpace, overridingPixelFormat));
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)
Ref<ImageData> ImageData::create(Ref<Float16ArrayPixelBuffer>&& pixelBuffer, std::optional<ImageDataPixelFormat> overridingPixelFormat)
{
    auto colorSpace = toPredefinedColorSpace(pixelBuffer->format().colorSpace);
    auto size = pixelBuffer->size();
    return adoptRef(*new ImageData(size, WTFMove(pixelBuffer.get()).takeData(), *colorSpace, overridingPixelFormat));
}
#endif // ENABLE(PIXEL_FORMAT_RGBA16F)

RefPtr<ImageData> ImageData::create(RefPtr<ByteArrayPixelBuffer>&& pixelBuffer, std::optional<ImageDataPixelFormat> overridingPixelFormat)
{
    if (!pixelBuffer)
        return nullptr;
    return create(pixelBuffer.releaseNonNull(), overridingPixelFormat);
}

RefPtr<ImageData> ImageData::create(Ref<PixelBuffer>&& pixelBuffer, std::optional<ImageDataPixelFormat> overridingPixelFormat)
{
    if (is<ByteArrayPixelBuffer>(pixelBuffer))
        return create(uncheckedDowncast<ByteArrayPixelBuffer>(WTFMove(pixelBuffer)), overridingPixelFormat);
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (is<Float16ArrayPixelBuffer>(pixelBuffer))
        return create(uncheckedDowncast<Float16ArrayPixelBuffer>(WTFMove(pixelBuffer)), overridingPixelFormat);
#endif // ENABLE(PIXEL_FORMAT_RGBA16F)
    return nullptr;
}

RefPtr<ImageData> ImageData::create(const IntSize& size, PredefinedColorSpace colorSpace, ImageDataPixelFormat imageDataPixelFormat)
{
    auto dataSize = computeDataSize(size, ImageDataPixelFormat::RgbaUnorm8);
    if (dataSize.hasOverflowed())
        return nullptr;
    auto array = ImageDataArray::tryCreate(dataSize, ImageDataPixelFormat::RgbaUnorm8);
    if (!array)
        return nullptr;
    return adoptRef(*new ImageData(size, WTFMove(*array), colorSpace, imageDataPixelFormat));
}

RefPtr<ImageData> ImageData::create(const IntSize& size, ImageDataArray&& array, PredefinedColorSpace colorSpace)
{
    auto dataSize = computeDataSize(size, array.pixelFormat());
    if (dataSize.hasOverflowed() || dataSize != array.length())
        return nullptr;
    return adoptRef(*new ImageData(size, WTFMove(array), colorSpace));
}


ExceptionOr<Ref<ImageData>> ImageData::create(unsigned sw, unsigned sh, PredefinedColorSpace defaultColorSpace, std::optional<ImageDataSettings> settings, std::span<const uint8_t> optionalBytes)
{
    if (!sw || !sh)
        return Exception { ExceptionCode::IndexSizeError };

    IntSize size(sw, sh);
    auto pixelFormat = computePixelFormat(settings);
    auto dataSize = computeDataSize(size, pixelFormat);
    if (dataSize.hasOverflowed())
        return Exception { ExceptionCode::RangeError, "Cannot allocate a buffer of this size"_s };

    auto array = ImageDataArray::tryCreate(dataSize, pixelFormat, optionalBytes);
    if (!array) {
        // FIXME: Does this need to be a "real" out of memory error with setOutOfMemoryError called on it?
        return Exception { ExceptionCode::RangeError, "Out of memory"_s };
    }

    auto colorSpace = ImageData::computeColorSpace(settings, defaultColorSpace);
    return adoptRef(*new ImageData(size, WTFMove(*array), colorSpace));
}

ExceptionOr<Ref<ImageData>> ImageData::create(unsigned sw, unsigned sh, std::optional<ImageDataSettings> settings)
{
    return create(sw, sh, PredefinedColorSpace::SRGB, settings);
}

ExceptionOr<Ref<ImageData>> ImageData::create(ImageDataArray&& array, unsigned sw, std::optional<unsigned> sh, std::optional<ImageDataSettings> settings)
{
    auto length = array.length();
    if (!length || length % 4)
        return Exception { ExceptionCode::InvalidStateError, "Length is not a non-zero multiple of 4"_s };

    auto pixels = length / 4;
    if (!sw || pixels % sw)
        return Exception { ExceptionCode::IndexSizeError, "Length is not a multiple of sw"_s };

    Checked<int, RecordOverflow> height = pixels / sw;
    if (height.hasOverflowed())
        return Exception { ExceptionCode::IndexSizeError, "Computed height is too big"_s };

    if (sh && sh.value() != height)
        return Exception { ExceptionCode::IndexSizeError, "sh value is not equal to height"_s };

    IntSize size(sw, height.value());
    auto dataSize = computeDataSize(size, computePixelFormat(settings));
    if (dataSize.hasOverflowed() || dataSize != length)
        return Exception { ExceptionCode::RangeError };

    auto colorSpace = computeColorSpace(settings);
    return adoptRef(*new ImageData(size, WTFMove(array), colorSpace));
}

ImageData::ImageData(const IntSize& size, ImageDataArray&& data, PredefinedColorSpace colorSpace)
    : m_size(size)
    , m_data(WTFMove(data))
    , m_colorSpace(colorSpace)
{
}

ImageData::ImageData(const IntSize& size, ImageDataArray&& data, PredefinedColorSpace colorSpace, std::optional<ImageDataPixelFormat> overridingPixelFormat)
    : m_size(size)
    , m_data(WTFMove(data), overridingPixelFormat)
    , m_colorSpace(colorSpace)
{
}

ImageData::~ImageData() = default;

Ref<ByteArrayPixelBuffer> ImageData::byteArrayPixelBuffer() const
{
    Ref uint8Data = m_data.asUint8ClampedArray();
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, toDestinationColorSpace(m_colorSpace) };
    return ByteArrayPixelBuffer::create(format, m_size, uint8Data.get());
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)
Ref<Float16ArrayPixelBuffer> ImageData::float16ArrayPixelBuffer() const
{
    Ref float16Data = m_data.asFloat16Array();
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA16F, toDestinationColorSpace(m_colorSpace) };
    return Float16ArrayPixelBuffer::create(format, m_size, float16Data.get());
}
#endif // ENABLE(PIXEL_FORMAT_RGBA16F)

Ref<PixelBuffer> ImageData::pixelBuffer() const
{
    switch (m_data.pixelFormat()) {
    case ImageDataPixelFormat::RgbaUnorm8:
        return byteArrayPixelBuffer();
    case ImageDataPixelFormat::RgbaFloat16:
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        return float16ArrayPixelBuffer();
#else
        RELEASE_ASSERT_NOT_REACHED("Unexpected ImageDataPixelFormat::RgbaFloat16");
#endif
    }
    RELEASE_ASSERT_NOT_REACHED("Unexpected ImageDataPixelFormat value");
}

TextStream& operator<<(TextStream& ts, const ImageData& imageData)
{
    // Print out the address of the pixel data array
    return ts << &imageData.data();
}

}
