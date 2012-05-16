/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef IDBKey_h
#define IDBKey_h

#if ENABLE(INDEXED_DATABASE)

#include "PlatformString.h"
#include <wtf/Forward.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

namespace WebCore {

class IDBKey : public ThreadSafeRefCounted<IDBKey> {
public:
    typedef Vector<RefPtr<IDBKey> > KeyArray;

    static PassRefPtr<IDBKey> createInvalid()
    {
        RefPtr<IDBKey> idbKey = adoptRef(new IDBKey());
        idbKey->m_type = InvalidType;
        return idbKey.release();
    }

    static PassRefPtr<IDBKey> createNumber(double number)
    {
        RefPtr<IDBKey> idbKey = adoptRef(new IDBKey());
        idbKey->m_type = NumberType;
        idbKey->m_number = number;
        idbKey->m_sizeEstimate += sizeof(double);
        return idbKey.release();
    }

    static PassRefPtr<IDBKey> createString(const String& string)
    {
        RefPtr<IDBKey> idbKey = adoptRef(new IDBKey());
        idbKey->m_type = StringType;
        idbKey->m_string = string;
        idbKey->m_sizeEstimate += string.length() * sizeof(UChar);
        return idbKey.release();
    }

    static PassRefPtr<IDBKey> createDate(double date)
    {
        RefPtr<IDBKey> idbKey = adoptRef(new IDBKey());
        idbKey->m_type = DateType;
        idbKey->m_date = date;
        idbKey->m_sizeEstimate += sizeof(double);
        return idbKey.release();
    }

    static PassRefPtr<IDBKey> createArray(const KeyArray& array)
    {
        RefPtr<IDBKey> idbKey = adoptRef(new IDBKey());
        idbKey->m_type = ArrayType;
        idbKey->m_array = array;

        for (size_t i = 0; i < array.size(); ++i)
            idbKey->m_sizeEstimate += array[i]->m_sizeEstimate;

        return idbKey.release();
    }

    ~IDBKey();

    // In order of the least to the highest precedent in terms of sort order.
    enum Type {
        InvalidType = 0,
        ArrayType,
        StringType,
        DateType,
        NumberType,
        MinType
    };

    Type type() const { return m_type; }
    bool isValid() const { return m_type != InvalidType; }

    const KeyArray& array() const
    {
        ASSERT(m_type == ArrayType);
        return m_array;
    }

    const String& string() const
    {
        ASSERT(m_type == StringType);
        return m_string;
    }

    double date() const
    {
        ASSERT(m_type == DateType);
        return m_date;
    }

    double number() const
    {
        ASSERT(m_type == NumberType);
        return m_number;
    }

    int compare(const IDBKey* other) const;
    bool isLessThan(const IDBKey* other) const;
    bool isEqual(const IDBKey* other) const;

    size_t sizeEstimate() const { return m_sizeEstimate; }

    static int compareTypes(Type a, Type b)
    {
        return b - a;
    }

    using ThreadSafeRefCounted<IDBKey>::ref;
    using ThreadSafeRefCounted<IDBKey>::deref;

private:
    IDBKey();

    Type m_type;
    KeyArray m_array;
    String m_string;
    double m_date;
    double m_number;

    size_t m_sizeEstimate;

    // Very rough estimate of minimum key size overhead.
    enum { kOverheadSize = 16 };
};

}

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBKey_h
