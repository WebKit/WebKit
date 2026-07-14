/*
 * Copyright (C) 2022-2026 Apple Inc. All rights reserved.
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

#pragma once

#include <JavaScriptCore/GenericTypedArrayView.h>
#include <JavaScriptCore/TypedArrayAdaptors.h>
#include <WebCore/PixelBuffer.h>

namespace WebCore {

template<PixelBuffer::Type pixelBufferType, typename JSCTypedArrayAdaptor, PixelFormat... pixelFormats>
class TypedArrayPixelBuffer final : public PixelBuffer {
public:
    using JSCTypedArray = JSC::GenericTypedArrayView<JSCTypedArrayAdaptor>;
    using ComponentType = typename JSCTypedArrayAdaptor::Type;

    static Ref<TypedArrayPixelBuffer> create(const PixelBufferFormat&, const IntSize&, JSCTypedArray&);
    static std::optional<Ref<TypedArrayPixelBuffer>> create(const PixelBufferFormat&, const IntSize&, std::span<const ComponentType> data);

    static RefPtr<TypedArrayPixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&);
    static RefPtr<TypedArrayPixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&, Ref<JSC::ArrayBuffer>&&);

    JSCTypedArray& data() const LIFETIME_BOUND { return m_data.get(); }
    Ref<JSCTypedArray>&& takeData() { return WTF::move(m_data); }

    Type type() const final { return pixelBufferType; }
    RefPtr<PixelBuffer> createScratchPixelBuffer(const IntSize&) const final;

private:
    TypedArrayPixelBuffer(const PixelBufferFormat&, const IntSize&, Ref<JSCTypedArray>&&);

    Ref<JSCTypedArray> m_data;
};

using ByteArrayPixelBuffer = TypedArrayPixelBuffer<WebCore::PixelBuffer::Type::ByteArray, JSC::Uint8ClampedAdaptor, PixelFormat::RGBA8, PixelFormat::BGRA8>;
extern template class TypedArrayPixelBuffer<WebCore::PixelBuffer::Type::ByteArray, JSC::Uint8ClampedAdaptor, PixelFormat::RGBA8, PixelFormat::BGRA8>;

#if ENABLE(PIXEL_FORMAT_RGBA16F)
using Float16ArrayPixelBuffer = TypedArrayPixelBuffer<PixelBuffer::Type::Float16Array, JSC::Float16Adaptor, PixelFormat::RGBA16F>;
extern template class TypedArrayPixelBuffer<PixelBuffer::Type::Float16Array, JSC::Float16Adaptor, PixelFormat::RGBA16F>;
#endif

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ByteArrayPixelBuffer)
    static bool isType(const WebCore::PixelBuffer& pixelBuffer) { return pixelBuffer.type() == WebCore::PixelBuffer::Type::ByteArray; }
SPECIALIZE_TYPE_TRAITS_END()

#if ENABLE(PIXEL_FORMAT_RGBA16F)
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Float16ArrayPixelBuffer)
    static bool isType(const WebCore::PixelBuffer& pixelBuffer) { return pixelBuffer.type() == WebCore::PixelBuffer::Type::Float16Array; }
SPECIALIZE_TYPE_TRAITS_END()
#endif
