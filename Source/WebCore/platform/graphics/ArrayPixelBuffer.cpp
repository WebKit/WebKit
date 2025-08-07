/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ArrayPixelBuffer.h"

#include "ByteArrayPixelBuffer.h"
#include "Float16ArrayPixelBuffer.h"

namespace WebCore {

ArrayPixelBuffer::ArrayPixelBuffer(const PixelBufferFormat& format, const IntSize& size, Ref<JSC::ArrayBufferView>&& data)
    : PixelBuffer(format, size, data->mutableSpan())
    , m_data(WTFMove(data))
{
    ASSERT(m_data->getType() == JSC::TypeUint8Clamped || m_data->getType() == JSC::TypeFloat16);
}

std::optional<Ref<ArrayPixelBuffer>> ArrayPixelBuffer::create(const PixelBufferFormat& format, const IntSize& size, std::span<const uint8_t> data)
{
    ASSERT(supportedPixelFormat(format.pixelFormat));

    auto computedBufferSize = PixelBuffer::computeBufferSize(format.pixelFormat, size);
    if (computedBufferSize.hasOverflowed()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    if (data.size_bytes() != computedBufferSize.value()) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    switch (format.pixelFormat) {
    case PixelFormat::RGBA8:
    case PixelFormat::BGRA8: {
        auto buffer = JSC::Uint8ClampedArray::tryCreate(data);
        if (!buffer) {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
        return ByteArrayPixelBuffer::create(format, size, buffer.releaseNonNull());
    }
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case PixelFormat::RGBA16F: {
        if ((uintptr_t(data.data()) % sizeof(Float16)) || (data.size_bytes() % sizeof(Float16))) {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
        auto buffer = JSC::Float16Array::tryCreate(spanReinterpretCast<const Float16>(data));
        if (!buffer) {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
        return Float16ArrayPixelBuffer::create(format, size, buffer.releaseNonNull());
    }
#endif
    default:
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }
}

} // namespace WebCore
