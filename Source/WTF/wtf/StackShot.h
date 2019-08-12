/*
 * Copyright (C) 2017 Apple Inc.  All rights reserved.
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

#pragma once

#include <wtf/Assertions.h>
#include <wtf/HashTraits.h>
#include <wtf/UniqueArray.h>

namespace WTF {

class StackShot {
    WTF_MAKE_FAST_ALLOCATED;
public:
    StackShot() { }
    
    ALWAYS_INLINE StackShot(size_t size)
        : m_size(size)
    {
        if (size) {
            m_array = makeUniqueArray<void*>(size);
            int intSize = size;
            WTFGetBacktrace(m_array.get(), &intSize);
            RELEASE_ASSERT(static_cast<size_t>(intSize) <= size);
            m_size = intSize;
            if (!m_size)
                m_array = nullptr;
        }
    }
    
    StackShot(WTF::HashTableDeletedValueType)
        : m_array(deletedValueArray())
        , m_size(0)
    {
    }
    
    StackShot& operator=(const StackShot& other)
    {
        auto newArray = makeUniqueArray<void*>(other.m_size);
        for (size_t i = other.m_size; i--;)
            newArray[i] = other.m_array[i];
        m_size = other.m_size;
        m_array = WTFMove(newArray);
        return *this;
    }
    
    StackShot(const StackShot& other)
    {
        *this = other;
    }
    
    void** array() const { return m_array.get(); }
    size_t size() const { return m_size; }
    
    explicit operator bool() const { return !!m_array; }
    
    bool operator==(const StackShot& other) const
    {
        if (m_size != other.m_size)
            return false;
        
        for (size_t i = m_size; i--;) {
            if (m_array[i] != other.m_array[i])
                return false;
        }
        
        return true;
    }
    
    unsigned hash() const
    {
        unsigned result = m_size;
        
        for (size_t i = m_size; i--;)
            result ^= PtrHash<void*>::hash(m_array[i]);
        
        return result;
    }
    
    bool isHashTableDeletedValue() const
    {
        return !m_size && m_array.get() == deletedValueArray();
    }
    
    // Make Spectrum<> happy.
    bool operator>(const StackShot&) const { return false; }
    
private:
    static void** deletedValueArray() { return bitwise_cast<void**>(static_cast<uintptr_t>(1)); }

    UniqueArray<void*> m_array;
    size_t m_size { 0 };
};

struct StackShotHash {
    static unsigned hash(const StackShot& shot) { return shot.hash(); }
    static bool equal(const StackShot& a, const StackShot& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<StackShot> {
    typedef StackShotHash Hash;
};

template<> struct HashTraits<StackShot> : SimpleClassHashTraits<StackShot> { };

} // namespace WTF

