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
#include "ZStream.h"

namespace WebCore {

bool ZStream::initializeIfNecessary(Algorithm algorithm, Operation operation)
{
    if (m_isInitialized)
        return true;

    int result = Z_OK;

    switch (operation) {
    case Operation::Compression:
        switch (algorithm) {
        // Values chosen here are based off
        // https://developer.apple.com/documentation/compression/compression_algorithm/compression_zlib?language=objc
        case Algorithm::Deflate:
            result = deflateInit2(&m_stream, 5, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
            break;
        case Algorithm::Zlib:
            result = deflateInit2(&m_stream, 5, Z_DEFLATED, 15, 8, Z_DEFAULT_STRATEGY);
            break;
        case Algorithm::Gzip:
            result = deflateInit2(&m_stream, 5, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
            break;
        }
        break;
    case Operation::Decompression:
        switch (algorithm) {
        case Algorithm::Deflate:
            result = inflateInit2(&m_stream, -15);
            break;
        case Algorithm::Zlib:
            result = inflateInit2(&m_stream, 15);
            break;
        case Algorithm::Gzip:
            result = inflateInit2(&m_stream, 15 + 16);
            break;
        }
        break;
    }
    if (result != Z_OK)
        return false;
    m_isInitialized = true;
    return true;
}

ZStream::ZStream()
{
    std::memset(&m_stream, 0, sizeof(m_stream));
}

ZStream::~ZStream()
{
    if (m_isInitialized)
        deflateEnd(&m_stream);
}

}
