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
#include "EncoderAdapter.h"

#include "DataReference.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

EncoderAdapter::EncoderAdapter()
    : m_encoder(CoreIPC::ArgumentEncoder::create(0))
{
}

CoreIPC::DataReference EncoderAdapter::data() const
{
    return CoreIPC::DataReference(m_encoder->buffer(), m_encoder->bufferSize());
}

void EncoderAdapter::encodeBytes(const uint8_t* bytes, size_t size)
{
    m_encoder->encodeBytes(bytes, size);
}

void EncoderAdapter::encodeBool(bool value)
{
    m_encoder->encodeBool(value);
}

void EncoderAdapter::encodeUInt32(uint32_t value)
{
    m_encoder->encodeUInt32(value);
}

void EncoderAdapter::encodeUInt64(uint64_t value)
{
    m_encoder->encodeUInt64(value);
}

void EncoderAdapter::encodeInt32(int32_t value)
{
    m_encoder->encodeInt32(value);
}

void EncoderAdapter::encodeInt64(int64_t value)
{
    m_encoder->encodeInt64(value);
}

void EncoderAdapter::encodeFloat(float value)
{
    m_encoder->encodeFloat(value);
}

void EncoderAdapter::encodeDouble(double value)
{
    m_encoder->encodeDouble(value);
}

void EncoderAdapter::encodeString(const String& value)
{
    m_encoder->encode(value);
}

}
