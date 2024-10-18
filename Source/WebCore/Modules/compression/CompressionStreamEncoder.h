/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "BufferSource.h"
#include "CompressionStream.h"
#include "ExceptionOr.h"
#include "Formats.h"
#include "ZStream.h"
#include <JavaScriptCore/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CompressionStreamEncoder : public RefCounted<CompressionStreamEncoder> {
public:
    static ExceptionOr<Ref<CompressionStreamEncoder>> create(unsigned char formatChar)
    {
        auto format = static_cast<Formats::CompressionFormat>(formatChar);
#if !PLATFORM(COCOA)
        if (format == Formats::CompressionFormat::Brotli)
            return Exception { ExceptionCode::NotSupportedError, "Unsupported algorithm"_s };
#endif
        return adoptRef(*new CompressionStreamEncoder(format));
    }

    ExceptionOr<RefPtr<Uint8Array>> encode(const BufferSource&&);
    ExceptionOr<RefPtr<Uint8Array>> flush();

private:
    bool didDeflateFinish(int) const;

    ExceptionOr<Ref<JSC::ArrayBuffer>> compress(std::span<const uint8_t>);
    ExceptionOr<Ref<JSC::ArrayBuffer>> compressZlib(std::span<const uint8_t>);
#if PLATFORM(COCOA)
    ExceptionOr<Ref<JSC::ArrayBuffer>> compressAppleCompressionFramework(std::span<const uint8_t>);
#endif

    explicit CompressionStreamEncoder(Formats::CompressionFormat format)
        : m_format(format)
    {
    }

    // If the user provides too small of an input size we will automatically allocate a page worth of memory instead.
    // Very small input sizes can result in a larger output than their input. This would require an additional 
    // encode call then, which is not desired.
    const size_t startingAllocationSize = 16384; // 16KB
    const size_t maxAllocationSize = 1073741824; // 1GB

    bool m_didFinish { false };
    const Formats::CompressionFormat m_format;

    // TODO: convert to using variant
    CompressionStream m_compressionStream;
    ZStream m_zstream;
};
} // namespace WebCore
