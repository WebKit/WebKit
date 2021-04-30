/*
 * Copyright (C) 2011-2019 Apple Inc. All rights reserved.
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

#include <wtf/FastMalloc.h>
#include <wtf/RawPtrTraits.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

// This implements a reference counted array for POD** values, which is optimized for:
// - An empty array only uses one word.
// - A copy of the array only uses one word (i.e. assignment means aliasing).
// - The vector can't grow beyond 2^32-1 elements.
// - In all other regards this has similar space usage to a Vector.
//
// ** This could be modified to support non-POD values quite easily. It just
//    hasn't been, so far, because there has been no need. Moreover, even now,
//    it's used for things that aren't quite POD according to the official
//    defintion, such as JSC::Instruction.

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(RefCountedArray);

template<typename T, typename PtrTraits = RawPtrTraits<T>>
class RefCountedArray {
    enum CommonCopyConstructorTag { CommonCopyConstructor };

public:
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    RefCountedArray() = default;
    
    RefCountedArray(const RefCountedArray& other)
        : RefCountedArray(CommonCopyConstructor, other)
    { }

    template<typename OtherTraits>
    RefCountedArray(const RefCountedArray<T, OtherTraits>& other)
        : RefCountedArray(CommonCopyConstructor, other)
    { }

    RefCountedArray(RefCountedArray&& other)
        : m_data(PtrTraits::exchange(other.m_data, nullptr))
    { }

    explicit RefCountedArray(size_t size)
    {
        if (!size) {
            // NOTE: JSC's LowLevelInterpreter relies on this being nullptr when the size is zero.
            PtrTraits::exchange(m_data, nullptr);
            return;
        }

        m_data = allocateUninitializedData(size);
        VectorTypeOperations<T>::initializeIfNonPOD(begin(), end());
    }

    template<typename OtherTraits = PtrTraits>
    RefCountedArray<T, OtherTraits> clone() const
    {
        RefCountedArray<T, OtherTraits> result(size());
        const T* data = this->data();
        T* resultData = result.data();
        for (unsigned i = size(); i--;)
            resultData[i] = data[i];
        return result;
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    explicit RefCountedArray(const Vector<T, inlineCapacity, OverflowHandler>& other)
    {
        if (other.isEmpty()) {
            PtrTraits::exchange(m_data, nullptr);
            return;
        }

        T* data = allocateUninitializedData(other.size());
        m_data = data;
        VectorTypeOperations<T>::uninitializedCopy(other.begin(), other.end(), data);
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    explicit RefCountedArray(Vector<T, inlineCapacity, OverflowHandler>&& other)
    {
        Vector<T, inlineCapacity, OverflowHandler> vector(WTFMove(other));
        if (vector.isEmpty()) {
            PtrTraits::exchange(m_data, nullptr);
            return;
        }

        T* data = allocateUninitializedData(vector.size());
        m_data = data;
        for (unsigned index = 0; index < vector.size(); ++index)
            new (data + index) T(WTFMove(vector[index]));
    }
    
    template<typename OtherTraits = PtrTraits>
    RefCountedArray& operator=(const RefCountedArray<T, OtherTraits>& other)
    {
        return assign<OtherTraits>(other);
    }

    RefCountedArray& operator=(const RefCountedArray& other)
    {
        return assign<PtrTraits>(other);
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    RefCountedArray& operator=(const Vector<T, inlineCapacity, OverflowHandler>& other)
    {
        T* oldData = data();
        if (other.isEmpty())
            PtrTraits::exchange(m_data, nullptr);
        else {
            T* data = allocateUninitializedData(other.size());
            m_data = data;
            VectorTypeOperations<T>::uninitializedCopy(other.begin(), other.end(), data);
        }
        if (!oldData)
            return *this;
        unsigned refCount = Header::fromPayload(oldData)->refCount - 1;
        if (refCount) {
            Header::fromPayload(oldData)->refCount = refCount;
            return *this;
        }
        VectorTypeOperations<T>::destruct(oldData, oldData + Header::fromPayload(oldData)->length);

        RefCountedArrayMalloc::free(Header::fromPayload(oldData));
        return *this;
    }

    template<size_t inlineCapacity, typename OverflowHandler>
    RefCountedArray& operator=(Vector<T, inlineCapacity, OverflowHandler>&& other)
    {
        Vector<T, inlineCapacity, OverflowHandler> vector(WTFMove(other));
        T* oldData = data();
        if (vector.isEmpty())
            PtrTraits::exchange(m_data, nullptr);
        else {
            T* data = allocateUninitializedData(vector.size());
            m_data = data;
            for (unsigned index = 0; index < vector.size(); ++index)
                new (data + index) T(WTFMove(vector[index]));
        }
        if (!oldData)
            return *this;
        unsigned refCount = Header::fromPayload(oldData)->refCount - 1;
        if (refCount) {
            Header::fromPayload(oldData)->refCount = refCount;
            return *this;
        }
        VectorTypeOperations<T>::destruct(oldData, oldData + Header::fromPayload(oldData)->length);

        RefCountedArrayMalloc::free(Header::fromPayload(oldData));
        return *this;
    }

    ~RefCountedArray()
    {
        if (!m_data)
            return;
        T* data = this->data();
        unsigned refCount = Header::fromPayload(data)->refCount - 1;
        if (refCount) {
            Header::fromPayload(data)->refCount = refCount;
            return;
        }
        VectorTypeOperations<T>::destruct(begin(), end());
        RefCountedArrayMalloc::free(Header::fromPayload(data));
    }
    
    unsigned refCount() const
    {
        if (!m_data)
            return 0;
        return Header::fromPayload(data())->refCount;
    }
    
    size_t size() const
    {
        if (!m_data)
            return 0;
        return Header::fromPayload(data())->length;
    }

    bool isEmpty() const { return size() == 0; }
    
    size_t byteSize() const { return size() * sizeof(T); }
    
    T* data() { return PtrTraits::unwrap(m_data); }
    iterator begin() { return data(); }
    iterator end()
    {
        if (!m_data)
            return 0;
        T* data = this->data();
        return data + Header::fromPayload(data)->length;
    }
    
    const T* data() const { return const_cast<RefCountedArray*>(this)->data(); }
    const_iterator begin() const { return const_cast<RefCountedArray*>(this)->begin(); }
    const_iterator end() const { return const_cast<RefCountedArray*>(this)->end(); }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    
    T& at(size_t i)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(i < size());
        return begin()[i];
    }
    
    const T& at(size_t i) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(i < size());
        return begin()[i];
    }
    
    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }

    T& first() { return (*this)[0]; }
    const T& first() const { return (*this)[0]; }
    T& last() { return (*this)[size() - 1]; }
    const T& last() const { return (*this)[size() - 1]; }

    void fill(const T& val)
    {
        std::fill(begin(), end(), val);
    }

    void swap(RefCountedArray& other)
    {
        PtrTraits::swap(m_data, other.m_data);
    }

    template<typename OtherTraits = PtrTraits>
    bool operator==(const RefCountedArray<T, OtherTraits>& other) const
    {
        T* data = const_cast<T*>(this->data());
        T* otherData = const_cast<T*>(other.data());
        if (data == otherData)
            return true;
        if (!data || !otherData)
            return false;
        unsigned length = Header::fromPayload(data)->length;
        if (length != Header::fromPayload(otherData)->length)
            return false;
        for (unsigned i = 0; i < length; ++i) {
            if (data[i] != otherData[i])
                return false;
        }
        return true;
    }

    bool operator==(const RefCountedArray& other) const { return this->operator==<PtrTraits>(other); }
    
private:
    static T* allocateUninitializedData(unsigned length)
    {
        T* data = (static_cast<Header*>(RefCountedArrayMalloc::malloc(Header::size() + sizeof(T) * length)))->payload();
        Header::fromPayload(data)->refCount = 1;
        Header::fromPayload(data)->length = length;
        ASSERT(Header::fromPayload(data)->length == length);
        return data;
    }

    template<typename OtherTraits = PtrTraits>
    RefCountedArray& assign(const RefCountedArray<T, OtherTraits>& other)
    {
        T* oldData = data();
        T* otherData = const_cast<T*>(other.data());
        if (otherData)
            Header::fromPayload(otherData)->refCount++;
        m_data = otherData;

        if (!oldData)
            return *this;
        unsigned refCount = Header::fromPayload(oldData)->refCount - 1;
        if (refCount) {
            Header::fromPayload(oldData)->refCount = refCount;
            return *this;
        }
        VectorTypeOperations<T>::destruct(oldData, oldData + Header::fromPayload(oldData)->length);

        RefCountedArrayMalloc::free(Header::fromPayload(oldData));
        return *this;
    }

    struct Header {
        unsigned refCount;
        unsigned length;
        
        static constexpr size_t size()
        {
            return (sizeof(Header) + 7) & ~7;
        }

        static ptrdiff_t offsetOfLength() { return OBJECT_OFFSETOF(Header, length); }
        
        T* payload()
        {
            char* result = reinterpret_cast<char*>(this) + size();
            ASSERT(!(bitwise_cast<uintptr_t>(result) & 7));
            return reinterpret_cast_ptr<T*>(result);
        }
        
        static Header* fromPayload(T* payload)
        {
            return reinterpret_cast_ptr<Header*>(reinterpret_cast<char*>(payload) - size());
        }

        static const Header* fromPayload(const T* payload)
        {
            return fromPayload(const_cast<T*>(payload));
        }
    };

    template<typename OtherTraits>
    RefCountedArray(CommonCopyConstructorTag, const RefCountedArray<T, OtherTraits>& other)
        : m_data(const_cast<T*>(other.data()))
    {
        if (m_data)
            Header::fromPayload(data())->refCount++;
    }

    friend class JSC::LLIntOffsetsExtractor;
    typename PtrTraits::StorageType m_data { nullptr };
};

} // namespace WTF

using WTF::RefCountedArray;
