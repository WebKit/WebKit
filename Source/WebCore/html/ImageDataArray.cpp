/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "ImageDataArray.h"

#include <JavaScriptCore/Float16Array.h>
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/Uint8ClampedArray.h>

// Needed for `downcast` below.
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Uint8ClampedArray)
    static bool isType(const JSC::ArrayBufferView& arrayBufferView) { return arrayBufferView.getType() == JSC::TypeUint8Clamped; }
SPECIALIZE_TYPE_TRAITS_END()
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Float16Array)
    static bool isType(const JSC::ArrayBufferView& arrayBufferView) { return arrayBufferView.getType() == JSC::TypeFloat16; }
SPECIALIZE_TYPE_TRAITS_END()

namespace WebCore {

template <typename F>
static auto visitArrayBufferView(JSC::ArrayBufferView& bufferView, F&& f)
{
    // Always try Uint8ClampedArray first, as it should be the most frequent.
    if (auto* array = dynamicDowncast<JSC::Uint8ClampedArray>(bufferView))
        return std::forward<F>(f)(*array);
    if (auto* array = dynamicDowncast<JSC::Float16Array>(bufferView))
        return std::forward<F>(f)(*array);
    RELEASE_ASSERT_NOT_REACHED("Unexpected ArrayBufferView type");
}

ImageDataArray::ImageDataArray(Ref<JSC::ArrayBufferView>&& arrayBufferView)
    : m_arrayBufferView(WTFMove(arrayBufferView))
{
    ASSERT(isSupported(m_arrayBufferView.get()));
}

ImageDataArray::ImageDataArray(Ref<JSC::Uint8ClampedArray>&& data)
    : ImageDataArray(Ref<JSC::ArrayBufferView>(WTFMove(data)))
{ }

ImageDataArray::ImageDataArray(Ref<JSC::Float16Array>&& data)
    : ImageDataArray(Ref<JSC::ArrayBufferView>(WTFMove(data)))
{ }

template <typename TypedArray>
static void fillTypedArray(TypedArray& typedArray, std::span<const uint8_t> optionalBytes)
{
    if (optionalBytes.empty())
        return typedArray.zeroFill();
    auto bufferViewSpan = typedArray.mutableSpan();
    RELEASE_ASSERT(bufferViewSpan.size_bytes() == optionalBytes.size_bytes(), "Caller should provide correctly-sized buffer to copy");
    memcpy(bufferViewSpan.data(), optionalBytes.data(), optionalBytes.size_bytes());
}

std::optional<ImageDataArray> ImageDataArray::tryCreate(size_t length, ImageDataStorageFormat storageFormat, std::span<const uint8_t> optionalBytes)
{
    std::optional<ImageDataArray> array;
    switch (storageFormat) {
    case ImageDataStorageFormat::Uint8:
        if (RefPtr typedArray = JSC::Uint8ClampedArray::tryCreateUninitialized(length)) {
            fillTypedArray(*typedArray, optionalBytes);
            array.emplace(typedArray.releaseNonNull());
        }
        break;
    case ImageDataStorageFormat::Float16:
        if (RefPtr typedArray = JSC::Float16Array::tryCreateUninitialized(length)) {
            fillTypedArray(*typedArray, optionalBytes);
            array.emplace(typedArray.releaseNonNull());
        }
    }
    return array;
}

bool ImageDataArray::isSupported(const JSC::ArrayBufferView& arrayBufferView)
{
    return isSupported(arrayBufferView.getType());
}

ImageDataStorageFormat ImageDataArray::storageFormat() const
{
    auto imageDataStorageFormat = toImageDataStorageFormat(m_arrayBufferView->getType());
    RELEASE_ASSERT(!!imageDataStorageFormat);
    return *imageDataStorageFormat;
}

template <typename From, typename To>
struct TypedArrayItemConverter;

template <>
struct TypedArrayItemConverter<JSC::Uint8ClampedArray, JSC::Float16Array> {
    static constexpr JSC::Float16Adaptor::Type convert(JSC::Uint8ClampedAdaptor::Type value)
    {
        return double(value) / 255.0;
    }
};

template <>
struct TypedArrayItemConverter<JSC::Float16Array, JSC::Uint8ClampedArray> {
    static constexpr JSC::Uint8ClampedAdaptor::Type convert(JSC::Float16Adaptor::Type value)
    {
        auto d = double(value);
        if (d <= 0)
            return 0;
        if (d >= 1)
            return 255;
        return d * 255.0;
    }
};

template <typename ToTypeArray>
static Ref<ToTypeArray> convertToTypedArray(const ImageDataArray& fromImageDataArray)
{
    Ref arrayBufferView = fromImageDataArray.arrayBufferView();
    return visitArrayBufferView(arrayBufferView, []<typename FromTypedArray>(FromTypedArray& fromTypedArray) {
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

Ref<JSC::Uint8ClampedArray> ImageDataArray::asUint8ClampedArray() const
{
    return convertToTypedArray<JSC::Uint8ClampedArray>(*this);
}

Ref<JSC::Float16Array> ImageDataArray::asFloat16Array() const
{
    return convertToTypedArray<JSC::Float16Array>(*this);
}

size_t ImageDataArray::length() const
{
    return visitArrayBufferView(
        m_arrayBufferView.get(),
        [](const auto& array) {
            return array.length();
        });
}

Ref<JSON::Value> ImageDataArray::copyToJSONArray() const
{
    return visitArrayBufferView(m_arrayBufferView.get(), []<typename T>(const T& array) -> Ref<JSON::Value> {
        static_assert(std::is_same_v<T, JSC::Uint8ClampedArray> || std::is_same_v<T, JSC::Float16Array>);
        using CType = std::conditional_t<std::is_same_v<T, JSC::Uint8ClampedArray>, int, double>;
        Ref jsArray = JSON::ArrayOf<CType>::create();
        for (const auto& item : array.typedSpan())
            jsArray->addItem(CType(item));
        return jsArray;
    });
}

ImageDataArray::operator Variant() const
{
    return visitArrayBufferView(m_arrayBufferView.get(), [](auto& a) {
        return Variant(RefPtr(&a));
    });
}

} // namespace WebCore
