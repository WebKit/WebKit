/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DecompressionStreamDecoder.h"

namespace WebCore {

ExceptionOr<Ref<JSC::ArrayBuffer>> DecompressionStreamDecoder::decompressAppleCompressionFramework(std::span<const uint8_t> input)
{
    size_t allocateSize = startingAllocationSize;
    auto storage = SharedBufferBuilder();

    compression_status result;
    bool shouldDecompress = true;

    if (!m_compressionStream.initializeIfNecessary(CompressionStream::Algorithm::Brotli, CompressionStream::Operation::Decompression))
        return Exception { ExceptionCode::TypeError, "Initialization Failed."_s };

    m_compressionStream.getPlatformStream().src_ptr = input.data();
    m_compressionStream.getPlatformStream().src_size = input.size();

    while (shouldDecompress) {
        Vector<uint8_t> output;
        if (!output.tryReserveInitialCapacity(allocateSize)) {
            allocateSize /= 4;

            if (allocateSize < startingAllocationSize)
                return Exception { ExceptionCode::OutOfMemoryError };

            continue;
        }

        output.grow(allocateSize);

        m_compressionStream.getPlatformStream().dst_ptr = output.data();
        m_compressionStream.getPlatformStream().dst_size = output.size();

        result = compression_stream_process(&m_compressionStream.getPlatformStream(), m_didFinish ? COMPRESSION_STREAM_FINALIZE : 0);

        if (result == COMPRESSION_STATUS_ERROR)
            return Exception { ExceptionCode::TypeError, "Failed to Decode Data."_s };

        if ((result == COMPRESSION_STATUS_END && m_compressionStream.getPlatformStream().src_size)
            || (m_didFinish && m_compressionStream.getPlatformStream().src_size))
            return Exception { ExceptionCode::TypeError, "Extra bytes past the end."_s };

        if (!m_compressionStream.getPlatformStream().src_size) {
            shouldDecompress = false;
            output.shrink(allocateSize - m_compressionStream.getPlatformStream().dst_size);
        } else {
            if (allocateSize < maxAllocationSize)
                allocateSize *= 2;
        }

        storage.append(output);
    }

    RefPtr decompressedData = storage.takeAsArrayBuffer();
    if (!decompressedData)
        return Exception { ExceptionCode::OutOfMemoryError };

    return decompressedData.releaseNonNull();
}

}
