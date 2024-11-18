/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "ByteArrayPixelBuffer.h"
#include "ExceptionOr.h"
#include "ImageDataSettings.h"
#include "IntSize.h"
#include "PredefinedColorSpace.h"
#include <JavaScriptCore/Forward.h>
#include <JavaScriptCore/TypedArrays.h>
#include <variant>
#include <wtf/Forward.h>

// Needed for `downcast` below.
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Uint8ClampedArray)
    static bool isType(const JSC::ArrayBufferView& arrayBufferView) { return arrayBufferView.getType() == JSC::TypeUint8Clamped; }
SPECIALIZE_TYPE_TRAITS_END()
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Float16Array)
    static bool isType(const JSC::ArrayBufferView& arrayBufferView) { return arrayBufferView.getType() == JSC::TypeFloat16; }
SPECIALIZE_TYPE_TRAITS_END()

namespace WebCore {

template <ImageDataStorageFormat storageFormat> struct TypedArrayForStorageFormat;
template <> struct TypedArrayForStorageFormat<ImageDataStorageFormat::Uint8> {
    using TypedArray = Uint8ClampedArray;
};
template <> struct TypedArrayForStorageFormat<ImageDataStorageFormat::Float16> {
    using TypedArray = Float16Array;
};

class ImageDataArray {
public:
    static std::optional<ImageDataArray> tryCreateUninitialized(size_t length, ImageDataStorageFormat storageFormat)
    {
        return tryCreate(length, storageFormat, false);
    }

    static std::optional<ImageDataArray> tryCreateZeroFilled(size_t length, ImageDataStorageFormat storageFormat)
    {
        return tryCreate(length, storageFormat, true);
    }

    ImageDataArray(Ref<Uint8ClampedArray>&& data)
        : m_bufferView(WTFMove(data))
    { }
    ImageDataArray(Ref<Float16Array>&& data)
        : m_bufferView(WTFMove(data))
    { }

    ImageDataStorageFormat storageFormat() const
    {
        switch (m_bufferView->getType()) {
        case JSC::TypeUint8Clamped: return ImageDataStorageFormat::Uint8;
        case JSC::TypeFloat16: return ImageDataStorageFormat::Float16;
        default: RELEASE_ASSERT_NOT_REACHED("Unexpected ArrayBufferView type");
        }
    }

    template <typename F>
    auto visit(F&& f) const
    {
        switch (m_bufferView->getType()) {
        case JSC::TypeUint8Clamped: return std::forward<F>(f)(downcast<Uint8ClampedArray>(m_bufferView.get()));
        case JSC::TypeFloat16: return std::forward<F>(f)(downcast<Float16Array>(m_bufferView.get()));
        default: RELEASE_ASSERT_NOT_REACHED("Unexpected ArrayBufferView type");
        }
    }

    Ref<Uint8ClampedArray> asUint8ClampedArray() const;
    Ref<Float16Array> asFloat16Array() const;

    ArrayBufferView& getArrayBufferView() const { return m_bufferView.get(); }
    ArrayBufferView* operator->() const { return &getArrayBufferView(); }

    auto length() const
    {
        return visit([](const auto& array) { return array.length(); });
    }

    // Needed by `toJS<IDLUnion<IDLUint8ClampedArray, ...>, const ImageDataArray&>()`
    using Variant = std::variant<RefPtr<Uint8ClampedArray>, RefPtr<Float16Array>>;
    operator Variant() const
    {
        return visit([](auto& a) {
            return Variant(RefPtr(&a));
        });
    }

private:
    static std::optional<ImageDataArray> tryCreate(size_t length, ImageDataStorageFormat storageFormat, bool zeroFill)
    {
        std::optional<ImageDataArray> array;
        switch (storageFormat) {
        case ImageDataStorageFormat::Uint8:
            if (RefPtr bufferView = Uint8ClampedArray::tryCreateUninitialized(length)) {
                if (zeroFill)
                    bufferView->zeroFill();
                array.emplace(bufferView.releaseNonNull());
            }
            break;
        case ImageDataStorageFormat::Float16:
            if (RefPtr bufferView = Float16Array::tryCreateUninitialized(length)) {
                if (zeroFill)
                    bufferView->zeroFill();
                array.emplace(bufferView.releaseNonNull());
            }
        }
        return array;
    }

    Ref<ArrayBufferView> m_bufferView;
};

class ImageData : public RefCounted<ImageData> {
public:
    WEBCORE_EXPORT static Ref<ImageData> create(Ref<ByteArrayPixelBuffer>&&);
    WEBCORE_EXPORT static RefPtr<ImageData> create(RefPtr<ByteArrayPixelBuffer>&&);
    WEBCORE_EXPORT static RefPtr<ImageData> createZeroFilled(const IntSize&, PredefinedColorSpace, ImageDataStorageFormat = ImageDataStorageFormat::Uint8);
    WEBCORE_EXPORT static RefPtr<ImageData> create(const IntSize&, ImageDataArray&&, PredefinedColorSpace);
    WEBCORE_EXPORT static ExceptionOr<Ref<ImageData>> createUninitialized(unsigned rows, unsigned pixelsPerRow, PredefinedColorSpace defaultColorSpace, std::optional<ImageDataSettings> = std::nullopt);
    WEBCORE_EXPORT static ExceptionOr<Ref<ImageData>> create(unsigned sw, unsigned sh, std::optional<ImageDataSettings>);
    WEBCORE_EXPORT static ExceptionOr<Ref<ImageData>> create(ImageDataArray&&, unsigned sw, std::optional<unsigned> sh, std::optional<ImageDataSettings>);

    WEBCORE_EXPORT ~ImageData();

    static PredefinedColorSpace computeColorSpace(std::optional<ImageDataSettings>, PredefinedColorSpace defaultColorSpace = PredefinedColorSpace::SRGB);

    const IntSize& size() const { return m_size; }

    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    const ImageDataArray& data() const { return m_data; }
    PredefinedColorSpace colorSpace() const { return m_colorSpace; }
    ImageDataStorageFormat storageFormat() const { return m_data.storageFormat(); }

    Ref<ByteArrayPixelBuffer> pixelBuffer() const;

    RefPtr<ImageData> clone() const;

private:
    explicit ImageData(const IntSize&, ImageDataArray&&, PredefinedColorSpace);

    IntSize m_size;
    ImageDataArray m_data;
    PredefinedColorSpace m_colorSpace;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const ImageData&);

} // namespace WebCore
