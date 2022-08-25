/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
    auto* data = input.data();
    if (!data)
        return Exception { TypeError, "No data provided"_s };

    auto compressedDataCheck = compress(data, input.length());
    if (compressedDataCheck.hasException())
        return compressedDataCheck.releaseException();

    auto compressedData = compressedDataCheck.returnValue();
    if (!compressedData.size())
        return nullptr;
    
    return Uint8Array::tryCreate(compressedData.data(), compressedData.size());
}

ExceptionOr<RefPtr<Uint8Array>> CompressionStreamEncoder::flush()
{
    finish = true;

    auto compressedDataCheck = compress(0, 0);
    if (compressedDataCheck.hasException())
        return compressedDataCheck.releaseException();
    
    auto compressedData = compressedDataCheck.returnValue();
    if (!compressedData.size())
        return nullptr;
    
    return Uint8Array::tryCreate(compressedData.data(), compressedData.size());
}

ExceptionOr<bool> CompressionStreamEncoder::initialize() 
{
    int result = Z_OK;

    initailized = true;
    zstream.opaque = 0;
    zstream.zalloc = 0;
    zstream.zfree = 0;

    switch (m_format) {
    // Values chosen here are based off
    // https://developer.apple.com/documentation/compression/compression_algorithm/compression_zlib?language=objc
    case Formats::CompressionFormat::Deflate:
        result = deflateInit2(&zstream, 5, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
        break;
    case Formats::CompressionFormat::Zlib:
        result = deflateInit2(&zstream, 5, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
        break;
    case Formats::CompressionFormat::Gzip:
        result = deflateInit2(&zstream, 5, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    if (result != Z_OK)
        return Exception { TypeError, "Initialization Failed."_s };

    return true;
}

ExceptionOr<Vector<uint8_t>> CompressionStreamEncoder::compress(const uint8_t* input, const size_t inputLength)
{
    size_t index = 0, totalSize = 0;
    size_t allocateSize = (inputLength < startingAllocationSize) ? startingAllocationSize : inputLength;
    Vector<Vector<uint8_t>>storage;
    storage.reserveCapacity(128);
    int result;    
    bool shouldCompress = true;

    zstream.next_in = const_cast<z_const Bytef*>(input);
    zstream.avail_in = inputLength;

    if (!initailized) {
        auto initializeResult = initialize();
        if (initializeResult.hasException())
            return initializeResult.releaseException();
    }

    while (shouldCompress) {
        storage.append(Vector<uint8_t>(allocateSize));
        totalSize += allocateSize;

        zstream.next_out = storage[index].data();
        zstream.avail_out = storage[index].size();

        result = deflate(&zstream, (finish) ? Z_FINISH : Z_NO_FLUSH);
        if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
            return Exception { TypeError, "Failed to compress data."_s };

        if (!zstream.avail_in)
            shouldCompress = false;
        else {
            index++;
            if (allocateSize < 1073741824) // 1GB
                allocateSize *= 2;
        }
    }

    storage.at(index).resize(allocateSize - zstream.avail_out);
    totalSize -= zstream.avail_out;

    Vector<uint8_t> output;
    output.reserveCapacity(totalSize);
    for (auto& storageElement : storage)
        output.append(storageElement.data(), storageElement.size());

    return output;
}
} // namespace WebCore
