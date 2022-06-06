/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "IntSize.h"
#include "PixelBufferFormat.h"
#include <wtf/RefCounted.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class PixelBuffer : public RefCounted<PixelBuffer> {
    WTF_MAKE_NONCOPYABLE(PixelBuffer);
public:
    WEBCORE_EXPORT static bool supportedPixelFormat(PixelFormat);

    WEBCORE_EXPORT virtual ~PixelBuffer();

    const PixelBufferFormat& format() const { return m_format; }
    const IntSize& size() const { return m_size; }

    uint8_t* bytes() const { return m_bytes; }
    size_t sizeInBytes() const { return m_sizeInBytes; }

    virtual bool isByteArrayPixelBuffer() const { return false; }
    virtual RefPtr<PixelBuffer> createScratchPixelBuffer(const IntSize&) const = 0;

    bool setRange(const uint8_t* data, size_t dataByteLength, size_t byteOffset);
    bool zeroRange(size_t byteOffset, size_t rangeByteLength);
    void zeroFill() { zeroRange(0, sizeInBytes()); }

    WEBCORE_EXPORT uint8_t item(size_t index) const;
    void set(size_t index, double value);

protected:
    WEBCORE_EXPORT PixelBuffer(const PixelBufferFormat&, const IntSize&, uint8_t* bytes, size_t sizeInBytes);
    
    WEBCORE_EXPORT static CheckedUint32 computeBufferSize(const PixelBufferFormat&, const IntSize&);

    PixelBufferFormat m_format;
    IntSize m_size;

    uint8_t* m_bytes { nullptr };
    size_t m_sizeInBytes { 0 };
};

} // namespace WebCore
