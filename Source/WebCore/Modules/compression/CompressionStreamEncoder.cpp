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

#include "config.h"
#include "CompressionStreamEncoder.h"

#include "BufferSource.h"
#include "Exception.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>

namespace WebCore {

ExceptionOr<RefPtr<Uint8Array>> CompressionStreamEncoder::encode(const BufferSource&& input)
{
    auto compressedDataCheck = compress(input.span());
    if (compressedDataCheck.hasException())
        return compressedDataCheck.releaseException();

    Ref compressedData = compressedDataCheck.releaseReturnValue();
    if (!compressedData->byteLength())
        return nullptr;

    return RefPtr { Uint8Array::create(WTFMove(compressedData)) };
}

ExceptionOr<RefPtr<Uint8Array>> CompressionStreamEncoder::flush()
{
    m_didFinish = true;

    auto compressedDataCheck = compress({ });
    if (compressedDataCheck.hasException())
        return compressedDataCheck.releaseException();

    Ref compressedData = compressedDataCheck.releaseReturnValue();
    if (!compressedData->byteLength())
        return nullptr;

    return RefPtr { Uint8Array::create(WTFMove(compressedData)) };
}

// The compression algorithm is broken up into 2 steps.
// 1. Compression of Data
// 2. Flush Remaining Data
//
// When avail_in is empty we can normally exit performing compression, but during the flush
// step we may have data buffered and will need to continue to keep flushing out the rest.
bool CompressionStreamEncoder::didDeflateFinish(int result) const
{
    return !m_zstream.getPlatformStream().avail_in && (!m_didFinish || (m_didFinish && result == Z_STREAM_END));
}

// See https://www.zlib.net/manual.html#Constants
static bool didDeflateFail(int result)
{
    return result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR;
}

ExceptionOr<Ref<JSC::ArrayBuffer>> CompressionStreamEncoder::compress(std::span<const uint8_t> input)
{
#if PLATFORM(COCOA)
    if (m_format == Formats::CompressionFormat::Brotli)
        return compressAppleCompressionFramework(input);
#endif
    return compressZlib(input);
}

static ZStream::Algorithm compressionAlgorithm(Formats::CompressionFormat format)
{
    switch (format) {
    case Formats::CompressionFormat::Brotli:
        RELEASE_ASSERT_NOT_REACHED();
    case Formats::CompressionFormat::Gzip:
        return ZStream::Algorithm::Gzip;
    case Formats::CompressionFormat::Zlib:
        return ZStream::Algorithm::Zlib;
    case Formats::CompressionFormat::Deflate:
        return ZStream::Algorithm::Deflate;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

ExceptionOr<Ref<JSC::ArrayBuffer>> CompressionStreamEncoder::compressZlib(std::span<const uint8_t> input)
{
    size_t allocateSize = std::max(input.size(), startingAllocationSize);
    auto storage = SharedBufferBuilder();

    int result;    
    bool shouldCompress = true;

    if (!m_zstream.initializeIfNecessary(compressionAlgorithm(m_format), ZStream::Operation::Compression))
        return Exception { ExceptionCode::TypeError, "Initialization Failed."_s };

    m_zstream.getPlatformStream().next_in = const_cast<z_const Bytef*>(input.data());
    m_zstream.getPlatformStream().avail_in = input.size();

    while (shouldCompress) {
        Vector<uint8_t> output;
        if (!output.tryReserveInitialCapacity(allocateSize)) {
            allocateSize /= 4;

            if (allocateSize < startingAllocationSize)
                return Exception { ExceptionCode::OutOfMemoryError };

            continue;
        }

        output.grow(allocateSize);

        m_zstream.getPlatformStream().next_out = output.data();
        m_zstream.getPlatformStream().avail_out = output.size();

        result = deflate(&m_zstream.getPlatformStream(), m_didFinish ? Z_FINISH : Z_NO_FLUSH);

        if (didDeflateFail(result))
            return Exception { ExceptionCode::TypeError, "Failed to compress data."_s };

        if (didDeflateFinish(result)) {
            shouldCompress = false;
            output.shrink(allocateSize - m_zstream.getPlatformStream().avail_out);
        }
        else {
            if (allocateSize < maxAllocationSize)
                allocateSize *= 2;
        }

        storage.append(output);
    }

    RefPtr compressedData = storage.takeAsArrayBuffer();
    if (!compressedData)
        return Exception { ExceptionCode::OutOfMemoryError };

    return compressedData.releaseNonNull();
}

} // namespace WebCore
