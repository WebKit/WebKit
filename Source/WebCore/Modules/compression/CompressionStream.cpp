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
#include "CompressionStream.h"

#include <wtf/StdLibExtras.h>

namespace WebCore {

CompressionStream::CompressionStream()
{
#if PLATFORM(COCOA)
    zeroBytes(m_stream);
#endif
}

CompressionStream::~CompressionStream()
{
#if PLATFORM(COCOA)
    if (m_isInitialized)
        compression_stream_destroy(&m_stream);
#endif
}

bool CompressionStream::initializeIfNecessary(Algorithm algorithm, Operation operation)
{
    if (m_isInitialized)
        return true;
#if PLATFORM(COCOA)
    switch (algorithm) {
    case Algorithm::Brotli:
        auto result = compression_stream_init(&m_stream, operation == Operation::Compression ? COMPRESSION_STREAM_ENCODE : COMPRESSION_STREAM_DECODE, COMPRESSION_BROTLI);
        if (result != COMPRESSION_STATUS_OK)
            return false;
        break;
    }
#else
    UNUSED_PARAM(algorithm);
    UNUSED_PARAM(operation);
#endif
    m_isInitialized = true;
    return true;
}

}
