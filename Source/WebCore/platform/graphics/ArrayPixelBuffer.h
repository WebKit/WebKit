/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ArrayBufferView.h>
#include <JavaScriptCore/TypedArrayAdaptersForwardDeclarations.h>
#include <WebCore/PixelBuffer.h>

namespace WebCore {

class ArrayPixelBuffer : public PixelBuffer {
public:
    JSC::ArrayBufferView& data() const LIFETIME_BOUND { return m_data.get(); }
    Ref<JSC::ArrayBufferView> takeData() && { return WTF::move(m_data); }
    std::span<const uint8_t> span() const LIFETIME_BOUND { return bytes(); }

    static RefPtr<ArrayPixelBuffer> tryCreate(const PixelBufferFormat&, const IntSize&);

protected:
    ArrayPixelBuffer(const PixelBufferFormat&, const IntSize&, Ref<JSC::ArrayBufferView>&&);

private:
    Ref<JSC::ArrayBufferView> m_data;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ArrayPixelBuffer)
    static bool isType(const WebCore::PixelBuffer& pixelBuffer)
    {
#if ENABLE(PIXEL_FORMAT_RGBA16F)
        return pixelBuffer.type() == WebCore::PixelBuffer::Type::ByteArray || pixelBuffer.type() == WebCore::PixelBuffer::Type::Float16Array;
#else
        return pixelBuffer.type() == WebCore::PixelBuffer::Type::ByteArray;
#endif
    }
SPECIALIZE_TYPE_TRAITS_END()

// Needed to safely downcast from JSC::ArrayBufferView to the concrete types used in WebCore.
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Uint8ClampedArray)
    static bool isType(const JSC::ArrayBufferView& arrayBufferView) { return arrayBufferView.getType() == JSC::TypeUint8Clamped; }
SPECIALIZE_TYPE_TRAITS_END()
SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::Float16Array)
    static bool isType(const JSC::ArrayBufferView& arrayBufferView) { return arrayBufferView.getType() == JSC::TypeFloat16; }
SPECIALIZE_TYPE_TRAITS_END()
