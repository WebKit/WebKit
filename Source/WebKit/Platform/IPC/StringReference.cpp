/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "StringReference.h"

#include "ArgumentCoders.h"
#include "DataReference.h"
#include "Decoder.h"
#include "Encoder.h"
#include <wtf/Hasher.h>
#include <wtf/text/CString.h>

namespace IPC {

CString StringReference::toString() const
{
    return WTF::CString(m_data, m_size);
}

void StringReference::encode(Encoder& encoder) const
{
    encoder << DataReference(reinterpret_cast<const uint8_t*>(m_data), m_size);
}

bool StringReference::decode(Decoder& decoder, StringReference& result)
{
    DataReference dataReference;
    if (!decoder.decode(dataReference))
        return false;

    result.m_data = reinterpret_cast<const char*>(dataReference.data());
    result.m_size = dataReference.size();

    return true;
}

unsigned StringReference::Hash::hash(const StringReference& a)
{
    return StringHasher::computeHash(reinterpret_cast<const unsigned char*>(a.data()), a.size());
}

} // namespace IPC
