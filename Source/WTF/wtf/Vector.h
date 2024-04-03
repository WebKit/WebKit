/*
 *  Copyright (C) 2005-2024 Apple Inc. All rights reserved.
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
#include <optional>
#include <span>
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

#if ASAN_ENABLED && __has_include(<sanitizer/asan_interface.h>)
#include <sanitizer/asan_interface.h>
#endif

namespace JSC {
class LLIntOffsetsExtractor;
}

namespace WTF {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(Vector);
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(VectorBuffer);

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

    template<typename... Args>
    static void initializeWithArgs(T* begin, T* end, Args&&... args)
    {
        for (T *cur = begin; cur != end; ++cur)
            new (NotNull, cur) T(args...);
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

    template<typename ... Args>
    static void initializeWithArgs(T* begin, T* end, Args&&... args)
    {
        VectorInitializer<VectorTraits<T>::needsInitialization, VectorTraits<T>::canInitializeWithMemset, T>::initializeWithArgs(begin, end, std::forward<Args>(args)...);
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

template<typename T>
constexpr inline bool isValidCapacityForVector(size_t capacity) { return capacity <= std::numeric_limits<unsigned>::max() / sizeof(T); }

template<typename Collection> struct CopyOrMoveToVectorResult;

template<typename T, typename Malloc>
class VectorBufferBase {
    WTF_MAKE_NONCOPYABLE(VectorBufferBase);
public:
    template<FailureAction action>
    bool allocateBuffer(size_t newCapacity)
    {
        static_assert(action == FailureAction::Crash || action == FailureAction::Report);
        ASSERT(newCapacity);
        if (!isValidCapacityForVector<T>(newCapacity)) {
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

template<typename T, size_t inlineCapacity, typename Malloc = VectorBufferMalloc> class VectorBuffer;

template<typename T, typename Malloc>
class VectorBuffer<T, 0, Malloc> : private VectorBufferBase<T, Malloc> {
private:
    typedef VectorBufferBase<T, Malloc> Base;
public:
    VectorBuffer()
    {
    }

    explicit VectorBuffer(size_t capacity, size_t size = 0)
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

    VectorBuffer(VectorBuffer<T, 0, Malloc>&& other)
    {
        m_buffer = std::exchange(other.m_buffer, nullptr);
        m_capacity = std::exchange(other.m_capacity, 0);
        m_size = std::exchange(other.m_size, 0);
    }

    void adopt(VectorBuffer&& other)
    {
        deallocateBuffer(buffer());
        m_buffer = std::exchange(other.m_buffer, nullptr);
        m_capacity = std::exchange(other.m_capacity, 0);
        m_size = std::exchange(other.m_size, 0);
    }

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

    explicit VectorBuffer(size_t capacity, size_t size = 0)
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
        // FIXME: This should ASSERT(!m_buffer) to catch misuse/leaks. https://bugs.webkit.org/show_bug.cgi?id=250801
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
        ASSERT_WITH_SECURITY_IMPLICATION(buffer());

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

    VectorBuffer(VectorBuffer&& other)
        : Base(inlineBuffer(), inlineCapacity, 0)
    {
        if (other.buffer() == other.inlineBuffer())
            VectorTypeOperations<T>::move(other.inlineBuffer(), other.inlineBuffer() + other.m_size, inlineBuffer());
        else {
            m_buffer = std::exchange(other.m_buffer, other.inlineBuffer());
            m_capacity = std::exchange(other.m_capacity, inlineCapacity);
        }
        m_size = std::exchange(other.m_size, 0);
    }

    void adopt(VectorBuffer&& other)
    {
        if (buffer() != inlineBuffer()) {
            deallocateBuffer(buffer());
            m_buffer = inlineBuffer();
        }
        if (other.buffer() == other.inlineBuffer()) {
            VectorTypeOperations<T>::move(other.inlineBuffer(), other.inlineBuffer() + other.m_size, inlineBuffer());
            m_capacity = other.m_capacity;
        } else {
            m_buffer = std::exchange(other.m_buffer, other.inlineBuffer());
            m_capacity = std::exchange(other.m_capacity, inlineCapacity);
        }
        m_size = std::exchange(other.m_size, 0);
    }

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
        
        ASSERT_WITH_SECURITY_IMPLICATION(leftSize <= inlineCapacity);
        ASSERT_WITH_SECURITY_IMPLICATION(rightSize <= inlineCapacity);
        
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
    static NO_RETURN_DUE_TO_ASSERT_WITH_SECURITY_IMPLICATION void overflowed()
    {
        ASSERT_NOT_REACHED_WITH_SECURITY_IMPLICATION();
    }
};

// Template default values are in Forward.h.
template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
class Vector : private VectorBuffer<T, inlineCapacity, Malloc> {
    WTF_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(Vector);
private:
    typedef VectorBuffer<T, inlineCapacity, Malloc> Base;
    typedef VectorTypeOperations<T> TypeOperations;
    friend class JSC::LLIntOffsetsExtractor;

public:
    // FIXME: Remove uses of ValueType and standardize on value_type, which is required for std::span.
    typedef T ValueType;
    typedef T value_type;

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

    template<typename Functor, typename = typename std::enable_if_t<std::is_invocable_v<Functor, size_t>>>
    Vector(size_t size, const Functor& valueGenerator)
    {
        reserveInitialCapacity(size);

        asanSetInitialBufferSizeTo(size);

        if constexpr (std::is_same_v<std::invoke_result_t<Functor, size_t>, std::optional<T>>) {
            for (size_t i = 0; i < size; ++i) {
                if (auto item = valueGenerator(i))
                    unsafeAppendWithoutCapacityCheck(WTFMove(*item));
                else
                    return;
            }
        } else {
            for (size_t i = 0; i < size; ++i)
                unsafeAppendWithoutCapacityCheck(valueGenerator(i));
        }
    }

    // Include this non-template conversion from std::span to guide implicit conversion from arrays.
    Vector(std::span<const T> span)
        : Base(span.size(), span.size())
    {
        asanSetInitialBufferSizeTo(span.size());

        if (begin())
            VectorCopier<std::is_trivial<T>::value, T>::uninitializedCopy(span.data(), span.data() + span.size(), begin());
    }

    template<typename U, size_t Extent> Vector(std::span<U, Extent> span)
        : Base(span.size(), span.size())
    {
        asanSetInitialBufferSizeTo(span.size());

        if (begin())
            VectorCopier<std::is_trivial<T>::value, U>::uninitializedCopy(span.data(), span.data() + span.size(), begin());
    }

    Vector(std::initializer_list<T> initializerList)
    {
        reserveInitialCapacity(initializerList.size());

        asanSetInitialBufferSizeTo(initializerList.size());

        for (const auto& element : initializerList)
            unsafeAppendWithoutCapacityCheck(element);
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
    std::span<const T> span() const { return { data(), size() }; }
    std::span<T> mutableSpan() { return { data(), size() }; }

    Vector<T> subvector(size_t offset, size_t length = std::dynamic_extent) const
    {
        return { span().subspan(offset, length) };
    }

    std::span<const T> subspan(size_t offset, size_t length = std::dynamic_extent) const
    {
        return span().subspan(offset, length);
    }

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

    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }

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
    template<typename MatchFunction> size_t findIf(const MatchFunction&) const;
    template<typename U> size_t reverseFind(const U&) const;
    template<typename MatchFunction> size_t reverseFindIf(const MatchFunction&) const;
    template<typename MatchFunction> bool containsIf(const MatchFunction& matches) const { return findIf(matches) != notFound; }

    template<typename U> bool appendIfNotContains(const U&);

    void shrink(size_t size);
    ALWAYS_INLINE void grow(size_t size) { growImpl<FailureAction::Crash>(size); }
    ALWAYS_INLINE bool tryGrow(size_t size) { return growImpl<FailureAction::Report>(size); }
    void resize(size_t size);
    void resizeToFit(size_t size);
    ALWAYS_INLINE void reserveCapacity(size_t newCapacity) { reserveCapacity<FailureAction::Crash>(newCapacity); }
    ALWAYS_INLINE bool tryReserveCapacity(size_t newCapacity) { return reserveCapacity<FailureAction::Report>(newCapacity); }
    ALWAYS_INLINE void reserveInitialCapacity(size_t initialCapacity) { reserveInitialCapacity<FailureAction::Crash>(initialCapacity); }
    ALWAYS_INLINE bool tryReserveInitialCapacity(size_t initialCapacity) { return reserveInitialCapacity<FailureAction::Report>(initialCapacity); }
    void shrinkCapacity(size_t newCapacity);
    void shrinkToFit() { shrinkCapacity(size()); }

    void growCapacityBy(size_t increment) { growCapacityBy<FailureAction::Crash>(increment); }
    bool tryGrowCapacityBy(size_t increment) { return growCapacityBy<FailureAction::Report>(increment); }

    void clear() { shrinkCapacity(0); }

    ALWAYS_INLINE void append(value_type&& value) { append<value_type>(std::forward<value_type>(value)); }
    ALWAYS_INLINE bool tryAppend(value_type&& value) { return tryAppend<value_type>(std::forward<value_type>(value)); }
    template<typename U> ALWAYS_INLINE void append(U&& u) { append<FailureAction::Crash, U>(std::forward<U>(u)); }
    template<typename U> ALWAYS_INLINE bool tryAppend(U&& u) { return append<FailureAction::Report, U>(std::forward<U>(u)); }
    template<typename... Args> ALWAYS_INLINE void constructAndAppend(Args&&... args) { constructAndAppend<FailureAction::Crash>(std::forward<Args>(args)...); }
    template<typename... Args> ALWAYS_INLINE bool tryConstructAndAppend(Args&&... args) { return constructAndAppend<FailureAction::Report>(std::forward<Args>(args)...); }

    template<typename U, size_t Extent> ALWAYS_INLINE bool tryAppend(std::span<const U, Extent> span) { return append<FailureAction::Report>(span); }
    template<typename U, size_t Extent> ALWAYS_INLINE bool tryAppend(std::span<U, Extent> span) { return append<FailureAction::Report>(std::span<const U> { span.data(), span.size() }); }
    template<typename U, size_t Extent> ALWAYS_INLINE void append(std::span<const U, Extent> span) { append<FailureAction::Crash>(span); }
    template<typename U, size_t Extent> ALWAYS_INLINE void append(std::span<U, Extent> span) { append<FailureAction::Crash>(std::span<const U> { span.data(), span.size() }); }
    template<typename U> ALWAYS_INLINE void appendList(std::initializer_list<U> initializerList) { append<FailureAction::Crash>(std::span { std::data(initializerList), initializerList.size() }); }
    template<typename U, size_t otherCapacity, typename OtherOverflowHandler, size_t otherMinCapacity, typename OtherMalloc> void appendVector(const Vector<U, otherCapacity, OtherOverflowHandler, otherMinCapacity, OtherMalloc>&);
    template<typename U, size_t otherCapacity, typename OtherOverflowHandler, size_t otherMinCapacity, typename OtherMalloc> void appendVector(Vector<U, otherCapacity, OtherOverflowHandler, otherMinCapacity, OtherMalloc>&&);

    template<typename Functor, typename = typename std::enable_if_t<std::is_invocable_v<Functor, size_t>>>
    void appendUsingFunctor(size_t, const Functor&);

    void insert(size_t position, value_type&& value) { insert<value_type>(position, std::forward<value_type>(value)); }
    void insertFill(size_t position, const T& value, size_t dataSize);
    template<typename U> void insert(size_t position, const U*, size_t);
    template<typename U> void insert(size_t position, U&&);
    template<typename U, size_t c, typename OH, size_t m, typename M> void insertVector(size_t position, const Vector<U, c, OH, m, M>&);

    void remove(size_t position);
    void remove(size_t position, size_t length);
    template<typename U> bool removeFirst(const U&);
    template<typename MatchFunction> bool removeFirstMatching(const MatchFunction&, size_t startIndex = 0);
    template<typename U> bool removeLast(const U&);
    template<typename MatchFunction> bool removeLastMatching(const MatchFunction&);
    template<typename MatchFunction> bool removeLastMatching(const MatchFunction&, size_t startIndex);
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
    template<typename ContainerType, typename MapFunction> void appendContainerWithMapping(ContainerType&&, const MapFunction&);

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

    template<typename ResultVector, typename MapFunction>
    auto map(MapFunction&&) const -> std::enable_if_t<std::is_invocable_v<MapFunction, const T&>, ResultVector>;

    template<typename MapFunction>
    auto map(MapFunction&&) const -> std::enable_if_t<std::is_invocable_v<MapFunction, const T&>, Vector<typename std::invoke_result_t<MapFunction, const T&>>>;

    bool isHashTableDeletedValue() const { return m_size == std::numeric_limits<decltype(m_size)>::max(); }

private:
    void unsafeAppendWithoutCapacityCheck(value_type&& value) { unsafeAppendWithoutCapacityCheck<value_type>(std::forward<value_type>(value)); }
    template<typename U> void unsafeAppendWithoutCapacityCheck(U&&);
    template<typename U> bool unsafeAppendWithoutCapacityCheck(const U*, size_t);

    template<FailureAction> bool growImpl(size_t);
    template<FailureAction> bool reserveCapacity(size_t newCapacity);
    template<FailureAction> bool reserveInitialCapacity(size_t initialCapacity);
    template<FailureAction> bool growCapacityBy(size_t increment);

    template<FailureAction> bool expandCapacity(size_t newMinCapacity);
    template<FailureAction> T* expandCapacity(size_t newMinCapacity, T*);
    template<FailureAction, typename U> U* expandCapacity(size_t newMinCapacity, U*);
    template<FailureAction, typename U> bool appendSlowCase(U&&);
    template<FailureAction, typename... Args> bool constructAndAppend(Args&&...);
    template<FailureAction, typename... Args> bool constructAndAppendSlowCase(Args&&...);

    template<FailureAction, typename U> bool append(U&&);
    template<FailureAction, typename U, size_t Extent> bool append(std::span<const U, Extent>);

    template<typename MapFunction, typename DestinationVectorType, typename SourceType, typename Enable> friend struct Mapper;
    template<typename MapFunction, typename DestinationVectorType, typename SourceType, typename Enable> friend struct CompactMapper;
    template<typename DestinationItemType, typename Collection> friend Vector<DestinationItemType> copyToVectorOf(const Collection&);
    template<typename Collection> friend Vector<typename CopyOrMoveToVectorResult<Collection>::Type> copyToVector(const Collection&);
    template<typename U, size_t otherInlineCapacity, typename OtherOverflowHandler, size_t otherMinCapacity, typename OtherMalloc> friend class Vector;
    template<typename DestinationVector, typename Collection> friend DestinationVector copyToVectorSpecialization(const Collection&);

    template<size_t position, typename U, typename... Items>
    void uncheckedInitialize(U&& item, Items&&... items)
    {
        uncheckedInitialize<position>(std::forward<U>(item));
        uncheckedInitialize<position + 1>(std::forward<Items>(items)...);
    }
    template<size_t position, typename U>
    void uncheckedInitialize(U&& value)
    {
        ASSERT_WITH_SECURITY_IMPLICATION(position < size());
        ASSERT_WITH_SECURITY_IMPLICATION(position < capacity());
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
    : Base(other.size(), other.size())
{
    asanSetInitialBufferSizeTo(other.size());

    if (begin())
        TypeOperations::uninitializedCopy(other.begin(), other.end(), begin());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<size_t otherCapacity, typename otherOverflowBehaviour, size_t otherMinimumCapacity, typename OtherMalloc>
Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(const Vector<T, otherCapacity, otherOverflowBehaviour, otherMinimumCapacity, OtherMalloc>& other)
    : Base(other.size(), other.size())
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

    std::copy_n(other.begin(), size(), begin());
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

    std::copy_n(other.begin(), size(), begin());
    TypeOperations::uninitializedCopy(other.begin() + size(), other.end(), end());
    m_size = other.size();

    return *this;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& other)
    : Base(WTFMove(other))
{
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::operator=(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& other)
{
    if (m_size)
        VectorTypeOperations<T>::destruct(begin(), end());

    // Make it possible to copy inline buffer.
    asanSetBufferSizeToFullCapacity();
    other.asanSetBufferSizeToFullCapacity();

    Base::adopt(WTFMove(other));

    asanSetInitialBufferSizeTo(m_size);

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
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::findIf(const MatchFunction& matches) const
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
    return findIf([&](auto& item) {
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
template<typename MatchFunction>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reverseFindIf(const MatchFunction& matches) const
{
    for (size_t i = 1; i <= size(); ++i) {
        const size_t index = size() - i;
        if (matches(at(index)))
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
    using category = typename std::iterator_traits<Iterator>::iterator_category;
    static_assert(std::is_base_of_v<std::input_iterator_tag, category>);

    if constexpr (std::is_base_of_v<std::random_access_iterator_tag, category>)
        reserveCapacity(size() + (end - start));

    for (Iterator it = start; it != end; ++it) {
        if constexpr (std::is_base_of_v<std::random_access_iterator_tag, category>)
            unsafeAppendWithoutCapacityCheck(*it);
        else
            append(*it);
    }
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename Functor, typename>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendUsingFunctor(size_t size, const Functor& valueGenerator)
{
    reserveCapacity(this->size() + size);
    for (size_t i = 0; i < size; ++i)
        unsafeAppendWithoutCapacityCheck(valueGenerator(i));
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
        UNUSED_PARAM(success);
        return ptr;
    }
    size_t index = ptr - begin();
    bool success = expandCapacity<action>(newMinCapacity);
    if constexpr (action == FailureAction::Report) {
        if (UNLIKELY(!success))
            return nullptr;
    }
    UNUSED_PARAM(success);
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
    UNUSED_PARAM(success);
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
    ASSERT_WITH_SECURITY_IMPLICATION(size <= m_size);
    TypeOperations::destruct(begin() + size, end());
    asanBufferSizeWillChangeTo(size);
    m_size = size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction failureAction>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::growImpl(size_t size)
{
    ASSERT_WITH_SECURITY_IMPLICATION(size >= m_size);
    if (size > capacity()) {
        bool success = expandCapacity<failureAction>(size);
        if constexpr (failureAction == FailureAction::Report) {
            if (UNLIKELY(!success))
                return false;
        }
    }
    asanBufferSizeWillChangeTo(size);
    if (begin())
        TypeOperations::initializeIfNonPOD(end(), begin() + size);
    m_size = size;
    return true;
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

    RELEASE_ASSERT_WITH_MESSAGE(newSize <= capacity(), "Attempt to expand size (%lu) beyond current capacity (%lu)", newSize, capacity());

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
    UNUSED_PARAM(success);
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
    ASSERT_WITH_SECURITY_IMPLICATION(!m_size);
    ASSERT(capacity() == inlineCapacity);
    if (initialCapacity <= inlineCapacity)
        return true;
    return Base::template allocateBuffer<action>(initialCapacity);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::growCapacityBy(size_t increment)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    unsigned increment32 = static_cast<unsigned>(increment);
    unsigned capacity32 = static_cast<unsigned>(capacity());
    unsigned newCapacity = increment32 + capacity32;
    if (increment32 < increment || newCapacity < capacity32) {
        if constexpr (action == FailureAction::Crash)
            CRASH();
        else
            return false;
    }
    return reserveCapacity<action>(newCapacity);
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
template<FailureAction action, typename U, size_t Extent>
ALWAYS_INLINE bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::append(std::span<const U, Extent> data)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    auto dataSize = data.size();
    if (!dataSize)
        return true;

    auto* dataPtr = data.data();
    size_t newSize = m_size + dataSize;
    if (newSize > capacity()) {
        dataPtr = expandCapacity<action>(newSize, dataPtr);
        if constexpr (action == FailureAction::Report) {
            if (UNLIKELY(!dataPtr))
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
    VectorCopier<std::is_trivial<T>::value, U>::uninitializedCopy(dataPtr, std::addressof(dataPtr[dataSize]), dest);
    m_size = newSize;
    return true;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
ALWAYS_INLINE bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::unsafeAppendWithoutCapacityCheck(const U* data, size_t dataSize)
{
    if (!dataSize)
        return true;

    ASSERT_WITH_SECURITY_IMPLICATION((Checked<size_t>(size()) + dataSize) <= capacity());

    size_t newSize = m_size + dataSize;
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
    ASSERT_WITH_SECURITY_IMPLICATION(size() == capacity());

    auto ptr = const_cast<std::remove_cvref_t<U>*>(std::addressof(value));
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
    ASSERT_WITH_SECURITY_IMPLICATION(size() == capacity());

    bool success = expandCapacity<action>(size() + 1);
    if constexpr (action == FailureAction::Report) {
        if (UNLIKELY(!success))
            return false;
    }
    UNUSED_PARAM(success);
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
ALWAYS_INLINE void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::unsafeAppendWithoutCapacityCheck(U&& value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(size() < capacity());

    asanBufferSizeWillChangeTo(m_size + 1);

    new (NotNull, end()) T(std::forward<U>(value));
    ++m_size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U, size_t otherCapacity, typename OtherOverflowHandler, size_t otherMinCapacity, typename OtherMalloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendVector(const Vector<U, otherCapacity, OtherOverflowHandler, otherMinCapacity, OtherMalloc>& val)
{
    append(val.span());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U, size_t otherCapacity, typename OtherOverflowHandler, size_t otherMinCapacity, typename OtherMalloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendVector(Vector<U, otherCapacity, OtherOverflowHandler, otherMinCapacity, OtherMalloc>&& val)
{
    size_t newSize = m_size + val.size();
    if (newSize > capacity())
        expandCapacity<FailureAction::Crash>(newSize);
    for (auto& item : val)
        unsafeAppendWithoutCapacityCheck(WTFMove(item));
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

    auto ptr = const_cast<std::remove_cvref_t<U>*>(std::addressof(value));
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
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insertFill(size_t position, const T& data, size_t dataSize)
{
    ASSERT_WITH_SECURITY_IMPLICATION(position <= size());
    size_t newSize = m_size + dataSize;
    if (newSize > capacity()) {
        expandCapacity<FailureAction::Crash>(newSize);
        ASSERT(begin());
    }
    if (newSize < m_size)
        CRASH();
    asanBufferSizeWillChangeTo(newSize);
    T* spot = begin() + position;
    TypeOperations::moveOverlapping(spot, end(), spot + dataSize);
    TypeOperations::uninitializedFill(spot, spot + dataSize, data);
    m_size = newSize;
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
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeLast(const U& value)
{
    return removeLastMatching([&value] (const T& current) {
        return current == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename MatchFunction>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeLastMatching(const MatchFunction& matches)
{
    return removeLastMatching(matches, size());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename MatchFunction>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeLastMatching(const MatchFunction& matches, size_t startIndex)
{
    for (size_t i = std::min(startIndex + 1, size()); i > 0; --i) {
        if (matches(at(i - 1))) {
            remove(i - 1);
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
template<typename ResultVector, typename MapFunction>
inline auto Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::map(MapFunction&& mapFunction) const -> std::enable_if_t<std::is_invocable_v<MapFunction, const T&>, ResultVector>
{
    ResultVector result;
    result.reserveInitialCapacity(size());
    for (size_t i = 0; i < size(); ++i)
        result.unsafeAppendWithoutCapacityCheck(mapFunction(at(i)));
    return result;
}

template <typename ContainerType>
size_t containerSize(const ContainerType& container) { return std::size(container); }

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename MapFunction>
inline auto Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::map(MapFunction&& mapFunction) const -> std::enable_if_t<std::is_invocable_v<MapFunction, const T&>, Vector<typename std::invoke_result_t<MapFunction, const T&>>>
{
    return map<Vector<typename std::invoke_result_t<MapFunction, const T&>>, MapFunction>(std::forward<MapFunction>(mapFunction));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename ContainerType, typename MapFunction>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendContainerWithMapping(ContainerType&& container, const MapFunction& mapFunction)
{
    reserveCapacity(size() + containerSize(container));
    for (auto&& item : container)
        unsafeAppendWithoutCapacityCheck(mapFunction(std::forward<decltype(item)>(item)));
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
#if ENABLE(SECURITY_ASSERTIONS)
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

#if ENABLE(SECURITY_ASSERTIONS)
template<typename T> struct ValueCheck<Vector<T>> {
    typedef Vector<T> TraitType;
    static void checkConsistency(const Vector<T>& v)
    {
        v.checkConsistency();
    }
};
#endif // ENABLE(SECURITY_ASSERTIONS)

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

template<typename MapFunction, typename DestinationVectorType, typename SourceType, typename Enable = void>
struct Mapper {
    static void map(DestinationVectorType& result, const SourceType& source, const MapFunction& mapFunction)
    {
        result.reserveInitialCapacity(containerSize(source));
        for (auto&& item : source)
            result.unsafeAppendWithoutCapacityCheck(mapFunction(item));
    }
};

template<typename MapFunction, typename DestinationVectorType, typename SourceType>
struct Mapper<MapFunction, DestinationVectorType, SourceType, typename std::enable_if<std::is_rvalue_reference<SourceType&&>::value>::type> {
    static void map(DestinationVectorType& result, SourceType&& source, const MapFunction& mapFunction)
    {
        result.reserveInitialCapacity(containerSize(source));
        for (auto&& item : source)
            result.unsafeAppendWithoutCapacityCheck(mapFunction(WTFMove(item)));
    }
};

template<size_t inlineCapacity = 0, typename OverflowHandler = CrashOnOverflow, size_t minCapacity = 16, typename MapFunction, typename SourceType>
Vector<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&&>::type, inlineCapacity, OverflowHandler, minCapacity> map(SourceType&& source, MapFunction&& mapFunction)
{
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename std::invoke_result<MapFunction, SourceItemType&&>::type;
    using DestinationVectorType = Vector<DestinationItemType, inlineCapacity, OverflowHandler, minCapacity>;

    DestinationVectorType result;
    Mapper<MapFunction, DestinationVectorType, SourceType>::map(result, std::forward<SourceType>(source), std::forward<MapFunction>(mapFunction));
    return result;
}

template<size_t inlineCapacity = 0, typename OverflowHandler = CrashOnOverflow, size_t minCapacity = 16, typename MapFunction, typename SourceType>
Vector<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&>::type, inlineCapacity, OverflowHandler, minCapacity> map(SourceType& source, MapFunction&& mapFunction)
{
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename std::invoke_result<MapFunction, SourceItemType&>::type;
    using DestinationVectorType = Vector<DestinationItemType, inlineCapacity, OverflowHandler, minCapacity>;

    DestinationVectorType result;
    Mapper<MapFunction, DestinationVectorType, SourceType&>::map(result, source, std::forward<MapFunction>(mapFunction));
    return result;
}

template<typename MapFunctionReturnType>
struct CompactMapTraits {
    static bool hasValue(const MapFunctionReturnType&);
    template<typename ItemType>
    static ItemType extractValue(MapFunctionReturnType&&);
};

template<typename T>
struct CompactMapTraits<std::optional<T>> {
    using ItemType = T;
    static bool hasValue(const std::optional<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(std::optional<T>&& returnValue) { return WTFMove(*returnValue); }
};

template<typename T>
struct CompactMapTraits<RefPtr<T>> {
    using ItemType = Ref<T>;
    static bool hasValue(const RefPtr<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(RefPtr<T>&& returnValue) { return returnValue.releaseNonNull(); }
};

template<typename T>
struct CompactMapTraits<RetainPtr<T>> {
    using ItemType = RetainPtr<T>;
    static bool hasValue(const RetainPtr<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(RetainPtr<T>&& returnValue) { return WTFMove(returnValue); }
};

template<typename MapFunction, typename DestinationVectorType, typename SourceType, typename Enable = void>
struct CompactMapper {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::invoke_result<MapFunction, SourceItemType&>::type;

    static void compactMap(DestinationVectorType& result, const SourceType& source, const MapFunction& mapFunction)
    {
        for (auto&& item : source) {
            auto itemResult = mapFunction(item);
            if (CompactMapTraits<ResultItemType>::hasValue(itemResult))
                result.append(CompactMapTraits<ResultItemType>::extractValue(WTFMove(itemResult)));
        }
        result.shrinkToFit();
    }
};

template<typename MapFunction, typename DestinationVectorType, typename SourceType>
struct CompactMapper<MapFunction, DestinationVectorType, SourceType, typename std::enable_if<std::is_rvalue_reference<SourceType&&>::value>::type> {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::invoke_result<MapFunction, SourceItemType&&>::type;

    static void compactMap(DestinationVectorType& result, SourceType&& source, const MapFunction& mapFunction)
    {
        for (auto&& item : source) {
            auto itemResult = mapFunction(WTFMove(item));
            if (CompactMapTraits<ResultItemType>::hasValue(itemResult))
                result.unsafeAppendWithoutCapacityCheck(CompactMapTraits<ResultItemType>::extractValue(WTFMove(itemResult)));
        }
        result.shrinkToFit();
    }
};

template<size_t inlineCapacity = 0, typename OverflowHandler = CrashOnOverflow, size_t minCapacity = 16, typename MapFunction, typename SourceType>
Vector<typename CompactMapTraits<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&&>::type>::ItemType, inlineCapacity, OverflowHandler, minCapacity> compactMap(SourceType&& source, MapFunction&& mapFunction)
{
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::invoke_result<MapFunction, SourceItemType&&>::type;
    using DestinationItemType = typename CompactMapTraits<ResultItemType>::ItemType;
    using DestinationVectorType = Vector<DestinationItemType, inlineCapacity, OverflowHandler, minCapacity>;

    DestinationVectorType result;
    result.reserveInitialCapacity(containerSize(source));
    CompactMapper<MapFunction, DestinationVectorType, SourceType>::compactMap(result, std::forward<SourceType>(source), std::forward<MapFunction>(mapFunction));
    return result;
}

template<size_t inlineCapacity = 0, typename OverflowHandler = CrashOnOverflow, size_t minCapacity = 16, typename MapFunction, typename SourceType>
Vector<typename CompactMapTraits<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&>::type>::ItemType, inlineCapacity, OverflowHandler, minCapacity> compactMap(SourceType& source, MapFunction&& mapFunction)
{
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::invoke_result<MapFunction, SourceItemType&>::type;
    using DestinationItemType = typename CompactMapTraits<ResultItemType>::ItemType;
    using DestinationVectorType = Vector<DestinationItemType, inlineCapacity, OverflowHandler, minCapacity>;

    DestinationVectorType result;
    result.reserveInitialCapacity(containerSize(source));
    CompactMapper<MapFunction, DestinationVectorType, SourceType&>::compactMap(result, source, std::forward<MapFunction>(mapFunction));
    return result;
}

template<typename MapFunction, typename SourceType>
struct FlatMapper {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename CollectionInspector<typename std::invoke_result<MapFunction, SourceItemType&>::type>::SourceItemType;

    static Vector<DestinationItemType> flatMap(const SourceType& source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        for (auto&& item : source)
            result.appendVector(mapFunction(item));
        result.shrinkToFit();
        return result;
    }
};

template<typename MapFunction, typename SourceType> requires std::is_rvalue_reference<SourceType&&>::value
struct FlatMapper<MapFunction, SourceType> {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename CollectionInspector<typename std::invoke_result<MapFunction, SourceItemType&&>::type>::SourceItemType;

    static Vector<DestinationItemType> flatMap(SourceType&& source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        for (auto&& item : source)
            result.appendVector(mapFunction(WTFMove(item)));
        result.shrinkToFit();
        return result;
    }
};

template<typename MapFunction, typename SourceType>
Vector<typename FlatMapper<MapFunction, SourceType>::DestinationItemType> flatMap(SourceType&& source, MapFunction&& mapFunction)
{
    return FlatMapper<MapFunction, SourceType>::flatMap(std::forward<SourceType>(source), std::forward<MapFunction>(mapFunction));
}

template<typename DestinationVector, typename Collection>
inline auto copyToVectorSpecialization(const Collection& collection) -> DestinationVector
{
    DestinationVector result;
    // FIXME: Use std::size when available on all compilers.
    result.reserveInitialCapacity(collection.size());
    for (auto&& item : collection)
        result.unsafeAppendWithoutCapacityCheck(item);
    return result;
}

template<typename DestinationItemType, typename Collection>
inline auto copyToVectorOf(const Collection& collection) -> Vector<DestinationItemType>
{
    return WTF::map(collection, [] (auto&& v) -> DestinationItemType {
        return v;
    });
}

template<typename Collection>
struct CopyOrMoveToVectorResult {
    using Type = typename std::remove_cv<typename CollectionInspector<Collection>::SourceItemType>::type;
};

template<typename Collection>
inline Vector<typename CopyOrMoveToVectorResult<Collection>::Type> copyToVector(const Collection& collection)
{
    return copyToVectorOf<typename CopyOrMoveToVectorResult<Collection>::Type>(collection);
}

template<typename DestinationItemType, typename Collection>
inline auto moveToVectorOf(Collection&& collection) -> Vector<DestinationItemType>
{
    return WTF::map(collection, [] (auto&& item) -> DestinationItemType {
        return std::forward<DestinationItemType>(item);
    });
}

template<typename Collection>
inline Vector<typename CopyOrMoveToVectorResult<Collection>::Type> moveToVector(Collection&& collection)
{
    return moveToVectorOf<typename CopyOrMoveToVectorResult<Collection>::Type>(collection);
}

template<typename T> Vector(const T*, size_t) -> Vector<T>;
template<typename T, size_t Extent> Vector(std::span<const T, Extent>) -> Vector<T>;

} // namespace WTF

using WTF::UnsafeVectorOverflow;
using WTF::Vector;
using WTF::copyToVector;
using WTF::copyToVectorOf;
using WTF::copyToVectorSpecialization;
using WTF::compactMap;
using WTF::flatMap;
using WTF::removeRepeatedElements;
