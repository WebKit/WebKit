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

#ifndef StringReference_h
#define StringReference_h

#include <string.h>
#include <wtf/Forward.h>
#include <wtf/HashTraits.h>

namespace IPC {

class Encoder;
class Decoder;

class StringReference {
public:
    StringReference()
        : m_data(0)
        , m_size(0)
    {
    }

    StringReference(const char* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    template<size_t length>
    StringReference(const char (&string)[length])
        : m_data(string)
        , m_size(length - 1)
    {
    }

    bool isEmpty() const { return !m_size; }

    size_t size() const { return m_size; }
    const char* data() const { return m_data; }

    CString toString() const;

    void encode(Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(Decoder&, StringReference&);

    struct Hash {
        static unsigned hash(const StringReference& a);
        static bool equal(const StringReference&, const StringReference&);
        static const bool safeToCompareToEmptyOrDeleted = true;
    };

private:
    const char* m_data;
    size_t m_size;
};

inline bool operator==(const StringReference& a, const StringReference& b)
{
    return a.size() == b.size() && !memcmp(a.data(), b.data(), a.size());
}

inline bool operator!=(const StringReference& a, const StringReference& b)
{
    return !(a == b);
}

inline bool StringReference::Hash::equal(const StringReference& a, const StringReference& b)
{
    return a == b;
}

} // namespace IPC

namespace WTF {
template<typename T> struct DefaultHash;

template<> struct DefaultHash<IPC::StringReference> : IPC::StringReference::Hash { };

template<> struct HashTraits<IPC::StringReference> : GenericHashTraits<IPC::StringReference> {
    static const bool emptyValueIsZero = 0;
    static void constructDeletedValue(IPC::StringReference& stringReference) { stringReference = IPC::StringReference(0, std::numeric_limits<size_t>::max()); }
    static bool isDeletedValue(const IPC::StringReference& stringReference) { return stringReference.size() == std::numeric_limits<size_t>::max(); }
};

} // namespace WTF

#endif // StringReference_h
