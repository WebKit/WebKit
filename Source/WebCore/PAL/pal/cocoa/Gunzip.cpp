/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "Gunzip.h"

#include <compression.h>

namespace PAL {

Vector<LChar> gunzip(const unsigned char* data, size_t length)
{
    Vector<LChar> result;

    // Parse the gzip header.
    auto checks = [&]() {
        return length >= 10 && data[0] == 0x1f && data[1] == 0x8b && data[2] == 0x8 && data[3] == 0x0;
    };
    ASSERT(checks());
    if (!checks())
        return { };

    constexpr auto ignoredByteCount = 10;

    compression_stream stream;
    auto status = compression_stream_init(&stream, COMPRESSION_STREAM_DECODE, COMPRESSION_ZLIB);
    ASSERT(status == COMPRESSION_STATUS_OK);
    if (status != COMPRESSION_STATUS_OK)
        return { };
    stream.dst_ptr = result.data();
    stream.dst_size = result.size();
    stream.src_ptr = data + ignoredByteCount;
    stream.src_size = length - ignoredByteCount;
    size_t offset = 0;

    do {
        uint8_t* originalDestinationPointer = stream.dst_ptr;
        status = compression_stream_process(&stream, COMPRESSION_STREAM_FINALIZE);
        uint8_t* newDestinationPointer = stream.dst_ptr;
        offset += newDestinationPointer - originalDestinationPointer;
        switch (status) {
        case COMPRESSION_STATUS_OK: {
            auto newSize = offset * 1.5 + 1;
            // FIXME: We can get better performance if we, instead of resizing the buffer, we allocate distinct chunks and have a postprocessing step which concatenates the chunks together.
            if (newSize > result.size())
                result.grow(newSize);
            stream.dst_ptr = result.data() + offset;
            stream.dst_size = result.size() - offset;
            break;
        }
        case COMPRESSION_STATUS_END:
            status = compression_stream_destroy(&stream);
            ASSERT(status == COMPRESSION_STATUS_OK);
            if (status == COMPRESSION_STATUS_OK) {
                result.shrink(stream.dst_ptr - result.data());
                return result;
            }
            return { };
        case COMPRESSION_STATUS_ERROR:
        default:
            ASSERT_NOT_REACHED();
            compression_stream_destroy(&stream);
            return { };
        }
    } while (true);
}

} // namespace WTF
