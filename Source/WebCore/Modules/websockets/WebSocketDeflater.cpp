/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSocketDeflater.h"

#include "Logging.h"
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <zlib.h>

namespace WebCore {

static const int defaultMemLevel = 8;
static const size_t bufferIncrementUnit = 4096;

WebSocketDeflater::WebSocketDeflater(int windowBits, ContextTakeOverMode contextTakeOverMode)
    : m_windowBits(windowBits)
    , m_contextTakeOverMode(contextTakeOverMode)
{
    ASSERT(m_windowBits >= 8);
    ASSERT(m_windowBits <= 15);
    m_stream = makeUniqueWithoutFastMallocCheck<z_stream>();
    memset(m_stream.get(), 0, sizeof(z_stream));
}

bool WebSocketDeflater::initialize()
{
    return deflateInit2(m_stream.get(), Z_DEFAULT_COMPRESSION, Z_DEFLATED, -m_windowBits, defaultMemLevel, Z_DEFAULT_STRATEGY) == Z_OK;
}

WebSocketDeflater::~WebSocketDeflater()
{
    int result = deflateEnd(m_stream.get());
    if (result != Z_OK)
        LOG(Network, "WebSocketDeflater %p Destructor deflateEnd() failed: %d is returned", this, result);
}

static void setStreamParameter(z_stream* stream, const uint8_t* inputData, size_t inputLength, uint8_t* outputData, size_t outputLength)
{
    stream->next_in = const_cast<uint8_t*>(inputData);
    stream->avail_in = inputLength;
    stream->next_out = outputData;
    stream->avail_out = outputLength;
}

bool WebSocketDeflater::addBytes(const uint8_t* data, size_t length)
{
    if (!length)
        return false;

    size_t maxLength = deflateBound(m_stream.get(), length);
    size_t writePosition = m_buffer.size();
    m_buffer.grow(writePosition + maxLength);
    setStreamParameter(m_stream.get(), data, length, m_buffer.data() + writePosition, maxLength);
    int result = deflate(m_stream.get(), Z_NO_FLUSH);
    if (result != Z_OK || m_stream->avail_in > 0)
        return false;

    m_buffer.shrink(writePosition + maxLength - m_stream->avail_out);
    return true;
}

bool WebSocketDeflater::finish()
{
    while (true) {
        size_t writePosition = m_buffer.size();
        m_buffer.grow(writePosition + bufferIncrementUnit);
        size_t availableCapacity = m_buffer.size() - writePosition;
        setStreamParameter(m_stream.get(), 0, 0, m_buffer.data() + writePosition, availableCapacity);
        int result = deflate(m_stream.get(), Z_SYNC_FLUSH);
        if (m_stream->avail_out) {
            m_buffer.shrink(writePosition + availableCapacity - m_stream->avail_out);
            if (result == Z_OK)
                break;
            if (result != Z_BUF_ERROR)
                return false;
        }
    }
    // Remove 4 octets from the tail as the specification requires.
    if (m_buffer.size() <= 4)
        return false;
    m_buffer.shrink(m_buffer.size() - 4);
    return true;
}

void WebSocketDeflater::reset()
{
    m_buffer.clear();
    if (m_contextTakeOverMode == DoNotTakeOverContext)
        deflateReset(m_stream.get());
}

WebSocketInflater::WebSocketInflater(int windowBits)
    : m_windowBits(windowBits)
{
    m_stream = makeUniqueWithoutFastMallocCheck<z_stream>();
    memset(m_stream.get(), 0, sizeof(z_stream));
}

bool WebSocketInflater::initialize()
{
    return inflateInit2(m_stream.get(), -m_windowBits) == Z_OK;
}

WebSocketInflater::~WebSocketInflater()
{
    int result = inflateEnd(m_stream.get());
    if (result != Z_OK)
        LOG(Network, "WebSocketInflater %p Destructor inflateEnd() failed: %d is returned", this, result);
}

bool WebSocketInflater::addBytes(const uint8_t* data, size_t length)
{
    if (!length)
        return false;

    size_t consumedSoFar = 0;
    while (consumedSoFar < length) {
        size_t writePosition = m_buffer.size();
        m_buffer.grow(writePosition + bufferIncrementUnit);
        size_t availableCapacity = m_buffer.size() - writePosition;
        size_t remainingLength = length - consumedSoFar;
        setStreamParameter(m_stream.get(), data + consumedSoFar, remainingLength, m_buffer.data() + writePosition, availableCapacity);
        int result = inflate(m_stream.get(), Z_NO_FLUSH);
        consumedSoFar += remainingLength - m_stream->avail_in;
        m_buffer.shrink(writePosition + availableCapacity - m_stream->avail_out);
        if (result == Z_BUF_ERROR)
            continue;
        if (result == Z_STREAM_END) {
            // Received a block with BFINAL set to 1. Reset decompression state.
            if (inflateReset(m_stream.get()) != Z_OK)
                return false;
            continue;
        }
        if (result != Z_OK)
            return false;
        ASSERT(remainingLength > m_stream->avail_in);
    }
    ASSERT(consumedSoFar == length);
    return true;
}

bool WebSocketInflater::finish()
{
    constexpr uint8_t strippedFields[] = { 0, 0, 0xFF, 0xFF };
    constexpr auto strippedLength = std::size(strippedFields);

    // Appends 4 octests of 0x00 0x00 0xff 0xff
    size_t consumedSoFar = 0;
    while (consumedSoFar < strippedLength) {
        size_t writePosition = m_buffer.size();
        m_buffer.grow(writePosition + bufferIncrementUnit);
        size_t availableCapacity = m_buffer.size() - writePosition;
        size_t remainingLength = strippedLength - consumedSoFar;
        setStreamParameter(m_stream.get(), strippedFields + consumedSoFar, remainingLength, m_buffer.data() + writePosition, availableCapacity);
        int result = inflate(m_stream.get(), Z_FINISH);
        consumedSoFar += remainingLength - m_stream->avail_in;
        m_buffer.shrink(writePosition + availableCapacity - m_stream->avail_out);
        if (result == Z_BUF_ERROR)
            continue;
        if (result != Z_OK && result != Z_STREAM_END)
            return false;
        ASSERT(remainingLength > m_stream->avail_in);
    }
    ASSERT(consumedSoFar == strippedLength);

    return true;
}

void WebSocketInflater::reset()
{
    m_buffer.clear();
}

} // namespace WebCore
