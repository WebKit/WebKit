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

#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

static CheckedUint32 computeDataSize(const IntSize& size)
{
    constexpr uint32_t componentsPerPixel = 4;
    auto dataSize = CheckedUint32 { size.width() } * size.height() * componentsPerPixel;
    if (!dataSize.hasOverflowed() && dataSize.value() > std::numeric_limits<int32_t>::max())
        dataSize.overflowed();
    return dataSize;
}

PredefinedColorSpace ImageData::computeColorSpace(std::optional<ImageDataSettings> settings, PredefinedColorSpace defaultColorSpace)
{
    if (settings && settings->colorSpace)
        return *settings->colorSpace;
    return defaultColorSpace;
}

Ref<ImageData> ImageData::create(Ref<ByteArrayPixelBuffer>&& pixelBuffer)
{
    auto colorSpace = toPredefinedColorSpace(pixelBuffer->format().colorSpace);
    return adoptRef(*new ImageData(pixelBuffer->size(), pixelBuffer->takeData(), *colorSpace));
}

RefPtr<ImageData> ImageData::create(RefPtr<ByteArrayPixelBuffer>&& pixelBuffer)
{
    if (!pixelBuffer)
        return nullptr;
    return create(pixelBuffer.releaseNonNull());
}

RefPtr<ImageData> ImageData::createZeroFilled(const IntSize& size, PredefinedColorSpace colorSpace, ImageDataStorageFormat storageFormat)
{
    auto dataSize = computeDataSize(size);
    if (dataSize.hasOverflowed())
        return nullptr;
    auto array = ImageDataArray::tryCreateZeroFilled(dataSize, storageFormat);
    if (!array)
        return nullptr;
    return adoptRef(*new ImageData(size, WTFMove(*array), colorSpace));
}

RefPtr<ImageData> ImageData::create(const IntSize& size, ImageDataArray&& array, PredefinedColorSpace colorSpace)
{
    auto dataSize = computeDataSize(size);
    if (dataSize.hasOverflowed() || dataSize != array.length())
        return nullptr;

    return adoptRef(*new ImageData(size, WTFMove(array), colorSpace));
}

ExceptionOr<Ref<ImageData>> ImageData::createUninitialized(unsigned rows, unsigned pixelsPerRow, PredefinedColorSpace defaultColorSpace, std::optional<ImageDataSettings> settings)
{
    IntSize size(rows, pixelsPerRow);
    auto dataSize = computeDataSize(size);
    if (dataSize.hasOverflowed())
        return Exception { ExceptionCode::RangeError, "Cannot allocate a buffer of this size"_s };

    auto array = ImageDataArray::tryCreateUninitialized(dataSize, settings ? settings->storageFormat : ImageDataStorageFormat::Uint8);
    if (!array) {
        // FIXME: Does this need to be a "real" out of memory error with setOutOfMemoryError called on it?
        return Exception { ExceptionCode::RangeError, "Out of memory"_s };
    }

    auto colorSpace = computeColorSpace(settings, defaultColorSpace);
    return adoptRef(*new ImageData(size, WTFMove(*array), colorSpace));
}

ExceptionOr<Ref<ImageData>> ImageData::create(unsigned sw, unsigned sh, std::optional<ImageDataSettings> settings)
{
    if (!sw || !sh)
        return Exception { ExceptionCode::IndexSizeError };

    IntSize size(sw, sh);
    auto dataSize = computeDataSize(size);
    if (dataSize.hasOverflowed())
        return Exception { ExceptionCode::RangeError, "Cannot allocate a buffer of this size"_s };

    auto array = ImageDataArray::tryCreateZeroFilled(dataSize, settings ? settings->storageFormat : ImageDataStorageFormat::Uint8);
    if (!array) {
        // FIXME: Does this need to be a "real" out of memory error with setOutOfMemoryError called on it?
        return Exception { ExceptionCode::RangeError, "Out of memory"_s };
    }

    auto colorSpace = computeColorSpace(settings);
    return adoptRef(*new ImageData(size, WTFMove(*array), colorSpace));
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
    auto dataSize = computeDataSize(size);
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

ImageData::~ImageData() = default;

template <typename From, typename To>
struct TypedArrayItemConverter;

template <>
struct TypedArrayItemConverter<Uint8ClampedArray, Float16Array> {
    static constexpr JSC::Float16Adaptor::Type convert(JSC::Uint8ClampedAdaptor::Type value)
    {
        return double(value) / 255.0;
    }
};

template <>
struct TypedArrayItemConverter<Float16Array, Uint8ClampedArray> {
    static constexpr JSC::Uint8ClampedAdaptor::Type convert(JSC::Float16Adaptor::Type value)
    {
        auto d = double(value);
        if (d <= 0)
            return 0;
        if (d >= 245.5)
            return 255;
        return (d * 255.0);
    }
};

template <typename ToTypeArray>
Ref<ToTypeArray> convertToTypedArray(const ImageDataArray& fromImageDataArray)
{
    return fromImageDataArray.visit([]<typename FromTypedArray>(FromTypedArray& fromTypedArray) {
        if constexpr (std::is_same_v<FromTypedArray, ToTypeArray>)
            return Ref(fromTypedArray);
        else {
            using Converter = TypedArrayItemConverter<FromTypedArray, ToTypeArray>;
            auto length = fromTypedArray.length();
            Ref<ToTypeArray> toArray = ToTypeArray::create(length);
            for (size_t i = 0; i < length; ++i)
                toArray->set(i, Converter::convert(fromTypedArray.item(i)));
            return toArray;
        }
    });
}

Ref<Uint8ClampedArray> ImageDataArray::asUint8ClampedArray() const
{
    return convertToTypedArray<Uint8ClampedArray>(*this);
}

Ref<Float16Array> ImageDataArray::asFloat16Array() const
{
    return convertToTypedArray<Float16Array>(*this);
}

Ref<ByteArrayPixelBuffer> ImageData::pixelBuffer() const
{
    Ref uint8Data = m_data.asUint8ClampedArray();
    PixelBufferFormat format { AlphaPremultiplication::Unpremultiplied, PixelFormat::RGBA8, toDestinationColorSpace(m_colorSpace) };
    return ByteArrayPixelBuffer::create(format, m_size, uint8Data.get());
}

RefPtr<ImageData> ImageData::clone() const
{
    return m_data.visit([self = Ref(*this)](auto& array) {
        return ImageData::create(self->m_size, array.create(array.data(), array.length()), self->m_colorSpace);
    });
}

TextStream& operator<<(TextStream& ts, const ImageData& imageData)
{
    // Print out the address of the pixel data array
    return ts << &imageData.data();
}

}
