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

#include <concepts>
#include <initializer_list>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <string.h>
#include <type_traits>
#include <utility>
#include <wtf/AlignedStorage.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FailureAction.h>
#include <wtf/FastMalloc.h>
#include <wtf/Forward.h>
#include <wtf/MallocSpan.h>
#include <wtf/MathExtras.h>
#include <wtf/Noncopyable.h>
#include <wtf/NotFound.h>
#include <wtf/RangeAdaptors.h>
#include <wtf/StdLibExtras.h>
#include <wtf/SwiftBridging.h>
#include <wtf/ValueCheck.h>
#include <wtf/VectorTraits.h>

#if ASAN_ENABLED && __has_include(<sanitizer/asan_interface.h>)
#include <sanitizer/asan_interface.h>
#endif

namespace JSC {
class LLIntOffsetsExtractor;
}

namespace WTF {
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(Vector, WTF_EXPORT_PRIVATE);
DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER_AND_EXPORT(VectorBuffer, WTF_EXPORT_PRIVATE);

enum class NulloptBehavior : bool { Ignore, Abort };

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

template<typename T>
struct VectorCopier {
    template<typename U, std::size_t Extent>
    static void uninitializedCopy(std::span<const U, Extent> src, std::span<T> dst)
    {
        if constexpr (std::is_trivial_v<T> && std::same_as<T, U>)
            memcpySpan(dst, src);
        else {
            for (size_t i = 0; i < src.size(); ++i)
                new (NotNull, &dst[i]) T(src[i]);
        }
    }
};

template<typename T>
struct VectorTypeOperations
{
    static void destruct(T* begin, T* end)
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (T* current = begin; current != end; ++current)
                current->~T();
        }
    }

    static void destruct(std::span<T> span)
    {
        destruct(std::to_address(span.begin()), std::to_address(span.end()));
    }

    static void initializeIfNonPOD(T* begin, T* end)
    {
        if constexpr (VectorTraits<T>::needsInitialization)
            initialize(begin, end);
    }

    static void initialize(T* begin, T* end)
    {
        if constexpr (VectorTraits<T>::canInitializeWithMemset)
            memset(static_cast<void*>(begin), 0, reinterpret_cast<char*>(end) - reinterpret_cast<char*>(begin));
        else {
            for (T* current = begin; current != end; ++current)
                new (NotNull, current) T();
        }
    }

    template<typename... Args>
    static void initializeWithArgs(T* begin, T* end, Args&&... args)
    {
        static_assert(VectorTraits<T>::needsInitialization);
        static_assert(!VectorTraits<T>::canInitializeWithMemset);

        for (T *current = begin; current != end; ++current)
            new (NotNull, current) T(args...);
    }

    static void move(T* src, T* srcEnd, T* dst)
    {
        if constexpr (VectorTraits<T>::canMoveWithMemcpy)
            memcpy(static_cast<void*>(dst), static_cast<void*>(const_cast<T*>(src)), reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
        else {
            while (src != srcEnd) {
                new (NotNull, dst) T(WTF::move(*src));
                src->~T();
                ++dst;
                ++src;
            }
        }
    }

    static void move(std::span<T> src, std::span<T> dst)
    {
        move(std::to_address(src.begin()), std::to_address(src.end()), std::to_address(dst.begin()));
    }

    static void moveOverlapping(T* src, T* srcEnd, T* dst)
    {
        if constexpr (VectorTraits<T>::canMoveWithMemcpy)
            memmove(static_cast<void*>(dst), static_cast<void*>(const_cast<T*>(src)), reinterpret_cast<const char*>(srcEnd) - reinterpret_cast<const char*>(src));
        else {
            if (src > dst)
                move(src, srcEnd, dst);
            else {
                T* dstEnd = dst + (srcEnd - src);
                while (src != srcEnd) {
                    --srcEnd;
                    --dstEnd;
                    new (NotNull, dstEnd) T(WTF::move(*srcEnd));
                    srcEnd->~T();
                }
            }
        }
    }

    static void moveOverlapping(std::span<T> src, std::span<T> dst)
    {
        moveOverlapping(std::to_address(src.begin()), std::to_address(src.end()), std::to_address(dst.begin()));
    }

    template<std::size_t Extent>
    static void uninitializedCopy(std::span<const T, Extent> src, std::span<T> dst)
    {
        if constexpr (VectorTraits<T>::canCopyWithMemcpy)
            memcpySpan(asMutableByteSpan(dst), asByteSpan(src));
        else {
            for (size_t i = 0; i < src.size(); ++i)
                new (NotNull, &dst[i]) T(src[i]);
        }
    }

    static void uninitializedFill(T* dst, T* dstEnd, const T& val)
    {
        if constexpr (VectorTraits<T>::canFillWithMemset) {
            static_assert(sizeof(T) == 1);
            memset(dst, val, dstEnd - dst);
        } else {
            while (dst != dstEnd) {
                new (NotNull, dst) T(val);
                ++dst;
            }
        }
    }
    
    static bool compare(const T* a, const T* b, size_t size)
    {
        if constexpr (VectorTraits<T>::canCompareWithMemcmp)
            return !memcmp(a, b, sizeof(T) * size);
        else {
            for (size_t i = 0; i < size; ++i) {
                if (!(a[i] == b[i]))
                    return false;
            }
            return true;
        }
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
            if (!newBuffer) [[unlikely]]
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

    T* buffer() LIFETIME_BOUND { return m_buffer; }
    const T* buffer() const LIFETIME_BOUND { return m_buffer; }
    static constexpr ptrdiff_t bufferMemoryOffset() { return OBJECT_OFFSETOF(VectorBufferBase, m_buffer); }
    size_t capacity() const { return m_capacity; }

    std::span<T> capacitySpan() { return unsafeMakeSpan(m_buffer, m_capacity); }
    std::span<const T> capacitySpan() const { return unsafeMakeSpan(m_buffer, m_capacity); }

    MallocSpan<T, Malloc> releaseBuffer()
    {
        m_capacity = 0;
        return adoptMallocSpan<T, Malloc>(unsafeMakeSpan(std::exchange(m_buffer, nullptr), std::exchange(m_size, 0)));
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
    void* endOfBuffer() LIFETIME_BOUND
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
    using Base::capacitySpan;

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
    void* endOfBuffer() LIFETIME_BOUND
    {
        ASSERT_WITH_SECURITY_IMPLICATION(buffer());

        IGNORE_WARNINGS_BEGIN("invalid-offsetof")
        static_assert((offsetof(VectorBuffer, m_inlineBuffer) + sizeof(m_inlineBuffer)) % 8 == 0, "Inline buffer end needs to be on 8 byte boundary for ASan annotations to work.");
        IGNORE_WARNINGS_END

        if (buffer() == inlineBuffer())
            return reinterpret_cast<char*>(m_inlineBuffer) + sizeof(m_inlineBuffer);

        return buffer() + capacity();
    }
#endif

    using Base::buffer;
    using Base::capacitySpan;
    using Base::capacity;
    using Base::bufferMemoryOffset;

    MallocSpan<T, Malloc> releaseBuffer()
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

    T* inlineBuffer() LIFETIME_BOUND { SUPPRESS_MEMORY_UNSAFE_CAST return reinterpret_cast_ptr<T*>(m_inlineBuffer); }
    const T* inlineBuffer() const LIFETIME_BOUND { SUPPRESS_MEMORY_UNSAFE_CAST return reinterpret_cast_ptr<const T*>(m_inlineBuffer); }

#if ASAN_ENABLED
    // ASan needs the buffer to begin and end on 8-byte boundaries for annotations to work.
    // FIXME: Add a redzone before the buffer to catch off by one accesses. We don't need a guard after, because the buffer is the last member variable.
    static constexpr size_t asanInlineBufferAlignment = std::alignment_of<T>::value >= 8 ? std::alignment_of<T>::value : 8;
    static constexpr size_t asanAdjustedInlineCapacity = ((sizeof(T) * inlineCapacity + 7) & ~7) / sizeof(T);
    AlignedStorage<T, asanInlineBufferAlignment> m_inlineBuffer[asanAdjustedInlineCapacity];
#else
    AlignedStorage<T> m_inlineBuffer[inlineCapacity];
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
    WTF_MAKE_CONFIGURABLE_ALLOCATED_WITH_HEAP_IDENTIFIER(Vector, FastMalloc);
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

    template<typename Range>
        requires std::ranges::input_range<Range> && std::convertible_to<std::ranges::range_value_t<Range>, T>
    explicit Vector(FromRange, Range&& range)
    {
        if constexpr (std::ranges::sized_range<Range>)
            reserveInitialCapacity(std::ranges::size(range));
        for (auto&& item : range) {
            if constexpr (std::is_rvalue_reference_v<Range&&>)
                append(WTF::move(item));
            else
                append(item);
        }
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

    template<std::invocable<size_t> Functor>
    requires (!std::is_same_v<std::invoke_result_t<Functor, size_t>, std::optional<T>>)
    Vector(size_t size, NOESCAPE const Functor& valueGenerator)
    {
        reserveInitialCapacity(size);

        asanSetInitialBufferSizeTo(size);

        for (size_t i = 0; i < size; ++i)
            unsafeAppendWithoutCapacityCheck(valueGenerator(i));
    }

    template<std::invocable<size_t> Functor>
    requires (std::is_same_v<std::invoke_result_t<Functor, size_t>, std::optional<T>>)
    Vector(size_t size, NOESCAPE const Functor& valueGenerator, NulloptBehavior nulloptBehavior = NulloptBehavior::Ignore)
    {
        reserveInitialCapacity(size);

        asanSetInitialBufferSizeTo(size);

        for (size_t i = 0; i < size; ++i) {
            if (auto item = valueGenerator(i))
                unsafeAppendWithoutCapacityCheck(WTF::move(*item));
            else if (nulloptBehavior == NulloptBehavior::Abort)
                break;
        }
        shrinkToFit();
    }

    template<typename U, size_t Extent>
    Vector(std::array<U, Extent> array)
        : Vector(std::span<const U, Extent> { array }) { }

    template<typename U, size_t Extent> Vector(std::span<U, Extent> span)
        : Base(span.size(), span.size())
    {
        asanSetInitialBufferSizeTo(span.size());

        if (begin())
            VectorCopier<T>::uninitializedCopy(spanConstCast<const U>(span), mutableSpan());
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

    [[nodiscard]] size_t size() const { return m_size; }
    [[nodiscard]] size_t sizeInBytes() const { return static_cast<size_t>(m_size) * sizeof(T); }
    static constexpr ptrdiff_t sizeMemoryOffset() { return OBJECT_OFFSETOF(Vector, m_size); }
    [[nodiscard]] size_t capacity() const { return Base::capacity(); }
    [[nodiscard]] bool isEmpty() const { return !size(); }
    [[nodiscard]] std::span<const T> span() const LIFETIME_BOUND { return { data(), size() }; }
    [[nodiscard]] std::span<T> mutableSpan() LIFETIME_BOUND { return { data(), size() }; }

    Vector<T> subvector(size_t offset, size_t length = std::dynamic_extent) const
    {
        return { span().subspan(offset, length) };
    }

    std::span<const T> subspan(size_t offset, size_t length = std::dynamic_extent) const LIFETIME_BOUND
    {
        return span().subspan(offset, length);
    }

    [[nodiscard]] T& at(size_t i) LIFETIME_BOUND
    {
        if (i >= size()) [[unlikely]]
            OverflowHandler::overflowed();
        return Base::buffer()[i];
    }
    [[nodiscard]] const T& at(size_t i) const LIFETIME_BOUND
    {
        if (i >= size()) [[unlikely]]
            OverflowHandler::overflowed();
        return Base::buffer()[i];
    }

    [[nodiscard]] T& operator[](size_t i) LIFETIME_BOUND { return at(i); }
    [[nodiscard]] const T& operator[](size_t i) const LIFETIME_BOUND { return at(i); }

    static constexpr ptrdiff_t dataMemoryOffset() { return Base::bufferMemoryOffset(); }

    [[nodiscard]] iterator begin() LIFETIME_BOUND { return data(); }
    [[nodiscard]] iterator end() LIFETIME_BOUND { return begin() + m_size; }
    [[nodiscard]] const_iterator begin() const LIFETIME_BOUND { return data(); }
    [[nodiscard]] const_iterator end() const LIFETIME_BOUND { return begin() + m_size; }

    [[nodiscard]] reverse_iterator rbegin() LIFETIME_BOUND { return reverse_iterator(end()); }
    [[nodiscard]] reverse_iterator rend() LIFETIME_BOUND { return reverse_iterator(begin()); }
    [[nodiscard]] const_reverse_iterator rbegin() const LIFETIME_BOUND { return const_reverse_iterator(end()); }
    [[nodiscard]] const_reverse_iterator rend() const LIFETIME_BOUND { return const_reverse_iterator(begin()); }

    [[nodiscard]] T& first() LIFETIME_BOUND { return at(0); }
    [[nodiscard]] const T& first() const LIFETIME_BOUND { return at(0); }
    [[nodiscard]] T& last() LIFETIME_BOUND { return at(size() - 1); }
    [[nodiscard]] const T& last() const LIFETIME_BOUND { return at(size() - 1); }
    
    T takeLast()
    {
        T result = WTF::move(last());
        removeLast();
        return result;
    }

    bool contains(const auto&) const;
    bool containsIf(NOESCAPE const Invocable<bool(const T&)> auto&) const;
    size_t find(const auto&) const;
    size_t findIf(NOESCAPE const Invocable<bool(const T&)> auto&) const;
    size_t reverseFind(const auto&) const;
    size_t reverseFindIf(NOESCAPE const Invocable<bool(const T&)> auto&) const;

    // Overloads for smart pointer element types that take raw pointer parameters.
    template<SmartPtr U = T, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
    bool contains(V*) const;
    template<SmartPtr U = T, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
    size_t find(V*) const;
    template<SmartPtr U = T, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
    size_t reverseFind(V*) const;

    bool appendIfNotContains(const auto&);

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

    void appendUsingFunctor(size_t, NOESCAPE const Invocable<T(size_t)> auto&);

    void insert(size_t position, value_type&& value) { insert<value_type>(position, std::forward<value_type>(value)); }
    void insertFill(size_t position, const T& value, size_t dataSize);
    template<typename U, std::size_t Extent = std::dynamic_extent> void insertSpan(size_t position, std::span<U, Extent>);
    template<typename U> void insert(size_t position, U&&);
    template<typename U, size_t c, typename OH, size_t m, typename M> void insertVector(size_t position, const Vector<U, c, OH, m, M>&);

    void removeAt(size_t position);
    void removeAt(size_t position, size_t length);
    bool removeFirst(const auto&);
    template<SmartPtr U = T, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
    bool removeFirst(V*);
    bool removeFirstMatching(NOESCAPE const Invocable<bool(T&)> auto&, size_t startIndex = 0);
    bool removeLast(const auto&);
    bool removeLastMatching(NOESCAPE const Invocable<bool(T&)> auto&);
    bool removeLastMatching(NOESCAPE const Invocable<bool(T&)> auto&, size_t startIndex);
    unsigned removeAll(const auto&);
    unsigned removeAllMatching(NOESCAPE const Invocable<bool(T&)> auto&, size_t startIndex = 0);

    void removeLast()
    {
        if (isEmpty()) [[unlikely]]
            OverflowHandler::overflowed();
        shrink(size() - 1); 
    }

    void fill(const T&, size_t);
    void fill(const T& val) { fill(val, size()); }

    template<typename Iterator> void appendRange(Iterator start, Iterator end);
    template<typename ContainerType, typename MapFunction> void appendContainerWithMapping(ContainerType&&, NOESCAPE const MapFunction&);

    MallocSpan<T, Malloc> releaseBuffer();

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

    template<typename ResultVector>
    ResultVector map(NOESCAPE const std::invocable<const T&> auto& mapFunction) const;

    template<std::invocable<const T&> MapFunction>
    Vector<std::invoke_result_t<MapFunction, const T&>> map(NOESCAPE const MapFunction&) const;

    bool isHashTableDeletedValue() const { return m_size == std::numeric_limits<decltype(m_size)>::max(); }
    static constexpr bool safeToCompareToHashTableEmptyOrDeletedValue = true;

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

    template<typename MapFunction, typename DestinationVectorType, typename SourceType> friend struct Mapper;
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

    [[nodiscard]] T* data() LIFETIME_BOUND { return Base::buffer(); }
    [[nodiscard]] const T* data() const LIFETIME_BOUND { return Base::buffer(); }

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
} SWIFT_ESCAPABLE_IF(T);

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(const Vector& other)
    : Base(other.size(), other.size())
{
    asanSetInitialBufferSizeTo(other.size());

    if (begin())
        TypeOperations::uninitializedCopy(other.span(), mutableSpan());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<size_t otherCapacity, typename otherOverflowBehaviour, size_t otherMinimumCapacity, typename OtherMalloc>
Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(const Vector<T, otherCapacity, otherOverflowBehaviour, otherMinimumCapacity, OtherMalloc>& other)
    : Base(other.size(), other.size())
{
    asanSetInitialBufferSizeTo(other.size());

    if (begin())
        TypeOperations::uninitializedCopy(other.span(), mutableSpan());
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
    auto oldSize = std::exchange(m_size, other.size());
    TypeOperations::uninitializedCopy(other.span().subspan(oldSize), mutableSpan().subspan(oldSize));

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
    auto oldSize = std::exchange(m_size, other.size());
    TypeOperations::uninitializedCopy(other.span().subspan(oldSize), mutableSpan().subspan(oldSize));

    return *this;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::Vector(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& other)
{
    // Make it possible to copy inline buffer.
    asanSetBufferSizeToFullCapacity();
    other.asanSetBufferSizeToFullCapacity();

    Base::adopt(std::forward<decltype(other)>(other));

    asanSetInitialBufferSizeTo(m_size);
    other.asanSetInitialBufferSizeTo(other.m_size);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>& Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::operator=(Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>&& other)
{
    if (m_size)
        VectorTypeOperations<T>::destruct(begin(), end());

    // Make it possible to copy inline buffer.
    asanSetBufferSizeToFullCapacity();
    other.asanSetBufferSizeToFullCapacity();

    Base::adopt(std::forward<decltype(other)>(other));

    asanSetInitialBufferSizeTo(m_size);
    other.asanSetInitialBufferSizeTo(other.m_size);

    return *this;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::containsIf(NOESCAPE const Invocable<bool(const T&)> auto& matches) const
{
    return findIf(matches) != notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::contains(const auto& value) const
{
    return find(value) != notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::findIf(NOESCAPE const Invocable<bool(const T&)> auto& matches) const
{
    for (size_t i = 0; i < size(); ++i) {
        if (matches(at(i)))
            return i;
    }
    return notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::find(const auto& value) const
{
    return findIf([&](auto& item) {
        return item == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reverseFindIf(NOESCAPE const Invocable<bool(const T&)> auto& matches) const
{
    for (size_t i = 1; i <= size(); ++i) {
        const size_t index = size() - i;
        if (matches(at(index)))
            return index;
    }
    return notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reverseFind(const auto& value) const
{
    return reverseFindIf([&](auto& item) {
        return item == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendIfNotContains(const auto& value)
{
    if (contains(value))
        return false;
    append(value);
    return true;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<SmartPtr U, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::contains(V* ptr) const
{
    return find(ptr) != notFound;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<SmartPtr U, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::find(V* ptr) const
{
    return findIf([&](auto& item) {
        return GetPtrHelper<U>::getPtr(item) == ptr;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<SmartPtr U, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
size_t Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reverseFind(V* ptr) const
{
    return reverseFindIf([&](auto& item) {
        return GetPtrHelper<U>::getPtr(item) == ptr;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<SmartPtr U, typename V> requires std::same_as<U, T> && std::derived_from<V, typename GetPtrHelper<U>::UnderlyingType>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeFirst(V* ptr)
{
    return removeFirstMatching([&](auto& item) {
        return GetPtrHelper<U>::getPtr(item) == ptr;
    });
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

    std::ranges::fill(*this, val);
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
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendUsingFunctor(size_t size, NOESCAPE const Invocable<T(size_t)> auto& valueGenerator)
{
    reserveCapacity(this->size() + size);
    for (size_t i = 0; i < size; ++i)
        unsafeAppendWithoutCapacityCheck(valueGenerator(i));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action>
bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::expandCapacity(size_t newMinCapacity)
{
    return reserveCapacity<action>(std::max(newMinCapacity, std::max(static_cast<size_t>(minCapacity), Malloc::nextCapacity(capacity()))));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<FailureAction action>
NEVER_INLINE T* Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::expandCapacity(size_t newMinCapacity, T* ptr)
{
    static_assert(action == FailureAction::Crash || action == FailureAction::Report);
    if (ptr < begin() || ptr >= end()) {
        bool success = expandCapacity<action>(newMinCapacity);
        if constexpr (action == FailureAction::Report) {
            if (!success) [[unlikely]]
                return nullptr;
        }
        UNUSED_PARAM(success);
        return ptr;
    }
    size_t index = ptr - begin();
    bool success = expandCapacity<action>(newMinCapacity);
    if constexpr (action == FailureAction::Report) {
        if (!success) [[unlikely]]
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
        if (!success) [[unlikely]]
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
    TypeOperations::destruct(mutableSpan().subspan(size).data(), end());
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
            if (!success) [[unlikely]]
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
        if (!success) [[unlikely]] {
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
            if (!dataPtr) [[unlikely]]
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
    auto oldSize = std::exchange(m_size, newSize);
    VectorCopier<T>::uninitializedCopy(unsafeMakeSpan(dataPtr, dataSize), mutableSpan().subspan(oldSize));
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
    auto oldSize = std::exchange(m_size, newSize);
    VectorCopier<T>::uninitializedCopy(unsafeMakeSpan(data, dataSize), mutableSpan().subspan(oldSize));
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
        if (!ptr) [[unlikely]]
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
        if (!success) [[unlikely]]
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
        unsafeAppendWithoutCapacityCheck(WTF::move(item));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U, std::size_t Extent>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insertSpan(size_t position, std::span<U, Extent> data)
{
    size_t newSize = m_size + data.size();
    if (newSize > capacity()) {
        data = unsafeMakeSpan<U, Extent>(expandCapacity<FailureAction::Crash>(newSize, data.data()), data.size());
        ASSERT(begin());
    }
    if (newSize < m_size)
        CRASH();
    asanBufferSizeWillChangeTo(newSize);
    auto oldSize = std::exchange(m_size, newSize);
    auto spot = mutableSpan().subspan(position);
    TypeOperations::moveOverlapping(spot.data(), spot.data() + oldSize - position, spot.data() + data.size());
    VectorCopier<T>::uninitializedCopy(spanConstCast<const U>(data), spot);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insert(size_t position, U&& value)
{
    auto ptr = const_cast<std::remove_cvref_t<U>*>(std::addressof(value));
    if (size() == capacity()) {
        ptr = expandCapacity<FailureAction::Crash>(size() + 1, ptr);
        ASSERT(begin());
    }

    asanBufferSizeWillChangeTo(m_size + 1);

    T* spot = mutableSpan().subspan(position).data();
    TypeOperations::moveOverlapping(spot, end(), spot + 1);
    new (NotNull, spot) T(std::forward<U>(*ptr));
    ++m_size;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insertFill(size_t position, const T& data, size_t dataSize)
{
    size_t newSize = m_size + dataSize;
    if (newSize > capacity()) {
        expandCapacity<FailureAction::Crash>(newSize);
        ASSERT(begin());
    }
    if (newSize < m_size)
        CRASH();
    asanBufferSizeWillChangeTo(newSize);
    T* spot = mutableSpan().subspan(position).data();
    TypeOperations::moveOverlapping(spot, end(), spot + dataSize);
    TypeOperations::uninitializedFill(spot, spot + dataSize, data);
    m_size = newSize;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename U, size_t c, typename OH, size_t m, typename M>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::insertVector(size_t position, const Vector<U, c, OH, m, M>& val)
{
    insertSpan(position, val.span());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeAt(size_t position)
{
    removeAt(position, 1);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeAt(size_t position, size_t length)
{
    auto beginSpot = mutableSpan().subspan(position);
    T* endSpot = beginSpot.subspan(length).data();
    TypeOperations::destruct(beginSpot.data(), endSpot);
    TypeOperations::moveOverlapping(endSpot, end(), beginSpot.data());
    asanBufferSizeWillChangeTo(m_size - length);
    m_size -= length;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeFirst(const auto& value)
{
    return removeFirstMatching([&value] (const T& current) {
        return current == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeFirstMatching(NOESCAPE const Invocable<bool(T&)> auto& matches, size_t startIndex)
{
    for (size_t i = startIndex; i < size(); ++i) {
        if (matches(at(i))) {
            removeAt(i);
            return true;
        }
    }
    return false;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeLast(const auto& value)
{
    return removeLastMatching([&value] (const T& current) {
        return current == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeLastMatching(NOESCAPE const Invocable<bool(T&)> auto& matches)
{
    return removeLastMatching(matches, size());
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline bool Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeLastMatching(NOESCAPE const Invocable<bool(T&)> auto& matches, size_t startIndex)
{
    for (size_t i = std::min(startIndex + 1, size()); i > 0; --i) {
        if (matches(at(i - 1))) {
            removeAt(i - 1);
            return true;
        }
    }
    return false;
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline unsigned Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeAll(const auto& value)
{
    return removeAllMatching([&value] (const T& current) {
        return current == value;
    });
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline unsigned Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::removeAllMatching(NOESCAPE const Invocable<bool(T&)> auto& matches, size_t startIndex)
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::reverse()
{
    for (size_t i = 0; i < m_size / 2; ++i)
        std::swap(at(i), at(m_size - 1 - i));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename ResultVector>
inline ResultVector Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::map(NOESCAPE const std::invocable<const T&> auto& mapFunction) const
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
template<std::invocable<const T&> MapFunction>
inline Vector<std::invoke_result_t<MapFunction, const T&>> Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::map(NOESCAPE const MapFunction& mapFunction) const
{
    return map<Vector<typename std::invoke_result_t<MapFunction, const T&>>, MapFunction>(mapFunction);
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
template<typename ContainerType, typename MapFunction>
void Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::appendContainerWithMapping(ContainerType&& container, NOESCAPE const MapFunction& mapFunction)
{
    reserveCapacity(size() + containerSize(container));
    for (auto&& item : container)
        unsafeAppendWithoutCapacityCheck(mapFunction(std::forward<decltype(item)>(item)));
}

template<typename T, size_t inlineCapacity, typename OverflowHandler, size_t minCapacity, typename Malloc>
inline MallocSpan<T, Malloc> Vector<T, inlineCapacity, OverflowHandler, minCapacity, Malloc>::releaseBuffer()
{
    // FIXME: Find a way to preserve annotations on the returned buffer.
    // ASan requires that all annotations are removed before deallocation,
    // and MallocSpan doesn't implement that.
    asanSetBufferSizeToFullCapacity();

    auto buffer = Base::releaseBuffer();
    if (inlineCapacity && buffer.span().empty() && m_size) {
        // If the vector had some data, but no buffer to release,
        // that means it was using the inline buffer. In that case,
        // we create a brand new buffer so the caller always gets one.
        buffer = MallocSpan<T, Malloc>::malloc(m_size * sizeof(T));
        memcpySpan(buffer.mutableSpan(), span());
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

    return VectorTypeOperations<T>::compare(a.span().data(), b.span().data(), a.size());
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

template<typename MapFunction, typename DestinationVectorType, typename SourceType>
struct Mapper {
    static void map(DestinationVectorType& result, SourceType&& source, const MapFunction& mapFunction)
    {
        result.reserveInitialCapacity(containerSize(source));
        for (auto&& item : std::forward<SourceType>(source))
            result.unsafeAppendWithoutCapacityCheck(mapFunction(WTF::forward_like_preserving_const<SourceType>(item)));
    }
};

template<size_t inlineCapacity = 0, typename OverflowHandler = CrashOnOverflow, size_t minCapacity = 16, typename MapFunction, typename SourceType>
Vector<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&&>::type, inlineCapacity, OverflowHandler, minCapacity> map(SourceType&& source, NOESCAPE MapFunction&& mapFunction)
{
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename std::invoke_result<MapFunction, SourceItemType&&>::type;
    using DestinationVectorType = Vector<DestinationItemType, inlineCapacity, OverflowHandler, minCapacity>;

    DestinationVectorType result;
    Mapper<MapFunction, DestinationVectorType, SourceType>::map(result, std::forward<SourceType>(source), std::forward<MapFunction>(mapFunction));
    return result;
}

template<size_t inlineCapacity = 0, typename OverflowHandler = CrashOnOverflow, size_t minCapacity = 16, typename MapFunction, typename SourceType>
Vector<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&>::type, inlineCapacity, OverflowHandler, minCapacity> map(SourceType& source, NOESCAPE MapFunction&& mapFunction)
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
    static ItemType extractValue(std::optional<T>&& returnValue) { return WTF::move(*returnValue); }
};

template<typename T>
struct CompactMapTraits<RefPtr<T>> {
    using ItemType = Ref<T>;
    static bool hasValue(const RefPtr<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(RefPtr<T>&& returnValue) { return returnValue.releaseNonNull(); }
};

template<typename T>
struct CompactMapTraits<CheckedPtr<T>> {
    using ItemType = CheckedRef<T>;
    static bool hasValue(const CheckedPtr<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(CheckedPtr<T>&& returnValue) { return returnValue.releaseNonNull(); }
};

template<typename T>
struct CompactMapTraits<RetainPtr<T>> {
    using ItemType = RetainPtr<T>;
    static bool hasValue(const RetainPtr<T>& returnValue) { return !!returnValue; }
    static ItemType extractValue(RetainPtr<T>&& returnValue) { return WTF::move(returnValue); }
};

template<typename MapFunction, typename DestinationVectorType, typename SourceType, typename Enable = void>
struct CompactMapper {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::invoke_result<MapFunction, SourceItemType&>::type;

    static void compactMap(DestinationVectorType& result, const SourceType& source, NOESCAPE const MapFunction& mapFunction)
    {
        for (auto&& item : source) {
            auto itemResult = mapFunction(item);
            if (CompactMapTraits<ResultItemType>::hasValue(itemResult))
                result.append(CompactMapTraits<ResultItemType>::extractValue(WTF::move(itemResult)));
        }
        result.shrinkToFit();
    }
};

template<typename MapFunction, typename DestinationVectorType, typename SourceType>
    requires (std::is_rvalue_reference_v<SourceType&&>)
struct CompactMapper<MapFunction, DestinationVectorType, SourceType> {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using ResultItemType = typename std::invoke_result<MapFunction, SourceItemType&&>::type;

    static void compactMap(DestinationVectorType& result, SourceType&& source, NOESCAPE const MapFunction& mapFunction)
    {
        for (auto&& item : source) {
            auto itemResult = mapFunction(WTF::move(item));
            if (CompactMapTraits<ResultItemType>::hasValue(itemResult))
                result.unsafeAppendWithoutCapacityCheck(CompactMapTraits<ResultItemType>::extractValue(WTF::move(itemResult)));
        }
        result.shrinkToFit();
    }
};

template<size_t inlineCapacity = 0, typename OverflowHandler = CrashOnOverflow, size_t minCapacity = 16, typename MapFunction, typename SourceType>
Vector<typename CompactMapTraits<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&&>::type>::ItemType, inlineCapacity, OverflowHandler, minCapacity> compactMap(SourceType&& source, NOESCAPE MapFunction&& mapFunction)
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
Vector<typename CompactMapTraits<typename std::invoke_result<MapFunction, typename CollectionInspector<SourceType>::SourceItemType&>::type>::ItemType, inlineCapacity, OverflowHandler, minCapacity> compactMap(SourceType& source, NOESCAPE MapFunction&& mapFunction)
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

template<typename MapFunction, typename SourceType>
    requires (std::is_rvalue_reference_v<SourceType&&>)
struct FlatMapper<MapFunction, SourceType> {
    using SourceItemType = typename CollectionInspector<SourceType>::SourceItemType;
    using DestinationItemType = typename CollectionInspector<typename std::invoke_result<MapFunction, SourceItemType&&>::type>::SourceItemType;

    static Vector<DestinationItemType> flatMap(SourceType&& source, const MapFunction& mapFunction)
    {
        Vector<DestinationItemType> result;
        for (auto&& item : source)
            result.appendVector(mapFunction(WTF::move(item)));
        result.shrinkToFit();
        return result;
    }
};

template<typename MapFunction, typename SourceType>
Vector<typename FlatMapper<MapFunction, SourceType>::DestinationItemType> flatMap(SourceType&& source, NOESCAPE MapFunction&& mapFunction)
{
    return FlatMapper<MapFunction, SourceType>::flatMap(std::forward<SourceType>(source), std::forward<MapFunction>(mapFunction));
}

template<typename DestinationVector, typename Collection>
inline auto copyToVectorSpecialization(const Collection& collection) -> DestinationVector
{
    DestinationVector result;
    result.reserveInitialCapacity(std::size(collection));
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

template<typename T, size_t inlineCapacity = 0> static bool insertInUniquedSortedVector(Vector<T, inlineCapacity>& vector, const T& value)
{
    auto it = std::ranges::lower_bound(vector, value);
    if (it != vector.end() && *it == value) [[unlikely]]
        return false;
    vector.insert(it - vector.begin(), value);
    return true;
}

template<typename T> Vector(const T*, size_t) -> Vector<T>;
template<typename T, size_t Extent> Vector(std::span<const T, Extent>) -> Vector<T>;

} // namespace WTF

using WTF::NulloptBehavior;
using WTF::UnsafeVectorOverflow;
using WTF::Vector;
using WTF::copyToVector;
using WTF::copyToVectorOf;
using WTF::copyToVectorSpecialization;
using WTF::compactMap;
using WTF::flatMap;
using WTF::insertInUniquedSortedVector;
using WTF::removeRepeatedElements;
