/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef UStringImpl_h
#define UStringImpl_h

#include <limits>
#include <wtf/CrossThreadRefCounted.h>
#include <wtf/OwnFastMallocPtr.h>
#include <wtf/PossiblyNull.h>
#include <wtf/StringHashFunctions.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>
#include <wtf/text/StringImplBase.h>

namespace JSC {

class IdentifierTable;

typedef OwnFastMallocPtr<const UChar> SharableUChar;
typedef CrossThreadRefCounted<SharableUChar> SharedUChar;

class UStringImpl : public StringImplBase {
    friend struct CStringTranslator;
    friend struct UCharBufferTranslator;
    friend class JIT;
    friend class SmallStringsStorage;
    friend void initializeUString();
private:
    // For SmallStringStorage, which allocates an array and uses an in-place new.
    UStringImpl() { }

    // Used to construct static strings, which have an special refCount that can never hit zero.
    // This means that the static string will never be destroyed, which is important because
    // static strings will be shared across threads & ref-counted in a non-threadsafe manner.
    UStringImpl(const UChar* characters, unsigned length, StaticStringConstructType)
        : StringImplBase(length, ConstructStaticString)
        , m_data(characters)
        , m_buffer(0)
        , m_hash(0)
    {
        hash();
    }

    // Create a normal string with internal storage (BufferInternal)
    UStringImpl(unsigned length)
        : StringImplBase(length, BufferInternal)
        , m_data(reinterpret_cast<UChar*>(this + 1))
        , m_buffer(0)
        , m_hash(0)
    {
        ASSERT(m_data);
        ASSERT(m_length);
    }

    // Create a UStringImpl adopting ownership of the provided buffer (BufferOwned)
    UStringImpl(const UChar* characters, unsigned length)
        : StringImplBase(length, BufferOwned)
        , m_data(characters)
        , m_buffer(0)
        , m_hash(0)
    {
        ASSERT(m_data);
        ASSERT(m_length);
    }

    // Used to create new strings that are a substring of an existing UStringImpl (BufferSubstring)
    UStringImpl(const UChar* characters, unsigned length, PassRefPtr<UStringImpl> base)
        : StringImplBase(length, BufferSubstring)
        , m_data(characters)
        , m_substringBuffer(base.releaseRef())
        , m_hash(0)
    {
        ASSERT(m_data);
        ASSERT(m_length);
        ASSERT(m_substringBuffer->bufferOwnership() != BufferSubstring);
    }

    // Used to construct new strings sharing an existing SharedUChar (BufferShared)
    UStringImpl(const UChar* characters, unsigned length, PassRefPtr<SharedUChar> sharedBuffer)
        : StringImplBase(length, BufferShared)
        , m_data(characters)
        , m_sharedBuffer(sharedBuffer.releaseRef())
        , m_hash(0)
    {
        ASSERT(m_data);
        ASSERT(m_length);
    }

    // For use only by Identifier's XXXTranslator helpers.
    void setHash(unsigned hash)
    {
        ASSERT(!isStatic());
        ASSERT(!m_hash);
        ASSERT(hash == computeHash(m_data, m_length));
        m_hash = hash;
    }

public:
    ~UStringImpl();

    static PassRefPtr<UStringImpl> create(const UChar*, unsigned length);
    static PassRefPtr<UStringImpl> create(const char*, unsigned length);
    static PassRefPtr<UStringImpl> create(const char*);
    static PassRefPtr<UStringImpl> create(const UChar*, unsigned length, PassRefPtr<SharedUChar>);
    static PassRefPtr<UStringImpl> create(PassRefPtr<UStringImpl> rep, unsigned offset, unsigned length)
    {
        ASSERT(rep);
        ASSERT(length <= rep->length());

        if (!length)
            return empty();

        UStringImpl* ownerRep = (rep->bufferOwnership() == BufferSubstring) ? rep->m_substringBuffer : rep.get();
        return adoptRef(new UStringImpl(rep->m_data + offset, length, ownerRep));
    }

    static PassRefPtr<UStringImpl> createUninitialized(unsigned length, UChar*& output);
    static PassRefPtr<UStringImpl> tryCreateUninitialized(unsigned length, UChar*& output)
    {
        if (!length) {
            output = 0;
            return empty();
        }

        if (length > ((std::numeric_limits<size_t>::max() - sizeof(UStringImpl)) / sizeof(UChar)))
            return 0;
        UStringImpl* resultImpl;
        if (!tryFastMalloc(sizeof(UChar) * length + sizeof(UStringImpl)).getValue(resultImpl))
            return 0;
        output = reinterpret_cast<UChar*>(resultImpl + 1);
        return adoptRef(new(resultImpl) UStringImpl(length));
    }

    template<size_t inlineCapacity>
    static PassRefPtr<UStringImpl> adopt(Vector<UChar, inlineCapacity>& vector)
    {
        if (size_t size = vector.size()) {
            ASSERT(vector.data());
            return adoptRef(new UStringImpl(vector.releaseBuffer(), size));
        }
        return empty();
    }

    SharedUChar* sharedBuffer();
    const UChar* characters() const { return m_data; }

    size_t cost()
    {
        // For substrings, return the cost of the base string.
        if (bufferOwnership() == BufferSubstring)
            return m_substringBuffer->cost();

        if (m_refCountAndFlags & s_refCountFlagShouldReportedCost) {
            m_refCountAndFlags &= ~s_refCountFlagShouldReportedCost;
            return m_length;
        }
        return 0;
    }

    bool isIdentifier() const { return m_refCountAndFlags & s_refCountFlagIsIdentifier; }
    void setIsIdentifier(bool isIdentifier)
    {
        ASSERT(!isStatic());
        if (isIdentifier)
            m_refCountAndFlags |= s_refCountFlagIsIdentifier;
        else
            m_refCountAndFlags &= ~s_refCountFlagIsIdentifier;
    }

    unsigned hash() const { if (!m_hash) m_hash = computeHash(m_data, m_length); return m_hash; }
    unsigned existingHash() const { ASSERT(m_hash); return m_hash; }
    static unsigned computeHash(const UChar* data, unsigned length) { return WTF::stringHash(data, length); }
    static unsigned computeHash(const char* data, unsigned length) { return WTF::stringHash(data, length); }
    static unsigned computeHash(const char* data) { return WTF::stringHash(data); }

    ALWAYS_INLINE void deref() { m_refCountAndFlags -= s_refCountIncrement; if (!(m_refCountAndFlags & (s_refCountMask | s_refCountFlagStatic))) delete this; }

    static UStringImpl* empty();

    static void copyChars(UChar* destination, const UChar* source, unsigned numCharacters)
    {
        if (numCharacters <= s_copyCharsInlineCutOff) {
            for (unsigned i = 0; i < numCharacters; ++i)
                destination[i] = source[i];
        } else
            memcpy(destination, source, numCharacters * sizeof(UChar));
    }

private:
    // This number must be at least 2 to avoid sharing empty, null as well as 1 character strings from SmallStrings.
    static const unsigned s_copyCharsInlineCutOff = 20;

    BufferOwnership bufferOwnership() const { return static_cast<BufferOwnership>(m_refCountAndFlags & s_refCountMaskBufferOwnership); }
    bool isStatic() const { return m_refCountAndFlags & s_refCountFlagStatic; }

    const UChar* m_data;
    union {
        void* m_buffer;
        UStringImpl* m_substringBuffer;
        SharedUChar* m_sharedBuffer;
    };
    mutable unsigned m_hash;
};

bool equal(const UStringImpl*, const UStringImpl*);

}

#endif
