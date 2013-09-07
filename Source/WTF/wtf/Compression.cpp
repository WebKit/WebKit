/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Compression.h"

#include "CheckedArithmetic.h"

#if USE(ZLIB) && !COMPILER(MSVC)

#include <string.h>
#include <zlib.h>

namespace WTF {

static void* zAlloc(void*, uint32_t count, uint32_t size)
{
    CheckedSize allocSize = count;
    allocSize *= size;
    if (allocSize.hasOverflowed())
        return Z_NULL;
    void* result = 0;
    if (tryFastMalloc(allocSize.unsafeGet()).getValue(result))
        return result;
    return Z_NULL;
}

static void zFree(void*, void* data)
{
    fastFree(data);
}

PassOwnPtr<GenericCompressedData> GenericCompressedData::create(const uint8_t* data, size_t dataLength)
{
    enum { MinimumSize = sizeof(GenericCompressedData) * 8 };

    if (!data || dataLength < MinimumSize)
        return nullptr;

    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = zAlloc;
    stream.zfree = zFree;
    stream.data_type = Z_BINARY;
    stream.opaque = Z_NULL;
    stream.avail_in = dataLength;
    stream.next_in = const_cast<uint8_t*>(data);

    size_t currentOffset = OBJECT_OFFSETOF(GenericCompressedData, m_data);
    size_t currentCapacity = fastMallocGoodSize(MinimumSize);
    Bytef* compressedData = static_cast<Bytef*>(fastMalloc(currentCapacity));
    memset(compressedData, 0, sizeof(GenericCompressedData));
    stream.next_out = compressedData + currentOffset;
    stream.avail_out = currentCapacity - currentOffset;

    deflateInit(&stream, Z_BEST_COMPRESSION);

    while (true) {
        int deflateResult = deflate(&stream, Z_FINISH);
        if (deflateResult == Z_OK || !stream.avail_out) {
            size_t newCapacity = 0;
            currentCapacity -= stream.avail_out;
            if (!stream.avail_in)
                newCapacity = currentCapacity + 8;
            else {
                // Determine average capacity
                size_t compressedContent = stream.next_in - data;
                double expectedSize = static_cast<double>(dataLength) * compressedContent / currentCapacity;

                // Expand capacity by at least 8 bytes so we're always growing, and to
                // compensate for any exaggerated ideas of how effectively we'll compress
                // data in the future.
                newCapacity = std::max(static_cast<size_t>(expectedSize + 8), currentCapacity + 8);
            }
            newCapacity = fastMallocGoodSize(newCapacity);
            if (newCapacity >= dataLength)
                goto fail;
            compressedData = static_cast<Bytef*>(fastRealloc(compressedData, newCapacity));
            currentOffset = currentCapacity - stream.avail_out;
            stream.next_out = compressedData + currentOffset;
            stream.avail_out = newCapacity - currentCapacity;
            currentCapacity = newCapacity;
            continue;
        }

        if (deflateResult == Z_STREAM_END) {
            ASSERT(!stream.avail_in);
            break;
        }

        ASSERT_NOT_REACHED();
    fail:
        deflateEnd(&stream);
        fastFree(compressedData);
        return nullptr;
    }
    deflateEnd(&stream);
    static int64_t totalCompressed = 0;
    static int64_t totalInput = 0;

    totalCompressed += currentCapacity;
    totalInput += dataLength;
    GenericCompressedData* result = new (compressedData) GenericCompressedData(dataLength, stream.total_out);
    return adoptPtr(result);
}

bool GenericCompressedData::decompress(uint8_t* destination, size_t bufferSize, size_t* decompressedByteCount)
{
    if (decompressedByteCount)
        *decompressedByteCount = 0;
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = zAlloc;
    stream.zfree = zFree;
    stream.data_type = Z_BINARY;
    stream.opaque = Z_NULL;
    stream.next_out = destination;
    stream.avail_out = bufferSize;
    stream.next_in = m_data;
    stream.avail_in = compressedSize();
    if (inflateInit(&stream) != Z_OK) {
        ASSERT_NOT_REACHED();
        return false;
    }

    int inflateResult = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    ASSERT(stream.total_out <= bufferSize);
    if (decompressedByteCount)
        *decompressedByteCount = stream.total_out;

    if (inflateResult != Z_STREAM_END) {
        ASSERT_NOT_REACHED();
        return false;
    }

    return true;
}

}

#else

namespace WTF {
PassOwnPtr<GenericCompressedData> GenericCompressedData::create(const uint8_t*, size_t)
{
    return nullptr;
}

bool GenericCompressedData::decompress(uint8_t*, size_t, size_t*)
{
    return false;
}
}

#endif
