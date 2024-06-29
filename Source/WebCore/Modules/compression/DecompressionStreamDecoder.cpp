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
#include "DecompressionStreamDecoder.h"

#include "BufferSource.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>

namespace WebCore {

ExceptionOr<RefPtr<Uint8Array>> DecompressionStreamDecoder::decode(const BufferSource&& input)
{
    auto compressedDataCheck = decompress(input.span());
    if (compressedDataCheck.hasException())
        return compressedDataCheck.releaseException();

    Ref compressedData = compressedDataCheck.releaseReturnValue();
    if (!compressedData->byteLength())
        return nullptr;
    
    return RefPtr { Uint8Array::create(WTFMove(compressedData)) };
}

ExceptionOr<RefPtr<Uint8Array>> DecompressionStreamDecoder::flush()
{
    m_didFinish = true;

    auto compressedDataCheck = decompress({ });
    if (compressedDataCheck.hasException())
        return compressedDataCheck.releaseException();
    
    Ref compressedData = compressedDataCheck.releaseReturnValue();
    if (!compressedData->byteLength())
        return nullptr;

    return RefPtr { Uint8Array::create(WTFMove(compressedData)) };
}

inline ExceptionOr<Ref<JSC::ArrayBuffer>> DecompressionStreamDecoder::decompress(std::span<const uint8_t> input)
{
    return decompressZlib(input);
}

ExceptionOr<bool> DecompressionStreamDecoder::initialize() 
{
    int result = Z_OK;
    m_initialized = true;

    switch (m_format) {
    case Formats::CompressionFormat::Deflate:
        result = inflateInit2(&m_zstream, -15);
        break;
    case Formats::CompressionFormat::Zlib:
        result = inflateInit2(&m_zstream, 15);
        break;
    case Formats::CompressionFormat::Gzip:
        result = inflateInit2(&m_zstream, 15 + 16);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (result != Z_OK)
        return Exception { ExceptionCode::TypeError, "Initialization Failed."_s };

    return true;
}

// The decompression algorithm is broken up into 2 steps.
// 1. Decompression of Data
// 2. Flush Remaining Data
//
// When avail_in is empty we can normally exit performing compression, but during the flush
// step we may have data buffered and will need to continue to keep flushing out the rest.
bool DecompressionStreamDecoder::didInflateFinish(int result) const
{
    return !m_zstream.avail_in && (!m_didFinish || (m_didFinish && result == Z_STREAM_END));
}

// See https://www.zlib.net/manual.html#Constants
static bool didInflateFail(int result)
{
    return result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR;
}

bool DecompressionStreamDecoder::didInflateContainExtraBytes(int result) const
{
    return (result == Z_STREAM_END && m_zstream.avail_in) || (result == Z_BUF_ERROR && m_didFinish);
}

ExceptionOr<Ref<JSC::ArrayBuffer>> DecompressionStreamDecoder::decompressZlib(std::span<const uint8_t> input)
{
    size_t allocateSize = startingAllocationSize;
    auto storage = SharedBufferBuilder();

    int result;    
    bool shouldDecompress = true;

    m_zstream.next_in = const_cast<z_const Bytef*>(input.data());
    m_zstream.avail_in = input.size();

    if (!m_initialized) {
        auto initializeResult = initialize();
        if (initializeResult.hasException())
            return initializeResult.releaseException();
    }

    while (shouldDecompress) {
        Vector<uint8_t> output;
        if (!output.tryReserveInitialCapacity(allocateSize)) {
            allocateSize /= 4;

            if (allocateSize < startingAllocationSize)
                return Exception { ExceptionCode::OutOfMemoryError };

            continue;
        }

        output.grow(allocateSize);

        m_zstream.next_out = output.data();
        m_zstream.avail_out = output.size();

        result = inflate(&m_zstream, m_didFinish ? Z_FINISH : Z_NO_FLUSH);
        
        if (didInflateFail(result))
            return Exception { ExceptionCode::TypeError, "Failed to Decode Data."_s };

        if (didInflateContainExtraBytes(result))
            return Exception { ExceptionCode::TypeError, "Extra bytes past the end."_s };

        if (didInflateFinish(result)) {
            shouldDecompress = false;
            output.shrink(allocateSize - m_zstream.avail_out);
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

#if PLATFORM(COCOA)
ExceptionOr<bool> DecompressionStreamDecoder::initializeAppleCompressionFramework() 
{
    m_initialized = true;

    auto result = compression_stream_init(&m_stream, COMPRESSION_STREAM_DECODE, COMPRESSION_ZLIB);
    if (result != COMPRESSION_STATUS_OK)
        return Exception { ExceptionCode::TypeError, "Initialization Failed."_s };

    return true;
}

ExceptionOr<Ref<JSC::ArrayBuffer>> DecompressionStreamDecoder::decompressAppleCompressionFramework(std::span<const uint8_t> input)
{
    size_t allocateSize = startingAllocationSize;
    auto storage = SharedBufferBuilder();

    compression_status result;    
    bool shouldDecompress = true;

    if (!m_initialized) {
        auto initializeResult = initializeAppleCompressionFramework();
        if (initializeResult.hasException())
            return initializeResult.releaseException();
    }

    m_stream.src_ptr = input.data();
    m_stream.src_size = input.size();
    
    while (shouldDecompress) {
        Vector<uint8_t> output;
        if (!output.tryReserveInitialCapacity(allocateSize)) {
            allocateSize /= 4;

            if (allocateSize < startingAllocationSize)
                return Exception { ExceptionCode::OutOfMemoryError };

            continue;
        }

        output.grow(allocateSize);

        m_stream.dst_ptr = output.data();
        m_stream.dst_size = output.size();

        result = compression_stream_process(&m_stream, m_didFinish ? COMPRESSION_STREAM_FINALIZE : 0);
        
        if (result == COMPRESSION_STATUS_ERROR)
            return Exception { ExceptionCode::TypeError, "Failed to Decode Data."_s };

        if ((result == COMPRESSION_STATUS_END && m_stream.src_size)
            || (m_didFinish && m_stream.src_size))
            return Exception { ExceptionCode::TypeError, "Extra bytes past the end."_s };

        if (!m_stream.src_size) {
            shouldDecompress = false;
            output.shrink(allocateSize - m_stream.dst_size);
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
#endif

} // namespace WebCore
