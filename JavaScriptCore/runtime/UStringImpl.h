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

#include <wtf/CrossThreadRefCounted.h>
#include <wtf/OwnFastMallocPtr.h>
#include <wtf/PossiblyNull.h>
#include <wtf/unicode/Unicode.h>
#include <limits>

namespace JSC {

class IdentifierTable;
  
typedef CrossThreadRefCounted<OwnFastMallocPtr<UChar> > SharedUChar;

class UntypedPtrAndBitfield {
public:
    UntypedPtrAndBitfield() {}

    UntypedPtrAndBitfield(void* ptrValue, uintptr_t bitValue)
        : m_value(reinterpret_cast<uintptr_t>(ptrValue) | bitValue)
    {
        ASSERT(ptrValue == asPtr<void*>());
        ASSERT((*this & ~s_alignmentMask) == bitValue);
    }

    template<typename T>
    T asPtr() const { return reinterpret_cast<T>(m_value & s_alignmentMask); }

    UntypedPtrAndBitfield& operator&=(uintptr_t bits)
    {
        m_value &= bits | s_alignmentMask;
        return *this;
    }

    UntypedPtrAndBitfield& operator|=(uintptr_t bits)
    {
        m_value |= bits & ~s_alignmentMask;
        return *this;
    }

    uintptr_t operator&(uintptr_t mask) const
    {
        return m_value & mask & ~s_alignmentMask;
    }

private:
    static const uintptr_t s_alignmentMask = ~0x7u;
    uintptr_t m_value;
};

class UStringImpl : Noncopyable {
public:
    static PassRefPtr<UStringImpl> create(UChar* buffer, int length)
    {
        return adoptRef(new UStringImpl(buffer, length, BufferOwned));
    }

    static PassRefPtr<UStringImpl> createCopying(const UChar* buffer, int length)
    {
        UChar* newBuffer;
        if (!UStringImpl::allocChars(length).getValue(newBuffer))
            return &null();
        copyChars(newBuffer, buffer, length);
        return adoptRef(new UStringImpl(newBuffer, length, BufferOwned));
    }

    static PassRefPtr<UStringImpl> create(PassRefPtr<UStringImpl> rep, int offset, int length)
    {
        ASSERT(rep);
        rep->checkConsistency();
        return adoptRef(new UStringImpl(rep->m_data + offset, length, rep->bufferOwnerString()));
    }

    static PassRefPtr<UStringImpl> create(PassRefPtr<SharedUChar> sharedBuffer, UChar* buffer, int length)
    {
        return adoptRef(new UStringImpl(buffer, length, sharedBuffer));
    }

    static PassRefPtr<UStringImpl> createUninitialized(unsigned length, UChar*& output)
    {
        ASSERT(length);
        if (length > ((std::numeric_limits<size_t>::max() - sizeof(UStringImpl)) / sizeof(UChar)))
            return 0;

        UStringImpl* resultImpl;
        if (!tryFastMalloc(sizeof(UChar) * length + sizeof(UStringImpl)).getValue(resultImpl))
            return 0;

        output = reinterpret_cast<UChar*>(resultImpl + 1);
        return adoptRef(new(resultImpl) UStringImpl(output, length, BufferInternal));
    }

    SharedUChar* sharedBuffer();
    UChar* data() const { return m_data; }
    int size() const { return m_length; }
    size_t cost()
    {
        UStringImpl* base = bufferOwnerString();
        if (m_dataBuffer & s_reportedCostBit)
            return 0;
        m_dataBuffer |= s_reportedCostBit;
        return base->m_length;
    }
    unsigned hash() const { if (!m_hash) m_hash = computeHash(data(), m_length); return m_hash; }
    unsigned computedHash() const { ASSERT(m_hash); return m_hash; } // fast path for Identifiers
    void setHash(unsigned hash) { ASSERT(hash == computeHash(data(), m_length)); m_hash = hash; } // fast path for Identifiers
    IdentifierTable* identifierTable() const { return m_identifierTable; }
    void setIdentifierTable(IdentifierTable* table) { ASSERT(!isStatic()); m_identifierTable = table; }

    UStringImpl* ref() { m_refCount += s_refCountIncrement; return this; }
    ALWAYS_INLINE void deref() { if (!(m_refCount -= s_refCountIncrement)) destroy(); }

    static WTF::PossiblyNull<UChar*> allocChars(size_t length)
    {
        ASSERT(length);
        if (length > std::numeric_limits<size_t>::max() / sizeof(UChar))
            return 0;
        return tryFastMalloc(sizeof(UChar) * length);
    }

    static void copyChars(UChar* destination, const UChar* source, unsigned numCharacters)
    {
        if (numCharacters <= s_copyCharsInlineCutOff) {
            for (unsigned i = 0; i < numCharacters; ++i)
                destination[i] = source[i];
        } else
            memcpy(destination, source, numCharacters * sizeof(UChar));
    }

    static unsigned computeHash(const UChar*, int length);
    static unsigned computeHash(const char*, int length);
    static unsigned computeHash(const char* s) { return computeHash(s, strlen(s)); }

    static UStringImpl& null() { return *s_null; }
    static UStringImpl& empty() { return *s_empty; }

    ALWAYS_INLINE void checkConsistency() const
    {
        // There is no recursion of substrings.
        ASSERT(bufferOwnerString()->bufferOwnership() != BufferSubstring);
        // Static strings cannot be put in identifier tables, because they are globally shared.
        ASSERT(!isStatic() || !identifierTable());
    }

private:
    enum BufferOwnership {
        BufferInternal,
        BufferOwned,
        BufferSubstring,
        BufferShared,
    };

    // For SmallStringStorage, which allocates an array and uses an in-place new.
    UStringImpl() { }

    // Used to construct normal strings with an internal or external buffer.
    UStringImpl(UChar* data, int length, BufferOwnership ownership)
        : m_data(data)
        , m_length(length)
        , m_refCount(s_refCountIncrement)
        , m_hash(0)
        , m_identifierTable(0)
        , m_dataBuffer(0, ownership)
    {
        ASSERT((ownership == BufferInternal) || (ownership == BufferOwned));
        checkConsistency();
    }

    // Used to construct static strings, which have an special refCount that can never hit zero.
    // This means that the static string will never be destroyed, which is important because
    // static strings will be shared across threads & ref-counted in a non-threadsafe manner.
    enum StaticStringConstructType { ConstructStaticString };
    UStringImpl(UChar* data, int length, StaticStringConstructType)
        : m_data(data)
        , m_length(length)
        , m_refCount(s_staticRefCountInitialValue)
        , m_hash(0)
        , m_identifierTable(0)
        , m_dataBuffer(0, BufferOwned)
    {
        checkConsistency();
    }

    // Used to create new strings that are a substring of an existing string.
    UStringImpl(UChar* data, int length, PassRefPtr<UStringImpl> base)
        : m_data(data)
        , m_length(length)
        , m_refCount(s_refCountIncrement)
        , m_hash(0)
        , m_identifierTable(0)
        , m_dataBuffer(base.releaseRef(), BufferSubstring)
    {
        checkConsistency();
    }

    // Used to construct new strings sharing an existing shared buffer.
    UStringImpl(UChar* data, int length, PassRefPtr<SharedUChar> sharedBuffer)
        : m_data(data)
        , m_length(length)
        , m_refCount(s_refCountIncrement)
        , m_hash(0)
        , m_identifierTable(0)
        , m_dataBuffer(sharedBuffer.releaseRef(), BufferShared)
    {
        checkConsistency();
    }

    void* operator new(size_t size) { return fastMalloc(size); }
    void* operator new(size_t, void* inPlace) { return inPlace; }

    // This number must be at least 2 to avoid sharing empty, null as well as 1 character strings from SmallStrings.
    static const int s_minLengthToShare = 10;
    static const unsigned s_copyCharsInlineCutOff = 20;
    static const uintptr_t s_bufferOwnershipMask = 3;
    static const uintptr_t s_reportedCostBit = 4;
    // We initialize and increment/decrement the refCount for all normal (non-static) strings by the value 2.
    // We initialize static strings with an odd number (specifically, 1), such that the refCount cannot reach zero.
    static const int s_refCountIncrement = 2;
    static const int s_staticRefCountInitialValue = 1;

    void destroy();
    UStringImpl* bufferOwnerString() { return (bufferOwnership() == BufferSubstring) ? m_dataBuffer.asPtr<UStringImpl*>() :  this; }
    const UStringImpl* bufferOwnerString() const { return (bufferOwnership() == BufferSubstring) ? m_dataBuffer.asPtr<UStringImpl*>() :  this; }
    SharedUChar* baseSharedBuffer();
    unsigned bufferOwnership() const { return m_dataBuffer & s_bufferOwnershipMask; }
    bool isStatic() const { return m_refCount & 1; }

    // unshared data
    UChar* m_data;
    int m_length;
    unsigned m_refCount;
    mutable unsigned m_hash;
    IdentifierTable* m_identifierTable;
    UntypedPtrAndBitfield m_dataBuffer;

    JS_EXPORTDATA static UStringImpl* s_null;
    JS_EXPORTDATA static UStringImpl* s_empty;

    friend class JIT;
    friend class SmallStringsStorage;
    friend void initializeUString();
};

bool equal(const UStringImpl*, const UStringImpl*);

}

#endif
