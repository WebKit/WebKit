/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef IDBGetResult_h
#define IDBGetResult_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include "IDBKeyData.h"
#include "IDBKeyPath.h"
#include "SharedBuffer.h"
#include "ThreadSafeDataBuffer.h"

namespace WebCore {

class IDBGetResult {
public:
    IDBGetResult()
        : m_isDefined(false)
    {
    }

    IDBGetResult(SharedBuffer* buffer)
    {
        if (buffer)
            dataFromBuffer(*buffer);
    }

    IDBGetResult(const ThreadSafeDataBuffer& buffer)
        : m_valueBuffer(buffer)
    {
    }

    IDBGetResult(PassRefPtr<IDBKey> key)
        : m_keyData(key.get())
    {
    }

    IDBGetResult(const IDBKeyData& keyData)
        : m_keyData(keyData)
    {
    }

    IDBGetResult(PassRefPtr<SharedBuffer> buffer, PassRefPtr<IDBKey> key, const IDBKeyPath& path)
        : m_keyData(key.get())
        , m_keyPath(path)
    {
        if (buffer)
            dataFromBuffer(*buffer);
    }

    IDBGetResult(const IDBKeyData& keyData, const IDBKeyData& primaryKeyData)
        : m_keyData(keyData)
        , m_primaryKeyData(primaryKeyData)
    {
    }

    IDBGetResult(const IDBKeyData& keyData, const IDBKeyData& primaryKeyData, const ThreadSafeDataBuffer& valueBuffer)
        : m_valueBuffer(valueBuffer)
        , m_keyData(keyData)
        , m_primaryKeyData(primaryKeyData)
    {
    }

    IDBGetResult isolatedCopy() const;

    const ThreadSafeDataBuffer& valueBuffer() const { return m_valueBuffer; }
    const IDBKeyData& keyData() const { return m_keyData; }
    const IDBKeyData& primaryKeyData() const { return m_primaryKeyData; }
    const IDBKeyPath& keyPath() const { return m_keyPath; }
    bool isDefined() const { return m_isDefined; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static bool decode(Decoder&, IDBGetResult&);

    // FIXME: When removing LegacyIDB, remove these setters.
    // https://bugs.webkit.org/show_bug.cgi?id=150854

    void setValueBuffer(const ThreadSafeDataBuffer& valueBuffer) { m_valueBuffer = valueBuffer; }
    void setKeyData(const IDBKeyData& keyData) { m_keyData = keyData; }
    void setPrimaryKeyData(const IDBKeyData& keyData) { m_primaryKeyData = keyData; }
    void setKeyPath(const IDBKeyPath& keyPath) { m_keyPath = keyPath; }

private:
    WEBCORE_EXPORT void dataFromBuffer(SharedBuffer&);

    ThreadSafeDataBuffer m_valueBuffer;
    IDBKeyData m_keyData;
    IDBKeyData m_primaryKeyData;
    IDBKeyPath m_keyPath;
    bool m_isDefined { true };
};

template<class Encoder>
void IDBGetResult::encode(Encoder& encoder) const
{
    encoder << m_keyData << m_primaryKeyData << m_keyPath << m_isDefined;

    encoder << !!m_valueBuffer.data();
    if (m_valueBuffer.data())
        encoder << *m_valueBuffer.data();
}

template<class Decoder>
bool IDBGetResult::decode(Decoder& decoder, IDBGetResult& result)
{
    if (!decoder.decode(result.m_keyData))
        return false;

    if (!decoder.decode(result.m_primaryKeyData))
        return false;

    if (!decoder.decode(result.m_keyPath))
        return false;

    if (!decoder.decode(result.m_isDefined))
        return false;

    bool hasObject;
    if (!decoder.decode(hasObject))
        return false;

    if (hasObject) {
        Vector<uint8_t> value;
        if (!decoder.decode(value))
            return false;
        result.m_valueBuffer = ThreadSafeDataBuffer::adoptVector(value);
    }

    return true;
}

} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // IDBGetResult_h
