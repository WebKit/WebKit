// -*- mode: c++; c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KXMLCORE_VECTOR_H
#define KXMLCORE_VECTOR_H

#include "Assertions.h"
#include <stdlib.h>
#include <utility>
#include "VectorTraits.h"

namespace KXMLCore {

    using std::min;
    using std::max;
    
    template <bool needsDestruction, typename T>
    class VectorDestructor;

    template<typename T>
    struct VectorDestructor<false, T>
    {
        static void destruct(T*, T*) {}
    };

    template<typename T>
    struct VectorDestructor<true, T>
    {
        static void destruct(T* begin, T* end) 
        {
            for (T* cur = begin; cur != end; ++cur)
                cur->~T();
        }
    };

    template <bool needsInitialization, bool canInitializeWithMemset, typename T>
    class VectorInitializer;

    template<bool ignore, typename T>
    struct VectorInitializer<false, ignore, T>
    {
        static void initialize(T*, T*) {}
    };

    template<typename T>
    struct VectorInitializer<true, false, T>
    {
        static void initialize(T* begin, T* end) 
        {
            for (T* cur = begin; cur != end; ++cur)
                new (cur) T;
        }
    };

    template<typename T>
    struct VectorInitializer<true, true, T>
    {
        static void initialize(T* begin, T* end) 
        {
            memset(begin, 0, reinterpret_cast<char *>(end) - reinterpret_cast<char *>(begin));
        }
    };

    template <bool canMoveWithMemcpy, typename T>
    class VectorMover;

    template<typename T>
    struct VectorMover<false, T>
    {
        static void move(const T* src, const T* srcEnd, T* dst) 
        {
            while (src != srcEnd) {
                new (dst) T(*src);
                src->~T();
                ++dst;
                ++src;
            }
        }
    };

    template<typename T>
    struct VectorMover<true, T>
    {
        static void move(const T* src, const T* srcEnd, T* dst) 
        {
            memcpy(dst, src, reinterpret_cast<const char *>(srcEnd) - reinterpret_cast<const char *>(src));
        }
    };

    template <bool canCopyWithMemcpy, typename T>
    class VectorCopier;

    template<typename T>
    struct VectorCopier<false, T>
    {
        static void uninitializedCopy(const T* src, const T* srcEnd, T* dst) 
        {
            while (src != srcEnd) {
                new (dst) T(*src);
                ++dst;
                ++src;
            }
        }
    };

    template<typename T>
    struct VectorCopier<true, T>
    {
        static void uninitializedCopy(const T* src, const T* srcEnd, T* dst) 
        {
            memcpy(dst, src, reinterpret_cast<const char *>(srcEnd) - reinterpret_cast<const char *>(src));
        }
    };

    template <bool canFillWithMemset, typename T>
    class VectorFiller;

    template<typename T>
    struct VectorFiller<false, T>
    {
        static void uninitializedFill(T* dst, T* dstEnd, const T& val) 
        {
            while (dst != dstEnd) {
                new (dst) T(val);
                ++dst;
            }
        }
    };

    template<typename T>
    struct VectorFiller<true, T>
    {
        static void uninitializedFill(T* dst, T* dstEnd, const T& val) 
        {
            ASSERT(sizeof(T) == sizeof(char));
            memset(dst, val, dstEnd - dst);
        }
    };
    
    template<typename T>
    struct VectorTypeOperations
    {
        static void destruct(T* begin, T* end)
        {
            VectorDestructor<VectorTraits<T>::needsDestruction, T>::destruct(begin, end);
        }

        static void initialize(T* begin, T* end)
        {
            VectorInitializer<VectorTraits<T>::needsInitialization, VectorTraits<T>::canInitializeWithMemset, T>::initialize(begin, end);
        }

        static void move(const T* src, const T* srcEnd, T* dst)
        {
            VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::move(src, srcEnd, dst);
        }

        static void uninitializedCopy(const T* src, const T* srcEnd, T* dst)
        {
            VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(src, srcEnd, dst);
        }

        static void uninitializedFill(T* dst, T* dstEnd, const T& val)
        {
            VectorFiller<VectorTraits<T>::canFillWithMemset, T>::uninitializedFill(dst, dstEnd, val);
        }
    };

    template<typename T, size_t inlineCapacity>
    class VectorBuffer;

    template<typename T>
    class VectorBuffer<T, 0> 
    {
    public:
        VectorBuffer()
            : m_capacity(0)
            , m_buffer(0)
        {
        }
        
        VectorBuffer(size_t capacity)
            : m_capacity(0)
        {
            allocateBuffer(capacity);
        }

        ~VectorBuffer()
        {
            deallocateBuffer(m_buffer);
        }
        
        void deallocateBuffer(T* buffer)
        {
            fastFree(buffer);
        }
        
        void allocateBuffer(size_t newCapacity)
        {
            ASSERT(newCapacity >= m_capacity);
            m_capacity = newCapacity;
            m_buffer = reinterpret_cast<T*>(fastMalloc(newCapacity * sizeof(T)));
        }

        T* buffer() { return m_buffer; }
        const T* buffer() const { return m_buffer; }
        size_t capacity() const { return m_capacity; }

    protected:
        VectorBuffer(T* buffer, size_t capacity)
            : m_capacity(capacity)
            , m_buffer(buffer)
        {
        }

        size_t m_capacity;
        T *m_buffer;
    };

    template<typename T, size_t inlineCapacity>
    class VectorBuffer : public VectorBuffer<T, 0> {
    private:
        typedef VectorBuffer<T, 0> BaseBuffer;
    public:
        VectorBuffer() 
            : BaseBuffer(inlineBuffer(), inlineCapacity)
        {
        }

        VectorBuffer(size_t capacity)
            : BaseBuffer(inlineBuffer(), inlineCapacity)
        {
            if (capacity > inlineCapacity)
                BaseBuffer::allocateBuffer(capacity);
        }

        ~VectorBuffer()
        {
            if (BaseBuffer::buffer() == inlineBuffer())
                BaseBuffer::m_buffer = 0;
        }

        void deallocateBuffer(T* buffer)
        {
            if (buffer != inlineBuffer())
                BaseBuffer::deallocateBuffer(buffer);
        }

    private:
        static const size_t m_inlineBufferSize = inlineCapacity * sizeof(T);
        T *inlineBuffer() { return reinterpret_cast<T *>(&m_inlineBuffer); }
        char m_inlineBuffer[m_inlineBufferSize];
    };

    template<typename T, size_t inlineCapacity = 0>
    class Vector {
    private:
        typedef VectorBuffer<T, inlineCapacity> Impl;
        typedef VectorTypeOperations<T> TypeOperations;

    public:
        typedef T* iterator;
        typedef const T* const_iterator;

        Vector() 
            : m_size(0)
        {
        }
        
        explicit Vector(size_t size) 
            : m_size(0)
        {
            resize(size);
        }

        ~Vector()
        {
            clear();
        }

        Vector(const Vector&);
        template<size_t otherCapacity> 
        Vector(const Vector<T, otherCapacity>&);

        template<size_t otherCapacity> 
        Vector& operator=(const Vector<T, otherCapacity>&);

        size_t size() const { return m_size; }
        size_t capacity() const { return m_impl.capacity(); }
        bool isEmpty() const { return !size(); }

        T& at(size_t i) 
        { 
            ASSERT(i < size());
            return m_impl.buffer()[i]; 
        }
        const T& at(size_t i) const 
        {
            ASSERT(i < size());
            return m_impl.buffer()[i]; 
        }

        T& operator[](unsigned long i) { return at(i); }
        const T& operator[](unsigned long i) const { return at(i); }
        T& operator[](int i) { return at(i); }
        const T& operator[](int i) const { return at(i); }
        T& operator[](unsigned i) { return at(i); }
        const T& operator[](unsigned i) const { return at(i); }

        T* data() { return m_impl.buffer(); }
        const T* data() const { return m_impl.buffer(); }
        operator T*() { return data(); }
        operator const T*() const { return data(); }

        iterator begin() { return data(); }
        iterator end() { return begin() + m_size; }
        const_iterator begin() const { return data(); }
        const_iterator end() const { return begin() + m_size; }
        
        T& first() { return at(0); }
        const T& first() const { return at(0); }
        T& last() { return at(size() - 1); }
        const T& last() const { return at(size() - 1); }

        void resize(size_t size);
        void reserveCapacity(size_t newCapacity);

        void clear() { resize(0); }

        template<typename U>
        void append(const U& u);

        void removeLast() 
        {
            ASSERT(!isEmpty());
            resize(size() - 1); 
        }

        Vector(size_t size, const T& val)
            : m_size(size)
            , m_impl(size)
        {
            TypeOperations::uninitializedFill(begin(), end(), val);
        }

        void fill(const T& val, size_t size);
        void fill(const T& val) { fill(val, size()); }

    private:
        void expandCapacity(size_t newMinCapacity);

        Vector& operator=(const Vector&) { return *this; }

        size_t m_size;
        Impl m_impl;
    };

    template<typename T, size_t inlineCapacity>
    Vector<T, inlineCapacity>::Vector(const Vector& other)
        : m_size(other.size())
        , m_impl(other.capacity())
    {
        TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
    }

    template<typename T, size_t inlineCapacity>
    template<size_t otherCapacity> 
    Vector<T, inlineCapacity>::Vector(const Vector<T, otherCapacity>& other)
        : m_size(other.size())
        , m_impl(other.capacity())
    {
        TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
    }

    template<typename T, size_t inlineCapacity>
    template<size_t otherCapacity> 
    Vector<T, inlineCapacity>& Vector<T, inlineCapacity>::operator=(const Vector<T, otherCapacity>& other)
    {
        if (&other == this)
            return *this;
        
        if (size() > other.size())
            resize(other.size());
        else if (other.size() > capacity()) {
            clear();
            reserveCapacity(other.size());
        }
        
        std::copy(other.begin(), other.begin() + size(), begin());
        TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
        m_size = other.size();

        return *this;
    }

    template<typename T, size_t inlineCapacity>
    void Vector<T, inlineCapacity>::fill(const T& val, size_t newSize)
    {
        if (size() > newSize)
            resize(newSize);
        else if (newSize > capacity()) {
            clear();
            reserveCapacity(newSize);
        }
        
        std::fill(begin(), end(), val);
        TypeOperations::uninitializedFill(end(), begin() + newSize, val);
        m_size = newSize;
    }

    template<typename T, size_t inlineCapacity>
    void Vector<T, inlineCapacity>::expandCapacity(size_t newMinCapacity)
    {
        reserveCapacity(max(newMinCapacity, max(static_cast<size_t>(16), capacity() + capacity() / 4 + 1)));
    }
    
    template<typename T, size_t inlineCapacity>
    void Vector<T, inlineCapacity>::resize(size_t size)
    {
        if (size <= m_size)
            TypeOperations::destruct(begin() + size, end());
        else {
            if (size > capacity())
                expandCapacity(size);
            TypeOperations::initialize(end(), begin() + size);
        }
        
        m_size = size;
    }

    template<typename T, size_t inlineCapacity>
    void Vector<T, inlineCapacity>::reserveCapacity(size_t newCapacity)
    {
        if (newCapacity < capacity())
            return;
        T* oldBuffer = begin();
        T* oldEnd = end();
        m_impl.allocateBuffer(newCapacity);
        TypeOperations::move(oldBuffer, oldEnd, begin());
        m_impl.deallocateBuffer(oldBuffer);
    }

    // templatizing this is better than just letting the conversion happen implicitly,
    // because for instance it allows a PassRefPtr to be appended to a RefPtr vector
    // without refcount thrash.
    template<typename T, size_t inlineCapacity>
    template<typename U>
    inline void Vector<T, inlineCapacity>::append(const U& val)
    {
        if (size() == capacity())
            expandCapacity(size() + 1);
        
        new (end()) T(val);
        ++m_size;
    }
    
    template<typename T, size_t inlineCapacity>
    void deleteAllValues(Vector<T, inlineCapacity>& collection)
    {
        typedef Vector<T, inlineCapacity> Vec;
        typename Vec::iterator end = collection.end();
        for (typename Vec::iterator it = collection.begin(); it != collection.end(); ++it)
            delete *it;
    }

} // namespace KXMLCore

using KXMLCore::Vector;

#endif // KXMLCORE_VECTOR_H
