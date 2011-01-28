/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DecoderAdapter.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

DecoderAdapter::DecoderAdapter(const uint8_t* buffer, size_t bufferSize)
    : m_decoder(buffer, bufferSize)
{
}

bool DecoderAdapter::decodeBytes(Vector<uint8_t>& bytes)
{
    return m_decoder.decodeBytes(bytes);
}

bool DecoderAdapter::decodeBool(bool& value)
{
    return m_decoder.decodeBool(value);
}

bool DecoderAdapter::decodeUInt32(uint32_t& value)
{
    return m_decoder.decodeUInt32(value);
}

bool DecoderAdapter::decodeUInt64(uint64_t& value)
{
    return m_decoder.decodeUInt64(value);
}

bool DecoderAdapter::decodeInt32(int32_t& value)
{
    return m_decoder.decodeInt32(value);
}

bool DecoderAdapter::decodeInt64(int64_t& value)
{
    return m_decoder.decodeInt64(value);
}

bool DecoderAdapter::decodeFloat(float& value)
{
    return m_decoder.decodeFloat(value);
}

bool DecoderAdapter::decodeDouble(double& value)
{
    return m_decoder.decodeDouble(value);
}

bool DecoderAdapter::decodeString(String& value)
{
    return m_decoder.decode(value);
}

}
