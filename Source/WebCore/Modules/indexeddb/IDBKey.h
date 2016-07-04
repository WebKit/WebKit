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

#include "IndexedDB.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

using WebCore::IndexedDB::KeyType;

namespace WebCore {

class IDBKey : public RefCounted<IDBKey> {
public:
    static Ref<IDBKey> createInvalid()
    {
        return adoptRef(*new IDBKey());
    }

    static Ref<IDBKey> createNumber(double number)
    {
        return adoptRef(*new IDBKey(KeyType::Number, number));
    }

    static Ref<IDBKey> createString(const String& string)
    {
        return adoptRef(*new IDBKey(string));
    }

    static Ref<IDBKey> createDate(double date)
    {
        return adoptRef(*new IDBKey(KeyType::Date, date));
    }

    static Ref<IDBKey> createMultiEntryArray(const Vector<RefPtr<IDBKey>>& array)
    {
        Vector<RefPtr<IDBKey>> result;

        size_t sizeEstimate = 0;
        for (auto& key : array) {
            if (!key->isValid())
                continue;

            bool skip = false;
            for (auto& resultKey : result) {
                if (key->isEqual(*resultKey)) {
                    skip = true;
                    break;
                }
            }
            if (!skip) {
                result.append(key);
                sizeEstimate += key->m_sizeEstimate;
            }
        }
        Ref<IDBKey> idbKey = adoptRef(*new IDBKey(result, sizeEstimate));
        ASSERT(idbKey->isValid());
        return idbKey;
    }

    static Ref<IDBKey> createArray(const Vector<RefPtr<IDBKey>>& array)
    {
        size_t sizeEstimate = 0;
        for (auto& key : array)
            sizeEstimate += key->m_sizeEstimate;

        return adoptRef(*new IDBKey(array, sizeEstimate));
    }

    WEBCORE_EXPORT ~IDBKey();

    KeyType type() const { return m_type; }
    WEBCORE_EXPORT bool isValid() const;

    const Vector<RefPtr<IDBKey>>& array() const
    {
        ASSERT(m_type == KeyType::Array);
        return m_array;
    }

    const String& string() const
    {
        ASSERT(m_type == KeyType::String);
        return m_string;
    }

    double date() const
    {
        ASSERT(m_type == KeyType::Date);
        return m_number;
    }

    double number() const
    {
        ASSERT(m_type == KeyType::Number);
        return m_number;
    }

    int compare(const IDBKey& other) const;
    bool isLessThan(const IDBKey& other) const;
    bool isEqual(const IDBKey& other) const;

    size_t sizeEstimate() const { return m_sizeEstimate; }

    static int compareTypes(KeyType a, KeyType b)
    {
        return b - a;
    }

    using RefCounted<IDBKey>::ref;
    using RefCounted<IDBKey>::deref;

#if !LOG_DISABLED
    String loggingString() const;
#endif

private:
    IDBKey() : m_type(KeyType::Invalid), m_number(0), m_sizeEstimate(OverheadSize) { }
    IDBKey(KeyType type, double number) : m_type(type), m_number(number), m_sizeEstimate(OverheadSize + sizeof(double)) { }
    explicit IDBKey(const String& value) : m_type(KeyType::String), m_string(value), m_number(0), m_sizeEstimate(OverheadSize + value.length() * sizeof(UChar)) { }
    IDBKey(const Vector<RefPtr<IDBKey>>& keyArray, size_t arraySize) : m_type(KeyType::Array), m_array(keyArray), m_number(0), m_sizeEstimate(OverheadSize + arraySize) { }

    const KeyType m_type;
    const Vector<RefPtr<IDBKey>> m_array;
    const String m_string;
    const double m_number;

    const size_t m_sizeEstimate;

    // Very rough estimate of minimum key size overhead.
    enum { OverheadSize = 16 };
};

}

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBKey_h
