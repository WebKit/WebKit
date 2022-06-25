/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "ThreadSafeDataBuffer.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class SerializedScriptValue;

class IDBValue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT IDBValue();
    IDBValue(const SerializedScriptValue&);
    IDBValue(const ThreadSafeDataBuffer&);
    IDBValue(const SerializedScriptValue&, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths);
    IDBValue(const ThreadSafeDataBuffer&, Vector<String>&& blobURLs, Vector<String>&& blobFilePaths);
    IDBValue(const ThreadSafeDataBuffer&, const Vector<String>& blobURLs, const Vector<String>& blobFilePaths);

    void setAsIsolatedCopy(const IDBValue&);
    WEBCORE_EXPORT IDBValue isolatedCopy() const;

    const ThreadSafeDataBuffer& data() const { return m_data; }
    const Vector<String>& blobURLs() const { return m_blobURLs; }
    const Vector<String>& blobFilePaths() const { return m_blobFilePaths; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<IDBValue> decode(Decoder&);

    size_t size() const;
private:
    ThreadSafeDataBuffer m_data;
    Vector<String> m_blobURLs;
    Vector<String> m_blobFilePaths;
};

template<class Encoder>
void IDBValue::encode(Encoder& encoder) const
{
    encoder << m_data;
    encoder << m_blobURLs;
    encoder << m_blobFilePaths;
}

template<class Decoder>
std::optional<IDBValue> IDBValue::decode(Decoder& decoder)
{
    IDBValue result;
    if (!decoder.decode(result.m_data))
        return std::nullopt;

    if (!decoder.decode(result.m_blobURLs))
        return std::nullopt;

    if (!decoder.decode(result.m_blobFilePaths))
        return std::nullopt;

    return result;
}

} // namespace WebCore
