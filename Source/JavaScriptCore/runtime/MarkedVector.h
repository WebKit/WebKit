/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2023, 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/JSCast.h>
#include <JavaScriptCore/VM.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/HashSet.h>
#include <wtf/OverflowPolicy.h>
#include <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class AssertNoGC;

namespace MarkedVectorHelper {

template<typename T>
static constexpr bool isJSCAPIValueType()
{
    if constexpr (std::is_same_v<T, JSContextRef>
        || std::is_same_v<T, JSGlobalContextRef>
        || std::is_same_v<T, JSValueRef>
        || std::is_same_v<T, JSObjectRef>)
        return true;
    return false;
}

template<typename T>
requires (isJSCAPIValueType<T>())
static constexpr bool isJSValueConvertible() { return true; }

template<typename T>
requires (!isJSCAPIValueType<T>())
static constexpr bool isJSValueConvertible()
{
    if constexpr (std::is_same_v<T, JSValue>)
        return true;
    if constexpr (std::is_pointer_v<T> && std::derived_from<std::remove_pointer_t<T>, JSCell>)
        return true;
    return false;
}

template <typename T>
concept Iterable = requires(T t) {
    t.begin();
    t.end();
    t.size();
};

} // namespace MarkedVectorHelper

class alignas(alignof(EncodedJSValue)) MarkedVectorBase {
    WTF_FORBID_HEAP_ALLOCATION;
    friend class VM;
    friend class ArgList;

protected:
    enum class Status { Success, Overflowed };
public:
    typedef UncheckedKeyHashSet<MarkedVectorBase*> ListSet;

    ~MarkedVectorBase()
    {
        removeFromMarkSetAndDeallocateBuffer();
    }

    size_t size() const { return m_size; }
    bool isEmpty() const { return !m_size; }

    void removeLast()
    {
        ASSERT(m_size);
        m_size--;
    }

    template<typename Visitor> static void markLists(Visitor&, ListSet&);

    void overflowCheckNotNeeded() { clearNeedsOverflowCheck(); }

protected:
#if CPU(ADDRESS64)
    // Constructor for a read-write list, to which you may append values.
    // FIXME: Remove all clients of this API, then remove this API.
    MarkedVectorBase(size_t inlineCapacity)
        : m_size(0)
        , m_capacity(inlineCapacity)
        , m_buffer(inlineBuffer())
        , m_markSet(nullptr)
    {
    }

#else
    enum class StorageType : uint8_t {
        JSValue,
        JSCell,
        JSContextRef,
        JSGlobalContextRef,
        JSValueRef,
        JSObjectRef
    };

    template<typename U>
    static constexpr StorageType storageType()
    {
        if constexpr (std::is_same_v<U, JSValue>)
            return StorageType::JSValue;
        if constexpr (std::is_same_v<U, JSContextRef>)
            return StorageType::JSContextRef;
        if constexpr (std::is_same_v<U, JSGlobalContextRef>)
            return StorageType::JSGlobalContextRef;
        if constexpr (std::is_same_v<U, JSValueRef>)
            return StorageType::JSValueRef;
        if constexpr (std::is_same_v<U, JSObjectRef>)
            return StorageType::JSObjectRef;
        return StorageType::JSCell;
    }

    MarkedVectorBase(size_t inlineCapacity, StorageType storageType)
        : m_size(0)
        , m_capacity(inlineCapacity)
        , m_buffer(inlineBuffer())
        , m_markSet(nullptr)
        , m_storageType(storageType)
    {
    }
#endif

    void* inlineBuffer()
    {
        return std::bit_cast<uint8_t*>(this) + sizeof(MarkedVectorBase);
    }

    Status expandCapacity();
    Status expandCapacity(unsigned newCapacity);
    JS_EXPORT_PRIVATE Status slowEnsureCapacity(size_t requestedCapacity);
    JS_EXPORT_PRIVATE void slowEnsureCapacityAndCrashOnOverflow(size_t requestedCapacity);

    JS_EXPORT_PRIVATE void addMarkSet(JSValue);

    JS_EXPORT_PRIVATE Status slowAppend(JSValue);

#if CPU(ADDRESS64)
    template<typename U>
    requires std::is_same_v<U, JSValueRef>
    void addMarkSet(U v) { addMarkSet(std::bit_cast<JSValue>(v)); }

    template<typename U>
    requires (!std::is_same_v<U, JSValueRef> && std::is_pointer_v<U>)
    void addMarkSet(U v)
    {
        static_assert(MarkedVectorHelper::isJSValueConvertible<U>());
        addMarkSet(JSValue(std::bit_cast<JSCell*>(v)));
    }
#else
    JS_EXPORT_PRIVATE Status slowAppend(const void*);
    JS_EXPORT_PRIVATE void addMarkSet(const void*);

    void*& pointerSlotFor(unsigned item) const
    {
        return std::bit_cast<void**>(m_buffer)[item];
    }

    static JSCell* toCell(const void*, StorageType);
    static JSCell* toCellForGC(const void*, StorageType);
#endif

    EncodedJSValue& jsValueSlotFor(unsigned item) const
    {
        return std::bit_cast<EncodedJSValue*>(m_buffer)[item];
    }

    EncodedJSValue* mallocBase()
    {
        if (m_buffer == inlineBuffer())
            return nullptr;
        return &jsValueSlotFor(0);
    }

    void removeFromMarkSetAndDeallocateBuffer()
    {
        if (m_markSet)
            m_markSet->remove(this);
        if (EncodedJSValue* base = mallocBase())
            FastMalloc::free(base);
    }

    void adopt(MarkedVectorBase&& other)
    {
        removeFromMarkSetAndDeallocateBuffer();
        m_markSet = nullptr;

#if CPU(ADDRESS32)
        ASSERT(m_storageType == other.m_storageType);
#endif

        auto size = other.m_size;
        if (other.mallocBase()) {
            m_buffer = std::exchange(other.m_buffer, other.inlineBuffer());
            if (other.m_markSet) {
                m_markSet = other.m_markSet;
                m_markSet->remove(&other);
                m_markSet->add(this);
                other.m_markSet = nullptr;
            }
        } else {
            m_buffer = inlineBuffer();
            ASSERT(!m_markSet);
#if CPU(ADDRESS32)
            if (m_storageType != StorageType::JSValue) {
                auto dstSpan = unsafeMakeSpan(std::bit_cast<void**>(m_buffer), size);
                auto srcSpan = unsafeMakeSpan(std::bit_cast<void**>(other.m_buffer), size);
                memcpySpan(dstSpan, srcSpan);
            } else
#endif
            {
                auto dstSpan = unsafeMakeSpan(std::bit_cast<EncodedJSValue*>(m_buffer), size);
                auto srcSpan = unsafeMakeSpan(std::bit_cast<EncodedJSValue*>(other.m_buffer), size);
                memcpySpan(dstSpan, srcSpan);
            }
        }

        m_capacity = std::exchange(other.m_capacity, 0);
        m_size = std::exchange(other.m_size, 0);
    }

#if ASSERT_ENABLED
    void disableNeedsOverflowCheck() { m_overflowCheckEnabled = false; }
    void setNeedsOverflowCheck() { m_needsOverflowCheck = m_overflowCheckEnabled; }
    void clearNeedsOverflowCheck() { m_needsOverflowCheck = false; }

    bool m_needsOverflowCheck { false };
    bool m_overflowCheckEnabled { true };
#else
    void disableNeedsOverflowCheck() { }
    void setNeedsOverflowCheck() { }
    void clearNeedsOverflowCheck() { }
#endif // ASSERT_ENABLED
    unsigned m_size;
    unsigned m_capacity;
    void* m_buffer;
    ListSet* m_markSet;
#if CPU(ADDRESS32)
    StorageType m_storageType;
#endif

    // The only reason we need this m_storageForOutOfBoundsAccess field is because the at()
    // accessors can potentially allow the client to read / write an OOB value when the
    // OverHandler does not CrashOnOverflow (e.g. RecordOverflow), and hence, be able to
    // read / write out of bounds. If the client is using RecordOverflow, it is expected to
    // explicitly check and handle the overflow in that case, and the OOB access should never
    // occur. However, as a hardening measure, in the event of a bug, to prevent this potential
    // out of bounds access from corrupting any real data, we'll ensure that at() return a
    // reference to the singleton m_storageForOutOfBoundsAccess field, and ensure that any junk
    // data is read from / written there.
    JS_EXPORT_PRIVATE static EncodedJSValue m_storageForOutOfBoundsAccess;
};

template<typename T, size_t passedInlineCapacity = 8, class OverflowHandler = CrashOnOverflow>
class MarkedVector : public OverflowHandler, public MarkedVectorBase  {
    using Base = MarkedVectorBase;
#if CPU(ADDRESS64)
    static_assert(sizeof(T) == sizeof(EncodedJSValue));
#endif
public:
    static constexpr size_t inlineCapacity = passedInlineCapacity;

    MarkedVector()
#if CPU(ADDRESS64)
        : Base(inlineCapacity)
#else
        : Base(inlineCapacity, storageType<T>())
#endif
    {
        static_assert(MarkedVectorHelper::isJSValueConvertible<T>());
        ASSERT(std::bit_cast<T*>(inlineBuffer()) == m_inlineBuffer);
        if constexpr (std::is_same_v<OverflowHandler, CrashOnOverflow>) {
            // CrashOnOverflow handles overflows immediately. So, we do not
            // need to check for it after.
            disableNeedsOverflowCheck();
        }
    }

    MarkedVector(size_t initialCapacity)
        : MarkedVector()
    {
        static_assert(MarkedVectorHelper::isJSValueConvertible<T>());
        ensureCapacity(initialCapacity);
    }

    MarkedVector(MarkedVector&& other)
#if CPU(ADDRESS64)
        : Base(inlineCapacity)
#else
        : Base(inlineCapacity, storageType<T>())
#endif
    {
        static_assert(MarkedVectorHelper::isJSValueConvertible<T>());
        Base::adopt(std::forward<MarkedVector>(other));
    }

    MarkedVector& operator=(MarkedVector&& other)
    {
        Base::adopt(std::forward<MarkedVector>(other));
        return *this;
    }

    template<typename U = T>
    const U* data() const { return std::bit_cast<U*>(m_buffer); }
    template<typename U = T>
    U* data() { return std::bit_cast<U*>(m_buffer); }

    [[nodiscard]] std::span<const T> span() const LIFETIME_BOUND { return { data(), size() }; }
    [[nodiscard]] std::span<T> mutableSpan() LIFETIME_BOUND { return { data(), size() }; }

    T* begin() { return std::bit_cast<T*>(m_buffer); }
    T* end() { return std::bit_cast<T*>(m_buffer) + m_size; }

    [[nodiscard]] T& at(size_t i) LIFETIME_BOUND
    {
        if (i >= size()) { [[unlikely]]
            OverflowHandler::overflowed();
            if constexpr (!std::is_same_v<OverflowHandler, CrashOnOverflow>) {
                T* oobStorage = std::bit_cast<T*>(&m_storageForOutOfBoundsAccess);
                *oobStorage = uninitializedValue<T>();
                return *oobStorage;
            }
        }
        return begin()[i];
    }
    [[nodiscard]] const T& at(size_t i) const LIFETIME_BOUND
    {
        if (i >= size()) { [[unlikely]]
            const_cast<MarkedVector*>(this)->OverflowHandler::overflowed();
            if constexpr (!std::is_same_v<OverflowHandler, CrashOnOverflow>) {
                T* oobStorage = std::bit_cast<T*>(&m_storageForOutOfBoundsAccess);
                *oobStorage = uninitializedValue<T>();
                return *oobStorage;
            }
        }
        return const_cast<MarkedVector*>(this)->begin()[i];
    }

    [[nodiscard]] T& operator[](size_t i) LIFETIME_BOUND { return at(i); }
    [[nodiscard]] const T& operator[](size_t i) const LIFETIME_BOUND { return at(i); }

    void clear()
    {
        ASSERT(!m_needsOverflowCheck);
        OverflowHandler::clearOverflow();
        m_size = 0;
    }

    void append(T v)
    {
        ASSERT(m_size <= m_capacity);
        if (m_size == m_capacity || mallocBase()) {
            if (slowAppend<T>(v) == Status::Overflowed)
                this->overflowed();
            return;
        }

        *end() = v;
        ++m_size;
    }

#if CPU(ADDRESS64)
    template<typename U>
    Status slowAppend(U v) { return Base::slowAppend(std::bit_cast<JSValue>(v)); }
#else
    template<typename U>
    requires std::is_pointer_v<U>
    Status slowAppend(U v) { return Base::slowAppend(v); }

    template<typename U>
    Status slowAppend(JSValue v) { return Base::slowAppend(v); }
#endif

    void appendWithCrashOnOverflow(T v)
    {
        append(v);
        if constexpr (!std::is_same<OverflowHandler, CrashOnOverflow>::value)
            RELEASE_ASSERT(!this->hasOverflowed());
    }

    auto last() const -> T
    {
        ASSERT(m_size);
        return at(m_size - 1);
    }

    T takeLast()
    {
        T result = last();
        removeLast();
        return result;
    }

    void ensureCapacity(size_t requestedCapacity)
    {
        if (requestedCapacity > static_cast<size_t>(m_capacity)) {
            if constexpr (WTF::shouldCrashOnOverflow(OverflowHandler::policy))
                slowEnsureCapacityAndCrashOnOverflow(requestedCapacity);
            else {
                if (slowEnsureCapacity(requestedCapacity) == Status::Overflowed)
                    this->overflowed();
            }
        }
    }

    bool hasOverflowed()
    {
        clearNeedsOverflowCheck();
        return OverflowHandler::hasOverflowed();
    }

    template<typename Functor>
    void fill(VM& vm, size_t count, const Functor& func)
    {
        ASSERT(!m_size);
        ensureCapacity(count);
        if constexpr (!WTF::shouldCrashOnOverflow(OverflowHandler::policy)) {
            if (OverflowHandler::hasOverflowed())
                return;
        }
        if (!isUsingInlineBuffer()) {
            if (!m_markSet) [[likely]] {
                m_markSet = &vm.heap.markListSet();
                m_markSet->add(this);
            }
        }
        m_size = count;
        auto* buffer = begin();

        // This clearing does not need to consider about concurrent marking from GC since MarkedVector
        // gets marked only while mutator is stopping. So, while clearing in the mutator, concurrent
        // marker will not see the buffer.
#if USE(JSVALUE64)
        zeroSpan(unsafeMakeSpan(std::bit_cast<uint8_t*>(buffer), sizeof(T) * count));
#else
        clearBuffer(buffer, count);
#endif

        func(buffer);
    }

    void fillWith(VM& vm, MarkedVectorHelper::Iterable auto const& iterable, auto&& mapValue)
    {
        ensureCapacity(iterable.size());
        if constexpr (!WTF::shouldCrashOnOverflow(OverflowHandler::policy)) {
            if (OverflowHandler::hasOverflowed())
                return;
        }
        m_size = 0;
        if (!isUsingInlineBuffer()) {
            if (!m_markSet) [[likely]] {
                m_markSet = &vm.heap.markListSet();
                m_markSet->add(this);
            }
        }

        size_t i = 0;
        for (const auto it : iterable) {
            begin()[i++] = mapValue(it);
            m_size = i;
        }
    }

private:
    bool isUsingInlineBuffer() const { return m_buffer == m_inlineBuffer; }

#if USE(JSVALUE32_64)
    template<typename U>
    requires std::is_pointer_v<U>
    static void clearBuffer(U* buffer, size_t count)
    {
        zeroSpan(unsafeMakeSpan(buffer, count));
    }

    template<typename U>
    requires std::is_same_v<U, JSValue>
    static void clearBuffer(U* buffer, size_t count)
    {
        for (unsigned i = 0; i < count; ++i)
            buffer[i] = JSValue();
    }
#endif

    template<typename U>
    requires std::is_pointer_v<U>
    static constexpr U uninitializedValue() { return nullptr; }

    template<typename U>
    static constexpr U uninitializedValue() { return std::bit_cast<U>(encodedJSUndefined()); }

    T m_inlineBuffer[inlineCapacity] { };
};

template<size_t passedInlineCapacity>
class MarkedArgumentBufferWithSize : public MarkedVector<JSValue, passedInlineCapacity, RecordOverflow> {
    WTF_MAKE_NONCOPYABLE(MarkedArgumentBufferWithSize);
    WTF_MAKE_NONMOVABLE(MarkedArgumentBufferWithSize);
    using Base = MarkedVector<JSValue, passedInlineCapacity, RecordOverflow>;
public:
    MarkedArgumentBufferWithSize() = default;

    const EncodedJSValue* data() const { return Base::template data<EncodedJSValue>(); }
    EncodedJSValue* data() { return Base::template data<EncodedJSValue>(); }
};

using MarkedArgumentBuffer = MarkedArgumentBufferWithSize<8>;

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
