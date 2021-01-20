/*
 *  Copyright (C) 2005-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <initializer_list>
#include <limits>
#include <string.h>
#include <type_traits>
#include <utility>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FailureAction.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/MallocPtr.h>
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/NotFound.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ValueCheck.h>
#include <wtf/VectorTraits.h>

#if ASAN_ENABLED
extern "C" void __sanitizer_annotate_contiguous_container(const void* begin, const void* end, const void* old_mid, const void* new_mid);
#endif

namespace JSC {
class LLIntOffsetsExtractor;
}

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Vector);

template <bool needsDestruction, typename T>
struct VectorDestructor;

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
struct VectorInitializer;

template<bool canInitializeWithMemset, typename T>
struct VectorInitializer<false, canInitializeWithMemset, T>
{
    static void initializeIfNonPOD(T*, T*) { }

    static void initialize(T* begin, T* end)
    {
        VectorInitializer<true, canInitializeWithMemset, T>::initialize(begin, end);
    }
};

template<typename T>
struct VectorInitializer<true, false, T>
{
    static void initializeIfNonPOD(T* begin, T* end)
    {
        for (T* cur = begin; cur != end; ++cur)
            new (NotNull, cur) T();
    }

    static void initialize(T* begin, T* end)
    {
        initializeIfNonPOD(begin, end);
    }
};

template<typename T>
struct VectorInitializer<true, true, T>
{
    static void initializeIfNonPOD(T* begin, T* end)
    {
        memset(static_cast<void*>(begin), 0, reinterpret_cast<char*>(end) - reinterpret_cast<char*>(begin));
    }

    static void initialize(T* begin, T* end)
    {
        initializeIfNonPOD(begin, end);
    }
};

template <bool canMoveWithMemcpy, typename T>
struct VectorMover;

template<typename T>
struct VectorMover<false, T>
{
    static void move(T* src, T* srcEnd, T* dst)
    {
        while (src != srcEnd) {
            new (NotNull, dst) T(WTFMove(*src));
            src->~T();
            ++dst;
            ++src;
        }
    }
    static void moveOverlapping(T* src, T* srcEnd, T* dst)
    {
        if (src > dst)
            move(src, srcEnd, dst);
        else {
            T* dstEnd = dst + (srcEnd - src);
            while (src != srcEnd) {
                --srcEnd;
                --dstEnd;
                new (NotNull, dstEnd) T(WTFMove(*srcEnd));
                srcEnd->~T();
            }
        }
    }
};

template<typename T>
struct VectorMover<true, T>
{
    static void move(const T* src, const T* srcEnd, T* dst) 
    {
        memcpy(static_cast<void*>(dst), static_cast<void*>(const_cast<T*>(src)), reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
    }
    static void moveOverlapping(const T* src, const T* srcEnd, T* dst) 
    {
        memmove(static_cast<void*>(dst), static_cast<void*>(const_cast<T*>(src)), reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
    }
};

template <bool canCopyWithMemcpy, typename T>
struct VectorCopier;

template<typename T>
struct VectorCopier<false, T>
{
    template<typename U>
    static void uninitializedCopy(const T* src, const T* srcEnd, U* dst)
    {
        while (src != srcEnd) {
            new (NotNull, dst) U(*src);
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
        memcpy(static_cast<void*>(dst), static_cast<void*>(const_cast<T*>(src)), reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
    }
    template<typename U>
    static void uninitializedCopy(const T* src, const T* srcEnd, U* dst)
    {
        VectorCopier<false, T>::uninitializedCopy(src, srcEnd, dst);
    }
};

template <bool canFillWithMemset, typename T>
struct VectorFiller;

template<typename T>
struct VectorFiller<false, T>
{
    static void uninitializedFill(T* dst, T* dstEnd, const T& val) 
    {
        while (dst != dstEnd) {
            new (NotNull, dst) T(val);
            ++dst;
        }
    }
};

template<typename T>
struct VectorFiller<true, T>
{
    static void uninitializedFill(T* dst, T* dstEnd, const T& val) 
    {
        static_assert(sizeof(T) == 1, "Size of type T should be equal to one!");
        memset(dst, val, dstEnd - dst);
    }
};

template<bool canCompareWithMemcmp, typename T>
struct VectorComparer;

template<typename T>
struct VectorComparer<false, T>
{
    static bool compare(const T* a, const T* b, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
            if (!(a[i] == b[i]))
                return false;
        return true;
    }
};

template<typename T>
struct VectorComparer<true, T>
{
    static bool compare(const T* a, const T* b, size_t size)
    {
        return memcmp(a, b, sizeof(T) * size) == 0;
    }
};

template<typename T>
struct VectorTypeOperations
{
    static void destruct(T* begin, T* end)
    {
        VectorDestructor<!std::is_trivially_destructible<T>::value, T>::destruct(begin, end);
    }

    static void initializeIfNonPOD(T* begin, T* end)
    {
        VectorInitializer<VectorTraits<T>::needsInitialization, VectorTraits<T>::canInitializeWithMemset, T>::initializeIfNonPOD(begin, end);
    }

    static void initialize(T* begin, T* end)
    {
        VectorInitializer<VectorTraits<T>::needsInitialization, VectorTraits<T>::canInitializeWithMemset, T>::initialize(begin, end);
    }

    static void move(T* src, T* srcEnd, T* dst)
    {
        VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::move(src, srcEnd, dst);
    }

    static void moveOverlapping(T* src, T* srcEnd, T* dst)
    {
        VectorMover<VectorTraits<T>::canMoveWithMemcpy, T>::moveOverlapping(src, srcEnd, dst);
    }

    static void uninitializedCopy(const T* src, const T* srcEnd, T* dst)
    {
        VectorCopier<VectorTraits<T>::canCopyWithMemcpy, T>::uninitializedCopy(src, srcEnd, dst);
    }

    static void uninitializedFill(T* dst, T* dstEnd, const T& val)
    {
        VectorFiller<VectorTraits<T>::canFillWithMemset, T>::uninitializedFill(dst, dstEnd, val);
    }
    
    static bool compare(const T* a, const T* b, size_t size)
    {
        return VectorComparer<VectorTraits<T>::canCompareWithMemcmp, T>::compare(a, b, size);
    }
};

template<typename T, typename Malloc>
class VectorBufferBase {
    WTF_MAKE_NONCOPYABLE(VectorBufferBase);
public:
    template<FailureAction action>
    bool allocateBuffer(size_t newCapacity)
    {
        static_assert(action == FailureAction::Crash || action == FailureAction::Report);
        ASSERT(newCapacity);
        if (newCapacity > std::numeric_limits<unsigned>::max() / sizeof(T)) {
            if constexpr (action == FailureAction::Crash)
                CRASH();
            else
                return false;
        }

        size_t sizeToAllocate = newCapacity * sizeof(T);
        T* newBuffer = nullptr;
        if constexpr (action == FailureAction::Crash)
            newBuffer = static_cast<T*>(Malloc::malloc(sizeToAllocate));
        else {
            newBuffer = static_cast<T*>(Malloc::tryMalloc(sizeToAllocate));
            if (UNLIKELY(!newBuffer))
                return false;
        }
        m_capacity = sizeToAllocate / sizeof(T);
        m_buffer = newBuffer;
        return true;
    }

    ALWAYS_INLINE void allocateBuffer(size_t newCapacity) { allocateBuffer<FailureAction::Crash>(newCapacity); }
    ALWAYS_INLINE bool tryAllocateBuffer(size_t newCapacity) { return allocateBuffer<FailureAction::Report>(newCapacity); }

    bool shouldReallocateBuffer(size_t newCapacity) const
    {
        return VectorTraits<T>::canMoveWithMemcpy && m_capacity && newCapacity;
    }

    void reallocateBuffer(size_t newCapacity)
    {
        ASSERT(shouldReallocateBuffer(newCapacity));
        if (newCapacity > std::numeric_limits<size_t>::max() / sizeof(T))
            CRASH();
        size_t sizeToAllocate = newCapacity * sizeof(T);
        m_capacity = sizeToAllocate / sizeof(T);
        m_buffer = static_cast<T*>(Malloc::realloc(m_buffer, sizeToAllocate));
    }

    void deallocateBuffer(T* bufferToDeallocate)
    {
        if (!bufferToDeallocate)
            return;
        
        if (m_buffer == bufferToDeallocate) {
            m_buffer = nullptr;
            m_capacity = 0;
        }

        Malloc::free(bufferToDeallocate);
    }

    T* buffer() { return m_buffer; }
    const T* buffer() const { return m_buffer; }
    static ptrdiff_t bufferMemoryOffset() { return OBJECT_OFFSETOF(VectorBufferBase, m_buffer); }
    size_t capacity() const { return m_capacity; }

    MallocPtr<T, Malloc> releaseBuffer()
    {
        T* buffer = m_buffer;
        m_buffer = nullptr;
        m_capacity = 0;
        return adoptMallocPtr<T, Malloc>(buffer);
    }

protected:
    VectorBufferBase()
        : m_buffer(nullptr)
        , m_capacity(0)
        , m_size(0)
    {
    }

    VectorBufferBase(T* buffer, size_t capacity, size_t size)
        : m_buffer(buffer)
        , m_capacity(capacity)
        , m_size(size)
    {
    }

    ~VectorBufferBase()
    {
        // FIXME: It would be nice to find a way to ASSERT that m_buffer hasn't leaked here.
    }

    T* m_buffer;
    unsigned m_capacity;
    unsigned m_size; // Only used by the Vector subclass, but placed here to avoid padding the struct.
};

template<typename T, size_t inlineCapacity, typename Malloc = VectorMalloc> class VectorBuffer;

template<typename T, typename Malloc>
class VectorBuffer<T, 0, Malloc> : private VectorBufferBase<T, Malloc> {
private:
    typedef VectorBufferBase<T, Malloc> Base;
public:
    VectorBuffer()
    {
    }

    VectorBuffer(size_t capacity, size_t size = 0)
    {
        m_size = size;
        // Calling malloc(0) might take a lock and may actually do an
        // allocation on some systems.
        if (capacity)
            allocateBuffer(capacity);
    }

    ~VectorBuffer()
    {
        deallocateBuffer(buffer());
    }
    
    void swap(VectorBuffer<T, 0, Malloc>& other, size_t, size_t)
    {
        std::swap(m_buffer, other.m_buffer);
        std::swap(m_capacity, other.m_capacity);
    }
    
    void restoreInlineBufferIfNeeded() { }

#if ASAN_ENABLED
    void* endOfBuffer()
    {
        return buffer() + capacity();
    }
#endif

    using Base::allocateBuffer;
    using Base::tryAllocateBuffer;
    using Base::shouldReallocateBuffer;
    using Base::reallocateBuffer;
    using Base::deallocateBuffer;

    using Base::buffer;
    using Base::capacity;
    using Base::bufferMemoryOffset;

    using Base::releaseBuffer;

protected:
    using Base::m_size;

private:
    friend class JSC::LLIntOffsetsExtractor;
    using Base::m_buffer;
    using Base::m_capacity;
};

template<typename T, size_t inlineCapacity, typename Malloc>
class VectorBuffer : private VectorBufferBase<T, Malloc> {
    WTF_MAKE_NONCOPYABLE(VectorBuffer);
private:
    typedef VectorBufferBase<T, Malloc> Base;
public:
    VectorBuffer()
        : Base(inlineBuffer(), inlineCapacity, 0)
    {
    }

    VectorBuffer(size_t capacity, size_t size = 0)
        : Base(inlineBuffer(), inlineCapacity, size)
    {
        if (capacity > inlineCapacity)
            Base::allocateBuffer(capacity);
    }

    ~VectorBuffer()
    {
        deallocateBuffer(buffer());
    }

    template<FailureAction action>
    bool allocateBuffer(size_t newCapacity)
    {
        // FIXME: This should ASSERT(!m_buffer) to catch misuse/leaks.
        if (newCapacity > inlineCapacity)
            return Base::template allocateBuffer<action>(newCapacity);
        m_buffer = inlineBuffer();
        m_capacity = inlineCapacity;
        return true;
    }

    ALWAYS_INLINE void allocateBuffer(size_t newCapacity) { allocateBuffer<FailureAction::Crash>(newCapacity); }
    ALWAYS_INLINE bool tryAllocateBuffer(size_t newCapacity) { return allocateBuffer<FailureAction::Report>(newCapacity); }

    void deallocateBuffer(T* bufferToDeallocate)
    {
        if (bufferToDeallocate == inlineBuffer())
            return;
        Base::deallocateBuffer(bufferToDeallocate);
    }

    bool shouldReallocateBuffer(size_t newCapacity) const
    {
        // We cannot reallocate the inline buffer.
        return Base::shouldReallocateBuffer(newCapacity) && std::min(static_cast<size_t>(m_capacity), newCapacity) > inlineCapacity;
    }

    void reallocateBuffer(size_t newCapacity)
    {
        ASSERT(shouldReallocateBuffer(newCapacity));
        Base::reallocateBuffer(newCapacity);
    }

    void swap(VectorBuffer& other, size_t mySize, size_t otherSize)
    {
        if (buffer() == inlineBuffer() && other.buffer() == other.inlineBuffer()) {
            swapInlineBuffer(other, mySize, otherSize);
            std::swap(m_capacity, other.m_capacity);
        } else if (buffer() == inlineBuffer()) {
            m_buffer = other.m_buffer;
            other.m_buffer = other.inlineBuffer();
            swapInlineBuffer(other, mySize, 0);
            std::swap(m_capacity, other.m_capacity);
        } else if (other.buffer() == other.inlineBuffer()) {
            other.m_buffer = m_buffer;
            m_buffer = inlineBuffer();
            swapInlineBuffer(other, 0, otherSize);
            std::swap(m_capacity, other.m_capacity);
        } else {
            std::swap(m_buffer, other.m_buffer);
            std::swap(m_capacity, other.m_capacity);
        }
    }

    void restoreInlineBufferIfNeeded()
    {
        if (m_buffer)
            return;
        m_buffer = inlineBuffer();
        m_capacity = inlineCapacity;
    }

#if ASAN_ENABLED
    void* endOfBuffer()
    {
        ASSERT(buffer());

        IGNORE_GCC_WARNINGS_BEGIN("invalid-offsetof")
        static_assert((offsetof(VectorBuffer, m_inlineBuffer) + sizeof(m_inlineBuffer)) % 8 == 0, "Inline buffer end needs to be on 8 byte boundary for ASan annotations to work.");
        IGNORE_GCC_WARNINGS_END

        if (buffer() == inlineBuffer())
            return reinterpret_cast<char*>(m_inlineBuffer) + sizeof(m_inlineBuffer);

        return buffer() + capacity();
    }
#endif

    using Base::buffer;
    using Base::capacity;
    using Base::bufferMemoryOffset;

    MallocPtr<T, Malloc> releaseBuffer()
    {
        if (buffer() == inlineBuffer())
            return { };
        return Base::releaseBuffer();
    }

protected:
    using Base::m_size;

private:
    using Base::m_buffer;
    using Base::m_capacity;
    
    void swapInlineBuffer(VectorBuffer& other, size_t mySize, size_t otherSize)
    {
        // FIXME: We could make swap part of VectorTypeOperations
        // https://bugs.webkit.org/show_bug.cgi?id=128863
        swapInlineBuffers(inlineBuffer(), other.inlineBuffer(), mySize, otherSize);
    }
    
    static void swapInlineBuffers(T* left, T* right, size_t leftSize, size_t rightSize)
    {
        if (left == right)
            return;
        
        ASSERT(leftSize <= inlineCapacity);
        ASSERT(rightSize <= inlineCapacity);
        
        size_t swapBound = std::min(leftSize, rightSize);
        for (unsigned i = 0; i < swapBound; ++i)
            std::swap(left[i], right[i]);
        VectorTypeOperations<T>::move(left + swapBound, left + leftSize, right + swapBound);
        VectorTypeOperations<T>::move(right + swapBound, right + rightSize, left + swapBound);
    }

    T* inlineBuffer() { return reinterpret_cast_ptr<T*>(m_inlineBuffer); }
    const T* inlineBuffer() const { return reinterpret_cast_ptr<const T*>(m_inlineBuffer); }

#if ASAN_ENABLED
    // ASan needs the buffer to begin and end on 8-byte boundaries for annotations to work.
    // FIXME: Add a redzone before the buffer to catch off by one accesses. We don't need a guard after, because the buffer is the last member variable.
    static constexpr size_t asanInlineBufferAlignment = std::alignment_of<T>::value >= 8 ? std::alignment_of<T>::value : 8;
    static constexpr size_t asanAdjustedInlineCapacity = ((sizeof(T) * inlineCapacity + 7) & ~7) / sizeof(T);
    typename std::aligned_storage<sizeof(T), asanInlineBufferAlignment>::type m_inlineBuffer[asanAdjustedInlineCapacity];
#else
    typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type m_inlineBuffer[inlineCapacity];
#endif
};

struct UnsafeVectorOverflow {
    static NO_RETURN_DUE_TO_ASSERT void overflowed()
    {
        ASSERT_NOT_REACHED();
    }
};

// Template default values are in Forward.h.
template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
class Vector : private VectorBuffer<T, inlineCapacity, Malloc> {
    WTF_MAKE_FAST_ALLOCATED;
private:
    typedef VectorBuffer<T, inlineCapacity, Malloc> Base;
    typedef VectorTypeOperations<T> TypeOperations;
    friend class JSC::LLIntOffsetsExtractor;

public:
    typedef T ValueType;

    typedef T* iterator;
    typedef const T* const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    Vector()
    {
    }

    // Unlike in std::vector, this constructor does not initialize POD types.
    explicit Vector(size_t size)
        : Base(size, size)
    {
        asanSetInitialBufferSizeTo(size);

        if (begin())
            TypeOperations::initializeIfNonPOD(begin(), end());
    }

    Vector(size_t size, const T& val)
        : Base(size, size)
    {
        asanSetInitialBufferSizeTo(size);

        if (begin())
            TypeOperations::uninitializedFill(begin(), end(), val);
    }

    Vector(std::initializer_list<T> initializerList)
    {
        reserveInitialCapacity(initializerList.size());

        asanSetInitialBufferSizeTo(initializerList.size());

        for (const auto& element : initializerList)
            uncheckedAppend(element);
    }

    template<typename... Items>
    static Vector from(Items&&... items)
    {
        Vector result;
        auto size = sizeof...(items);

        result.reserveInitialCapacity(size);
        result.asanSetInitialBufferSizeTo(size);
        result.m_size = size;

        result.uncheckedInitialize<0>(std::forward<Items>(items)...);
        return result;
    }

    Vector(WTF::HashTableDeletedValueType)
        : Base(0, std::numeric_limits<decltype(m_size)>::max())
    {
    }

    ~Vector()
    {
        if (m_size)
            TypeOperations::destruct(begin(), end());

        asanSetBufferSizeToFullCapacity(0);
    }

    Vector(const Vector&);
    template<size_t otherCapacity, typename otherOverflowBehaviour, size_t otherMinimumCapacity, typename OtherMalloc>
    explicit Vector(const Vector<T, otherCapacity, otherOverflowBehaviour, otherMinimumCapacity, OtherMalloc>&);

    Vector& operator=(const Vector&);
    template<size_t otherCapacity, typename otherOverflowBehaviour, size_t otherMinimumCapacity, typename OtherMalloc>
    Vector& operator=(const Vector<T, otherCapacity, otherOverflowBehaviour, otherMinimumCapacity, OtherMalloc>&);

    Vector(Vector&&);
    Vector& operator=(Vector&&);

    size_t size() const { return m_size; }
    size_t sizeInBytes() const { return static_cast<size_t>(m_size) * sizeof(T); }
    static ptrdiff_t sizeMemoryOffset() { return OBJECT_OFFSETOF(Vector, m_size); }
    size_t capacity() const { return Base::capacity(); }
    bool isEmpty() const { return !size(); }

    T& at(size_t i)
    {
        if (UNLIKELY(i >= size()))
            OverflowHandler::overflowed();
        return Base::buffer()[i];
    }
    const T& at(size_t i) const 
    {
        if (UNLIKELY(i >= size()))
            OverflowHandler::overflowed();
        return Base::buffer()[i];
    }
    T& at(Checked<size_t> i)
    {
        RELEASE_ASSERT(i < size());
        return Base::buffer()[i];
    }
    const T& at(Checked<size_t> i) const
    {
        RELEASE_ASSERT(i < size());
        return Base::buffer()[i];
    }

    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }
    T& operator[](Checked<size_t> i) { return at(i); }
    const T& operator[](Checked<size_t> i) const { return at(i); }

    T* data() { return Base::buffer(); }
    const T* data() const { return Base::buffer(); }
    static ptrdiff_t dataMemoryOffset() { return Base::bufferMemoryOffset(); }

    iterator begin() { return data(); }
    iterator end() { return begin() + m_size; }
    const_iterator begin() const { return data(); }
    const_iterator end() const { return begin() + m_size; }

    reverse_iterator rbegin() { return reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }

    T& first() { return at(0); }
    const T& first() const { return at(0); }
    T& last() { return at(size() - 1); }
    const T& last() const { return at(size() - 1); }
    
    T takeLast()
    {
        T result = WTFMove(last());
        removeLast();
        return result;
    }
    
    template<typename U> bool contains(const U&) const;
    template<typename U> size_t find(const U&) const;
    template<typename MatchFunction> size_t findMatching(const MatchFunction&) const;
    template<typename U> size_t reverseFind(const U&) const;
    
    template<typename U> bool appendIfNotContains(const U&);

    void shrink(size_t size);
    void grow(size_t size);
    void resize(size_t size);
    void resizeToFit(size_t size);
    ALWAYS_INLINE void reserveCapacity(size_t newCapacity) { reserveCapacity<FailureAction::Crash>(newCapacity); }
    ALWAYS_INLINE bool tryReserveCapacity(size_t newCapacity) { return reserveCapacity<FailureAction::Report>(newCapacity); }
    ALWAYS_INLINE void reserveInitialCapacity(size_t initialCapacity) { reserveInitialCapacity<FailureAction::Crash>(initialCapacity); }
    ALWAYS_INLINE bool tryReserveInitialCapacity(size_t initialCapacity) { return reserveInitialCapacity<FailureAction::Report>(initialCapacity); }
    void shrinkCapacity(size_t newCapacity);
    void shrinkToFit() { shrinkCapacity(size()); }

    void clear() { shrinkCapacity(0); }

    template<typename U = T> Vector<U> isolatedCopy() const &;
    template<typename U = T> Vector<U> isolatedCopy() &&;

    ALWAYS_INLINE void append(ValueType&& value) { append<ValueType>(std::forward<ValueType>(value)); }
    template<typename U> ALWAYS_INLINE void append(U&& u) { append<FailureAction::Crash, U>(std::forward<U>(u)); }
    template<typename U> ALWAYS_INLINE bool tryAppend(U&& u) { return append<FailureAction::Report, U>(std::forward<U>(u)); }
    template<typename... Args> ALWAYS_INLINE void constructAndAppend(Args&&... args) { constructAndAppend<FailureAction::Crash>(std::forward<Args>(args)...); }
    template<typename... Args> ALWAYS_INLINE bool tryConstructAndAppend(Args&&... args) { return constructAndAppend<FailureAction::Report>(std::forward<Args>(args)...); }

    void uncheckedAppend(ValueType&& value) { uncheckedAppend<ValueType>(std::forward<ValueType>(value)); }
    template<typename U> void uncheckedAppend(U&&);
    template<typename... Args> void uncheckedConstructAndAppend(Args&&...);

    template<typename U> ALWAYS_INLINE void append(const U* u, size_t size) { append<FailureAction::Crash>(u, size); }
    template<typename U> ALWAYS_INLINE bool tryAppend(const U* u, size_t size) { return append<FailureAction::Report>(u, size); }
    template<typename U, size_t otherCapacity> void appendVector(const Vector<U, otherCapacity>&);
    template<typename U, size_t otherCapacity> void appendVector(Vector<U, otherCapacity>&&);

    void insert(size_t position, ValueType&& value) { insert<ValueType>(position, std::forward<ValueType>(value)); }
    template<typename U> void insert(size_t position, const U*, size_t);
    template<typename U> void insert(size_t position, U&&);
    template<typename U, size_t c, typename OH, size_t m, typename M> void insertVector(size_t position, const Vector<U, c, OH, m, M>&);

    void remove(size_t position);
    void remove(size_t position, size_t length);
    template<typename U> bool removeFirst(const U&);
    template<typename MatchFunction> bool removeFirstMatching(const MatchFunction&, size_t startIndex = 0);
    template<typename U> unsigned removeAll(const U&);
    template<typename MatchFunction> unsigned removeAllMatching(const MatchFunction&, size_t startIndex = 0);

    void removeLast() 
    {
        if (UNLIKELY(isEmpty()))
            OverflowHandler::overflowed();
        shrink(size() - 1); 
    }

    void fill(const T&, size_t);
    void fill(const T& val) { fill(val, size()); }

    template<typename Iterator> void appendRange(Iterator start, Iterator end);

    MallocPtr<T, Malloc> releaseBuffer();

    void swap(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& other)
    {
#if ASAN_ENABLED
        if (this == std::addressof(other)) // ASan will crash if we try to restrict access to the same buffer twice.
            return;
#endif

        // Make it possible to copy inline buffers.
        asanSetBufferSizeToFullCapacity();
        other.asanSetBufferSizeToFullCapacity();

        Base::swap(other, m_size, other.m_size);
        std::swap(m_size, other.m_size);

        asanSetInitialBufferSizeTo(m_size);
        other.asanSetInitialBufferSizeTo(other.m_size);
    }

    void reverse();

    void checkConsistency();

    template<typename MapFunction, typename R = typename std::result_of<MapFunction(const T&)>::type> Vector<R> map(MapFunction) const;

    bool isHashTableDeletedValue() const { return m_size == std::numeric_limits<decltype(m_size)>::max(); }

private:
    template<FailureAction> bool reserveCapacity(size_t newCapacity);
    template<FailureAction> bool reserveInitialCapacity(size_t initialCapacity);

    template<FailureAction> bool expandCapacity(size_t newMinCapacity);
    template<FailureAction> T* expandCapacity(size_t newMinCapacity, T*);
    template<FailureAction, typename U> U* expandCapacity(size_t newMinCapacity, U*);
    template<FailureAction, typename U> bool appendSlowCase(U&&);
    template<FailureAction, typename... Args> bool constructAndAppend(Args&&...);
    template<FailureAction, typename... Args> bool constructAndAppendSlowCase(Args&&...);

    template<FailureAction, typename U> bool append(U&&);
    template<FailureAction, typename U> bool append(const U*, size_t);

    template<size_t position, typename U, typename... Items>
    void uncheckedInitialize(U&& item, Items&&... items)
    {
        uncheckedInitialize<position>(std::forward<U>(item));
        uncheckedInitialize<position + 1>(std::forward<Items>(items)...);
    }
    template<size_t position, typename U>
    void uncheckedInitialize(U&& value)
    {
        ASSERT(position < size());
        ASSERT(position < capacity());
        new (NotNull, begin() + position) T(std::forward<U>(value));
    }

    void asanSetInitialBufferSizeTo(size_t);
    void asanSetBufferSizeToFullCapacity(size_t);
    void asanSetBufferSizeToFullCapacity() { asanSetBufferSizeToFullCapacity(size()); }

    void asanBufferSizeWillChangeTo(size_t);

    using Base::m_size;
    using Base::buffer;
    using Base::capacity;
    using Base::swap;
    using Base::allocateBuffer;
    using Base::deallocateBuffer;
    using Base::tryAllocateBuffer;
    using Base::shouldReallocateBuffer;
    using Base::reallocateBuffer;
    using Base::restoreInlineBufferIfNeeded;
    using Base::releaseBuffer;
#if ASAN_ENABLED
    using Base::endOfBuffer;
#endif
};

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(const Vector& other)
    : Base(other.capacity(), other.size())
{
    asanSetInitialBufferSizeTo(other.size());

    if (begin())
        TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<size_t otherCapacity, typename otherOverflowBehaviour, size_t otherMinimumCapacity, typename OtherMalloc>
Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(const Vector<T, otherCapacity, otherOverflowBehaviour, otherMinimumCapacity, OtherMalloc>& other)
    : Base(other.capacity(), other.size())
{
    asanSetInitialBufferSizeTo(other.size());

    if (begin())
        TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::operator=(const Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& other)
{
    if (&other == this)
        return *this;
    
    if (size() > other.size())
        shrink(other.size());
    else if (other.size() > capacity()) {
        clear();
        reserveCapacity(other.size());
        ASSERT(begin());
    }

    asanBufferSizeWillChangeTo(other.size());

    std::copy(other.begin(), other.begin() + size(), begin());
    TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
    m_size = other.size();

    return *this;
}

inline bool typelessPointersAreEqual(const void* a, const void* b) { return a == b; }

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<size_t otherCapacity, typename otherOverflowBehaviour, size_t otherMinimumCapacity, typename OtherMalloc>
Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::operator=(const Vector<T, otherCapacity, otherOverflowBehaviour, otherMinimumCapacity, OtherMalloc>& other)
{
    // If the inline capacities match, we should call the more specific
    // template.  If the inline capacities don't match, the two objects
    // shouldn't be allocated the same address.
    ASSERT(!typelessPointersAreEqual(&other, this));

    if (size() > other.size())
        shrink(other.size());
    else if (other.size() > capacity()) {
        clear();
        reserveCapacity(other.size());
        ASSERT(begin());
    }
    
    asanBufferSizeWillChangeTo(other.size());

    std::copy(other.begin(), other.begin() + size(), begin());
    TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
    m_size = other.size();

    return *this;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& other)
{
    swap(other);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::operator=(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& other)
{
    swap(other);
    return *this;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::contains(const U& value) const
{
    return find(value) != notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename MatchFunction>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::findMatching(const MatchFunction& matches) const
{
    for (size_t i = 0; i < size(); ++i) {
        if (matches(at(i)))
            return i;
    }
    return notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::find(const U& value) const
{
    return findMatching([&](auto& item) {
        return item == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reverseFind(const U& value) const
{
    for (size_t i = 1; i <= size(); ++i) {
        const size_t index = size() - i;
        if (at(index) == value)
            return index;
    }
    return notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendIfNotContains(const U& value)
{
    if (contains(value))
        return false;
    append(value);
    return true;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::fill(const T& val, size_t newSize)
{
    if (size() > newSize)
        shrink(newSize);
    else if (newSize > capacity()) {
        clear();
        reserveCapacity(newSize);
        ASSERT(begin());
    }

    asanBufferSizeWillChangeTo(newSize);

    std::fill(begin(), end(), val);
    TypeOperations::uninitializedFill(end(), begin() + newSize, val);
    m_size = newSize;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename Iterator>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendRange(Iterator start, Iterator end)
{
    for (Iterator it = start; it != end; ++it)
        append(*it);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::expandCapacity(size_t newMinCapacity)
{
    return reserveCapacity<action>(std::max(newMinCapacity, std::max(static_cast<size_t>(minCapacity), capacity() + capacity() / 4 + 1)));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action>
NEVER_INLINE T* Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::expandCapacity(size_t newMinCapacity, T* ptr)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    if (ptr < begin() || ptr >= end()) {
        bool success = expandCapacity<action>(newMinCapacity);
        if constexpr (action == FailureAction::Report) {
            if (UNLIKELY(!success))
                return nullptr;
        }
        return ptr;
    }
    size_t index = ptr - begin();
    bool success = expandCapacity<action>(newMinCapacity);
    if constexpr (action == FailureAction::Report) {
        if (UNLIKELY(!success))
            return nullptr;
    }
    return begin() + index;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action, typename U>
inline U* Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::expandCapacity(size_t newMinCapacity, U* ptr)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    bool success = expandCapacity<action>(newMinCapacity);
    if constexpr (action == FailureAction::Report) {
        if (UNLIKELY(!success))
            return nullptr;
    }
    return ptr;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::resize(size_t size)
{
    if (size <= m_size) {
        TypeOperations::destruct(begin() + size, end());
        asanBufferSizeWillChangeTo(size);
    } else {
        if (size > capacity())
            expandCapacity<FailureAction::Crash>(size);
        asanBufferSizeWillChangeTo(size);
        if (begin())
            TypeOperations::initializeIfNonPOD(end(), begin() + size);
    }
    
    m_size = size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::resizeToFit(size_t size)
{
    reserveCapacity(size);
    resize(size);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::shrink(size_t size)
{
    ASSERT(size <= m_size);
    TypeOperations::destruct(begin() + size, end());
    asanBufferSizeWillChangeTo(size);
    m_size = size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::grow(size_t size)
{
    ASSERT(size >= m_size);
    if (size > capacity())
        expandCapacity<FailureAction::Crash>(size);
    asanBufferSizeWillChangeTo(size);
    if (begin())
        TypeOperations::initializeIfNonPOD(end(), begin() + size);
    m_size = size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::asanSetInitialBufferSizeTo(size_t size)
{
#if ASAN_ENABLED
    if (!buffer())
        return;

    // This function resticts buffer access to only elements in [begin(), end()) range, making ASan detect an error
    // when accessing elements in [end(), endOfBuffer()) range.
    // A newly allocated buffer can be accessed without restrictions, so "old_mid" argument equals "end" argument.
    __sanitizer_annotate_contiguous_container(buffer(), endOfBuffer(), endOfBuffer(), buffer() + size);
#else
    UNUSED_PARAM(size);
#endif
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::asanSetBufferSizeToFullCapacity(size_t size)
{
#if ASAN_ENABLED
    if (!buffer())
        return;

    // ASan requires that the annotation is returned to its initial state before deallocation.
    __sanitizer_annotate_contiguous_container(buffer(), endOfBuffer(), buffer() + size, endOfBuffer());
#else
    UNUSED_PARAM(size);
#endif
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::asanBufferSizeWillChangeTo(size_t newSize)
{
#if ASAN_ENABLED
    if (!buffer())
        return;

    // Change allowed range.
    __sanitizer_annotate_contiguous_container(buffer(), endOfBuffer(), buffer() + size(), buffer() + newSize);
#else
    UNUSED_PARAM(newSize);
#endif
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reserveCapacity(size_t newCapacity)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    if (newCapacity <= capacity())
        return true;
    T* oldBuffer = begin();
    T* oldEnd = end();

    asanSetBufferSizeToFullCapacity();

    bool success = Base::template allocateBuffer<action>(newCapacity);
    if constexpr (action == FailureAction::Report) {
        if (UNLIKELY(!success)) {
            asanSetInitialBufferSizeTo(size());
            return false;
        }
    }
    ASSERT(begin());

    asanSetInitialBufferSizeTo(size());

    TypeOperations::move(oldBuffer, oldEnd, begin());
    Base::deallocateBuffer(oldBuffer);
    return true;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reserveInitialCapacity(size_t initialCapacity)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    ASSERT(!m_size);
    ASSERT(capacity() == inlineCapacity);
    if (initialCapacity <= inlineCapacity)
        return true;
    return Base::template allocateBuffer<action>(initialCapacity);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::shrinkCapacity(size_t newCapacity)
{
    if (newCapacity >= capacity())
        return;

    if (newCapacity < size()) 
        shrink(newCapacity);

    asanSetBufferSizeToFullCapacity();

    T* oldBuffer = begin();
    if (newCapacity > 0) {
        if (Base::shouldReallocateBuffer(newCapacity)) {
            Base::reallocateBuffer(newCapacity);
            asanSetInitialBufferSizeTo(size());
            return;
        }

        T* oldEnd = end();
        Base::allocateBuffer(newCapacity);
        if (begin() != oldBuffer)
            TypeOperations::move(oldBuffer, oldEnd, begin());
    }

    Base::deallocateBuffer(oldBuffer);
    Base::restoreInlineBufferIfNeeded();

    asanSetInitialBufferSizeTo(size());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action, typename U>
ALWAYS_INLINE bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::append(const U* data, size_t dataSize)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    size_t newSize = m_size + dataSize;
    if (newSize > capacity()) {
        data = expandCapacity<action>(newSize, data);
        if constexpr (action == FailureAction::Report) {
            if (UNLIKELY(!data))
                return false;
        }
        ASSERT(begin());
    }
    if (newSize < m_size) {
        if constexpr (action == FailureAction::Crash)
            CRASH();
        else
            return false;
    }
    asanBufferSizeWillChangeTo(newSize);
    T* dest = end();
    VectorCopier<std::is_trivial<T>::value, U>::uninitializedCopy(data, std::addressof(data[dataSize]), dest);
    m_size = newSize;
    return true;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action, typename U>
ALWAYS_INLINE bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::append(U&& value)
{
    if (size() != capacity()) {
        asanBufferSizeWillChangeTo(m_size + 1);
        new (NotNull, end()) T(std::forward<U>(value));
        ++m_size;
        return true;
    }

    return appendSlowCase<action, U>(std::forward<U>(value));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action, typename... Args>
ALWAYS_INLINE bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::constructAndAppend(Args&&... args)
{
    if (size() != capacity()) {
        asanBufferSizeWillChangeTo(m_size + 1);
        new (NotNull, end()) T(std::forward<Args>(args)...);
        ++m_size;
        return true;
    }

    return constructAndAppendSlowCase<action>(std::forward<Args>(args)...);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action, typename U>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendSlowCase(U&& value)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    ASSERT(size() == capacity());

    auto ptr = const_cast<typename std::remove_const<typename std::remove_reference<U>::type>::type*>(std::addressof(value));
    ptr = expandCapacity<action>(size() + 1, ptr);
    if constexpr (action == FailureAction::Report) {
        if (UNLIKELY(!ptr))
            return false;
    }
    ASSERT(begin());

    asanBufferSizeWillChangeTo(m_size + 1);
    new (NotNull, end()) T(std::forward<U>(*ptr));
    ++m_size;
    return true;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action, typename... Args>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::constructAndAppendSlowCase(Args&&... args)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    ASSERT(size() == capacity());

    bool success = expandCapacity<action>(size() + 1);
    if constexpr (action == FailureAction::Report) {
        if (UNLIKELY(!success))
            return false;
    }
    ASSERT(begin());

    asanBufferSizeWillChangeTo(m_size + 1);
    new (NotNull, end()) T(std::forward<Args>(args)...);
    ++m_size;
    return true;
}

// This version of append saves a branch in the case where you know that the
// vector's capacity is large enough for the append to succeed.

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
ALWAYS_INLINE void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::uncheckedAppend(U&& value)
{
    ASSERT(size() < capacity());

    asanBufferSizeWillChangeTo(m_size + 1);

    new (NotNull, end()) T(std::forward<U>(value));
    ++m_size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename... Args>
ALWAYS_INLINE void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::uncheckedConstructAndAppend(Args&&... args)
{
    ASSERT(size() < capacity());

    asanBufferSizeWillChangeTo(m_size + 1);

    new (NotNull, end()) T(std::forward<Args>(args)...);
    ++m_size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U, size_t otherCapacity>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendVector(const Vector<U, otherCapacity>& val)
{
    append(val.begin(), val.size());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U, size_t otherCapacity>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendVector(Vector<U, otherCapacity>&& val)
{
    size_t newSize = m_size + val.size();
    if (newSize > capacity())
        expandCapacity<FailureAction::Crash>(newSize);
    for (auto& item : val)
        uncheckedAppend(WTFMove(item));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insert(size_t position, const U* data, size_t dataSize)
{
    ASSERT_WITH_SECURITY_IMPLICATION(position <= size());
    size_t newSize = m_size + dataSize;
    if (newSize > capacity()) {
        data = expandCapacity<FailureAction::Crash>(newSize, data);
        ASSERT(begin());
    }
    if (newSize < m_size)
        CRASH();
    asanBufferSizeWillChangeTo(newSize);
    T* spot = begin() + position;
    TypeOperations::moveOverlapping(spot, end(), spot + dataSize);
    VectorCopier<std::is_trivial<T>::value, U>::uninitializedCopy(data, std::addressof(data[dataSize]), spot);
    m_size = newSize;
}
 
template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insert(size_t position, U&& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(position <= size());

    auto ptr = const_cast<typename std::remove_const<typename std::remove_reference<U>::type>::type*>(std::addressof(value));
    if (size() == capacity()) {
        ptr = expandCapacity<FailureAction::Crash>(size() + 1, ptr);
        ASSERT(begin());
    }

    asanBufferSizeWillChangeTo(m_size + 1);

    T* spot = begin() + position;
    TypeOperations::moveOverlapping(spot, end(), spot + 1);
    new (NotNull, spot) T(std::forward<U>(*ptr));
    ++m_size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U, size_t c, typename OH, size_t m, typename M>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insertVector(size_t position, const Vector<U, c, OH, m, M>& val)
{
    insert(position, val.begin(), val.size());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::remove(size_t position)
{
    ASSERT_WITH_SECURITY_IMPLICATION(position < size());
    T* spot = begin() + position;
    spot->~T();
    TypeOperations::moveOverlapping(spot + 1, end(), spot);
    asanBufferSizeWillChangeTo(m_size - 1);
    --m_size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::remove(size_t position, size_t length)
{
    ASSERT_WITH_SECURITY_IMPLICATION(position <= size());
    ASSERT_WITH_SECURITY_IMPLICATION(position + length <= size());
    T* beginSpot = begin() + position;
    T* endSpot = beginSpot + length;
    TypeOperations::destruct(beginSpot, endSpot); 
    TypeOperations::moveOverlapping(endSpot, end(), beginSpot);
    asanBufferSizeWillChangeTo(m_size - length);
    m_size -= length;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeFirst(const U& value)
{
    return removeFirstMatching([&value] (const T& current) {
        return current == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename MatchFunction>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeFirstMatching(const MatchFunction& matches, size_t startIndex)
{
    for (size_t i = startIndex; i < size(); ++i) {
        if (matches(at(i))) {
            remove(i);
            return true;
        }
    }
    return false;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
inline unsigned Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeAll(const U& value)
{
    return removeAllMatching([&value] (const T& current) {
        return current == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename MatchFunction>
inline unsigned Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeAllMatching(const MatchFunction& matches, size_t startIndex)
{
    iterator holeBegin = end();
    iterator holeEnd = end();
    unsigned matchCount = 0;
    for (auto it = begin() + startIndex, itEnd = end(); it < itEnd; ++it) {
        if (matches(*it)) {
            if (holeBegin == end())
                holeBegin = it;
            else if (holeEnd != it) {
                TypeOperations::moveOverlapping(holeEnd, it, holeBegin);
                holeBegin += it - holeEnd;
            }
            holeEnd = it + 1;
            it->~T();
            ++matchCount;
        }
    }
    if (holeEnd != end())
        TypeOperations::moveOverlapping(holeEnd, end(), holeBegin);
    asanBufferSizeWillChangeTo(m_size - matchCount);
    m_size -= matchCount;
    return matchCount;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reverse()
{
    for (size_t i = 0; i < m_size / 2; ++i)
        std::swap(at(i), at(m_size - 1 - i));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename MapFunction, typename R>
inline Vector<R> Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::map(MapFunction mapFunction) const
{
    Vector<R> result;
    result.reserveInitialCapacity(size());
    for (size_t i = 0; i < size(); ++i)
        result.uncheckedAppend(mapFunction(at(i)));
    return result;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline MallocPtr<T, Malloc> Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::releaseBuffer()
{
    // FIXME: Find a way to preserve annotations on the returned buffer.
    // ASan requires that all annotations are removed before deallocation,
    // and MallocPtr doesn't implement that.
    asanSetBufferSizeToFullCapacity();

    auto buffer = Base::releaseBuffer();
    if (inlineCapacity && !buffer && m_size) {
        // If the vector had some data, but no buffer to release,
        // that means it was using the inline buffer. In that case,
        // we create a brand new buffer so the caller always gets one.
        size_t bytes = m_size * sizeof(T);
        buffer = adoptMallocPtr<T, Malloc>(static_cast<T*>(Malloc::malloc(bytes)));
        memcpy(buffer.get(), data(), bytes);
    }
    m_size = 0;
    // FIXME: Should we call Base::restoreInlineBufferIfNeeded() here?
    return buffer;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::checkConsistency()
{
#if ASSERT_ENABLED
    for (size_t i = 0; i < size(); ++i)
        ValueCheck<T>::checkConsistency(at(i));
#endif
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void swap(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& a, Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& b)
{
    a.swap(b);
}

template<typename T, size_t inlineCapacityA, typename OverflowHandlerA, size_t minCapacityA, typename MallocA, size_t inlineCapacityB, typename OverflowHandlerB, size_t minCapacityB, typename MallocB>
bool operator==(const Vector<T, inlineCapacityA, OverflowHandlerA, minCapacityA, MallocA>& a, const Vector<T, inlineCapacityB, OverflowHandlerB, minCapacityB, MallocB>& b)
{
    if (a.size() != b.size())
        return false;

    return VectorTypeOperations<T>::compare(a.data(), b.data(), a.size());
}

template<typename T, size_t inlineCapacityA, typename OverflowHandlerA, size_t minCapacityA, typename MallocA, size_t inlineCapacityB, typename OverflowHandlerB, size_t minCapacityB, typename MallocB>
inline bool operator!=(const Vector<T, inlineCapacityA, OverflowHandlerA, minCapacityA, MallocA>& a, const Vector<T, inlineCapacityB, OverflowHandlerB, minCapacityB, MallocB>& b)
{
    return !(a == b);
}

#if ASSERT_ENABLED
template<typename T> struct ValueCheck<Vector<T>> {
    typedef Vector<T> TraitType;
    static void checkConsistency(const Vector<T>& v)
    {
        v.checkConsistency();
    }
};
#endif // ASSERT_ENABLED

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
inline Vector<U> Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::isolatedCopy() const &
{
    Vector<U> copy;
    copy.reserveInitialCapacity(size());
    for (const auto& element : *this)
        copy.uncheckedAppend(element.isolatedCopy());
    return copy;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
inline Vector<U> Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::isolatedCopy() &&
{
    for (auto iterator = begin(), iteratorEnd = end(); iterator < iteratorEnd; ++iterator)
        *iterator = WTFMove(*iterator).isolatedCopy();
    return WTFMove(*this);
}

template<typename VectorType, typename Func>
size_t removeRepeatedElements(VectorType& vector, const Func& func)
{
    auto end = std::unique(vector.begin(), vector.end(), func);
    size_t newSize = end - vector.begin();
    vector.shrink(newSize);
    return newSize;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
size_t removeRepeatedElements(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& vector)
{
    return removeRepeatedElements(vector, [] (T& a, T& b) { return a == b; });
}

template<typename SourceType>
struct CollectionInspector {
    using RealSourceType = typename std::remove_reference<SourceType>::type;
    using IteratorType = decltype(std::begin(std::declval<RealSourceType>()));
    using SourceItemType = typename std::iterator_traits<IteratorType>::value_type;
};

template<typename MapFunction, typename SourceType, typename Enable = void>
struct Mapper {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename std::result_of<MapFunction(SourceItemType&)>::type;

    static Vector<DestinationItemType> map(SourceType source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        // FIXME: Use std::size when available on all compilers.
        result.reserveInitialCapacity(source.size());
        for (auto& item : source)
            result.uncheckedAppend(mapFunction(item));
        return result;
    }
};

template<typename MapFunction, typename SourceType>
struct Mapper<MapFunction, SourceType, typename std::enable_if<std::is_rvalue_reference<SourceType&&>::value>::type> {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename std::result_of<MapFunction(SourceItemType&&)>::type;

    static Vector<DestinationItemType> map(SourceType&& source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        // FIXME: Use std::size when available on all compilers.
        result.reserveInitialCapacity(source.size());
        for (auto& item : source)
            result.uncheckedAppend(mapFunction(WTFMove(item)));
        return result;
    }
};

template<typename MapFunction, typename SourceType>
Vector<typename Mapper<MapFunction, SourceType>::DestinationItemType> map(SourceType&& source, MapFunction&& mapFunction)
{
    return Mapper<MapFunction, SourceType>::map(std::forward<SourceType>(source), std::forward<MapFunction>(mapFunction));
}

template<typename MapFunctionReturnType>
struct CompactMapTraits {
    static bool hasValue(const MapFunctionReturnType&);
    template<typename ItemType>
    static ItemType extractValue(MapFunctionReturnType&&);
};

template<typename T>
struct CompactMapTraits<Optional<T>> {
    using ItemType = T;
    static bool hasValue(const Optional<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(Optional<T>&& returnValue) { return WTFMove(*returnValue); }
};

template<typename T>
struct CompactMapTraits<RefPtr<T>> {
    using ItemType = Ref<T>;
    static bool hasValue(const RefPtr<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(RefPtr<T>&& returnValue) { return returnValue.releaseNonNull(); }
};

template<typename MapFunction, typename SourceType, typename Enable = void>
struct CompactMapper {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::result_of<MapFunction(SourceItemType&)>::type;
    using DestinationItemType = typename CompactMapTraits<ResultItemType>::ItemType;

    static Vector<DestinationItemType> compactMap(SourceType source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        for (auto& item : source) {
            auto itemResult = mapFunction(item);
            if (CompactMapTraits<ResultItemType>::hasValue(itemResult))
                result.append(CompactMapTraits<ResultItemType>::extractValue(WTFMove(itemResult)));
        }
        result.shrinkToFit();
        return result;
    }
};

template<typename MapFunction, typename SourceType>
struct CompactMapper<MapFunction, SourceType, typename std::enable_if<std::is_rvalue_reference<SourceType&&>::value>::type> {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::result_of<MapFunction(SourceItemType&&)>::type;
    using DestinationItemType = typename CompactMapTraits<ResultItemType>::ItemType;

    static Vector<DestinationItemType> compactMap(SourceType source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        for (auto& item : source) {
            auto itemResult = mapFunction(WTFMove(item));
            if (CompactMapTraits<ResultItemType>::hasValue(itemResult))
                result.append(CompactMapTraits<ResultItemType>::extractValue(WTFMove(itemResult)));
        }
        result.shrinkToFit();
        return result;
    }
};

template<typename MapFunction, typename SourceType>
Vector<typename CompactMapper<MapFunction, SourceType>::DestinationItemType> compactMap(SourceType&& source, MapFunction&& mapFunction)
{
    return CompactMapper<MapFunction, SourceType>::compactMap(std::forward<SourceType>(source), std::forward<MapFunction>(mapFunction));
}

template<typename DestinationVector, typename Collection>
inline auto copyToVectorSpecialization(const Collection& collection) -> DestinationVector
{
    DestinationVector result;
    // FIXME: Use std::size when available on all compilers.
    result.reserveInitialCapacity(collection.size());
    for (auto& item : collection)
        result.uncheckedAppend(item);
    return result;
}

template<typename DestinationItemType, typename Collection>
inline auto copyToVectorOf(const Collection& collection) -> Vector<DestinationItemType>
{
    return WTF::map(collection, [] (const auto& v) -> DestinationItemType { return v; });
}

template<typename Collection>
struct CopyToVectorResult {
    using Type = typename std::remove_cv<typename CollectionInspector<Collection>::SourceItemType>::type;
};

template<typename Collection>
inline auto copyToVector(const Collection& collection) -> Vector<typename CopyToVectorResult<Collection>::Type>
{
    return copyToVectorOf<typename CopyToVectorResult<Collection>::Type>(collection);
}

} // namespace WTF

using WTF::UnsafeVectorOverflow;
using WTF::Vector;
using WTF::copyToVector;
using WTF::copyToVectorOf;
using WTF::copyToVectorSpecialization;
using WTF::compactMap;
using WTF::removeRepeatedElements;
