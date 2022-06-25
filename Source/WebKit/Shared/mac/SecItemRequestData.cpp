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
#include "SecItemRequestData.h"

#include "ArgumentCoders.h"
#include "ArgumentCodersCF.h"

namespace WebKit {

SecItemRequestData::SecItemRequestData(Type type, CFDictionaryRef query)
    : m_type(type)
    , m_queryDictionary(query)
{
}

SecItemRequestData::SecItemRequestData(Type type, CFDictionaryRef query, CFDictionaryRef attributesToMatch)
    : m_type(type)
    , m_queryDictionary(query)
    , m_attributesToMatch(attributesToMatch)
{
}

void SecItemRequestData::encode(IPC::Encoder& encoder) const
{
    encoder << m_type;

    encoder << static_cast<bool>(m_queryDictionary);
    if (m_queryDictionary)
        encoder << m_queryDictionary;

    encoder << static_cast<bool>(m_attributesToMatch);
    if (m_attributesToMatch)
        encoder << m_attributesToMatch;
}

bool SecItemRequestData::decode(IPC::Decoder& decoder, SecItemRequestData& secItemRequestData)
{
    if (!decoder.decode(secItemRequestData.m_type))
        return false;

    bool expectQuery;
    if (!decoder.decode(expectQuery))
        return false;

    if (expectQuery && !decoder.decode(secItemRequestData.m_queryDictionary))
        return false;
    
    bool expectAttributes;
    if (!decoder.decode(expectAttributes))
        return false;
    
    if (expectAttributes && !decoder.decode(secItemRequestData.m_attributesToMatch))
        return false;
    
    return true;
}

} // namespace WebKit
