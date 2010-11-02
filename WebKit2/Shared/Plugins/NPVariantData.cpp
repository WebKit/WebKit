/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(PLUGIN_PROCESS)

#include "NPVariantData.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "NotImplemented.h"

namespace WebKit {

NPVariantData::NPVariantData()
    : m_type(NPVariantData::Void)
    , m_boolValue(false)
    , m_doubleValue(0)
{
}

NPVariantData NPVariantData::makeVoid()
{
    return NPVariantData();
}

NPVariantData NPVariantData::makeBool(bool value)
{
    NPVariantData npVariantData;

    npVariantData.m_type = NPVariantData::Bool;
    npVariantData.m_boolValue = value;

    return npVariantData;
}

NPVariantData NPVariantData::makeDouble(double value)
{
    NPVariantData npVariantData;

    npVariantData.m_type = NPVariantData::Double;
    npVariantData.m_doubleValue = value;

    return npVariantData;
}

void NPVariantData::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(m_type);

    switch (type()) {
    case NPVariantData::Void:
        break;
    case NPVariantData::Bool:
        encoder->encode(m_boolValue);
        break;
    case NPVariantData::Double:
        encoder->encode(m_doubleValue);
        break;
    }
}

bool NPVariantData::decode(CoreIPC::ArgumentDecoder* decoder, NPVariantData& result)
{
    if (!decoder->decode(result.m_type))
        return false;

    switch (result.m_type) {
    case NPVariantData::Void:
        return true;
    case NPVariantData::Bool:
        return decoder->decode(result.m_boolValue);
    case NPVariantData::Double:
        return decoder->decode(result.m_doubleValue);
    default:
        return false;
    }
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
