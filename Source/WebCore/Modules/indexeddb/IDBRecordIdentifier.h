/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef IDBRecordIdentifier_h
#define IDBRecordIdentifier_h

#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

class IDBRecordIdentifier : public RefCounted<IDBRecordIdentifier> {
public:
    static PassRefPtr<IDBRecordIdentifier> create(const Vector<char>& encodedPrimaryKey, int64_t version)
    {
        return adoptRef(new IDBRecordIdentifier(encodedPrimaryKey, version));
    }

    static PassRefPtr<IDBRecordIdentifier> create()
    {
        return adoptRef(new IDBRecordIdentifier);
    }

    const Vector<char>& encodedPrimaryKey() const { return m_encodedPrimaryKey; }
    int64_t version() const { return m_version; }
    void reset(const Vector<char>& encodedPrimaryKey, int64_t version)
    {
        m_encodedPrimaryKey = encodedPrimaryKey;
        m_version = version;
    }

private:
    IDBRecordIdentifier(const Vector<char>& encodedPrimaryKey, int64_t version)
        : m_encodedPrimaryKey(encodedPrimaryKey)
        , m_version(version)
    {
        ASSERT(!encodedPrimaryKey.isEmpty());
    }

    IDBRecordIdentifier()
        : m_version(-1)
    {
    }

    Vector<char> m_encodedPrimaryKey;
    int64_t m_version;
};

} // namespace WebCore

#endif

#endif // IDBRecordIdentifier_h
