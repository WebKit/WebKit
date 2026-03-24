/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2026 Apple Inc. All rights reserved.
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

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

class alignas(alignof(EncodedJSValue)) MarkedVectorBase {
    WTF_MAKE_NONCOPYABLE(MarkedVectorBase);
    WTF_MAKE_NONMOVABLE(MarkedVectorBase);
    WTF_FORBID_HEAP_ALLOCATION;
    friend class VM;
    friend class ArgList;

protected:
    enum class Status { Success, Overflowed };
public:
    typedef UncheckedKeyHashSet<MarkedVectorBase*> ListSet;

    ~MarkedVectorBase()
    {
        if (m_markSet)
            m_markSet->remove(this);

        if (EncodedJSValue* base = mallocBase())
            FastMalloc::free(base);
    }

    size_t size() const { return m_size; }
    bool isEmpty() const { return !m_size; }

    const EncodedJSValue* data() const { return m_buffer; }
    EncodedJSValue* data() { return m_buffer; }

    void removeLast()
    {
        ASSERT(m_size);
        m_size--;
    }

    template<typename Visitor> static void markLists(Visitor&, ListSet&);

    void overflowCheckNotNeeded() { clearNeedsOverflowCheck(); }

protected:
    // Constructor for a read-write list, to which you may append values.
    // FIXME: Remove all clients of this API, then remove this API.
    MarkedVectorBase(size_t capacity)
        : m_size(0)
        , m_capacity(capacity)
        , m_buffer(inlineBuffer())
        , m_markSet(nullptr)
    {
    }

    EncodedJSValue* inlineBuffer()
    {
        return std::bit_cast<EncodedJSValue*>(std::bit_cast<uint8_t*>(this) + sizeof(MarkedVectorBase));
    }

    Status expandCapacity();
    Status expandCapacity(unsigned newCapacity);
    JS_EXPORT_PRIVATE Status slowEnsureCapacity(size_t requestedCapacity);

    void addMarkSet(JSValue);

    JS_EXPORT_PRIVATE Status slowAppend(JSValue);

    EncodedJSValue& slotFor(unsigned item) const
    {
        return m_buffer[item];
    }

    EncodedJSValue* mallocBase()
    {
        if (m_buffer == inlineBuffer())
            return nullptr;
        return &slotFor(0);
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
    EncodedJSValue* m_buffer;
    ListSet* m_markSet;

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

namespace MarkedVectorHelper {

template<typename T>
static constexpr bool isJSValueConvertible()
{
    return WTF::requiresGCAwareContainer<T>();
}

template <typename T>
concept Iterable = requires(T t) {
    t.begin();
    t.end();
    t.size();
};

} // namespace MarkedVectorHelper

template<typename T, size_t passedInlineCapacity = 8, class OverflowHandler = CrashOnOverflow>
class MarkedVector : public OverflowHandler, public MarkedVectorBase  {
    using Base = MarkedVectorBase;
    static_assert(sizeof(T) == sizeof(EncodedJSValue));
public:
    static constexpr size_t inlineCapacity = passedInlineCapacity;

    MarkedVector()
        : MarkedVectorBase(inlineCapacity)
    {
        static_assert(MarkedVectorHelper::isJSValueConvertible<T>());
        ASSERT(inlineBuffer() == m_inlineBuffer);
        if constexpr (std::is_same_v<OverflowHandler, CrashOnOverflow>) {
            // CrashOnOverflow handles overflows immediately. So, we do not
            // need to check for it after.
            disableNeedsOverflowCheck();
        }
    }

    MarkedVector(size_t initialCapacity)
        : MarkedVector()
    {
        ensureCapacity(initialCapacity);
    }

    [[nodiscard]] std::span<const T> span() const LIFETIME_BOUND { return { std::bit_cast<T*>(data()), size() }; }
    [[nodiscard]] std::span<T> mutableSpan() LIFETIME_BOUND { return { std::bit_cast<T*>(data()), size() }; }

    T* begin() { return std::bit_cast<T*>(m_buffer); }
    T* end() { return std::bit_cast<T*>(m_buffer + m_size); }

    [[nodiscard]] T& at(size_t i) LIFETIME_BOUND
    {
        if (i >= size()) { [[unlikely]]
            OverflowHandler::overflowed();
            if constexpr (!std::is_same_v<OverflowHandler, CrashOnOverflow>) {
                m_storageForOutOfBoundsAccess = encodedJSUndefined();
                return *std::bit_cast<T*>(&m_storageForOutOfBoundsAccess);
            }
        }
        return begin()[i];
    }
    [[nodiscard]] const T& at(size_t i) const LIFETIME_BOUND
    {
        if (i >= size()) { [[unlikely]]
            const_cast<MarkedVector*>(this)->OverflowHandler::overflowed();
            if constexpr (!std::is_same_v<OverflowHandler, CrashOnOverflow>) {
                m_storageForOutOfBoundsAccess = encodedJSUndefined();
                return *std::bit_cast<const T*>(&m_storageForOutOfBoundsAccess);
            }
        }
        return const_cast<MarkedVector*>(this)->begin()[i];
    }

    [[nodiscard]] T& operator[](size_t i) LIFETIME_BOUND { return at(i); }
    [[nodiscard]] const T& operator[](size_t i) const LIFETIME_BOUND { return at(i); }

    void set(unsigned i, T value)
    {
        if (i >= m_size)
            return;
        slotFor(i) = JSValue::encode(toJSValue(value));
    }

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
            if (slowAppend(toJSValue(v)) == Status::Overflowed)
                this->overflowed();
            return;
        }

        slotFor(m_size) = JSValue::encode(toJSValue(v));
        ++m_size;
    }

    void appendWithCrashOnOverflow(T v)
    {
        append(v);
        if constexpr (!std::is_same<OverflowHandler, CrashOnOverflow>::value)
            RELEASE_ASSERT(!this->hasOverflowed());
    }

    auto last() const -> T
    {
        if constexpr (std::is_same_v<T, JSValue> || isJSCAPIValueType<T>()) {
            ASSERT(m_size);
            return std::bit_cast<T>(JSValue::decode(slotFor(m_size - 1)));
        } else {
            ASSERT(m_size);
            return jsCast<T>(JSValue::decode(slotFor(m_size - 1)).asCell());
        }
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
            if (slowEnsureCapacity(requestedCapacity) == Status::Overflowed)
                this->overflowed();
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
        if (OverflowHandler::hasOverflowed())
            return;
        if (!isUsingInlineBuffer()) {
            if (!m_markSet) [[likely]] {
                m_markSet = &vm.heap.markListSet();
                m_markSet->add(this);
            }
        }
        m_size = count;
        auto* buffer = reinterpret_cast<JSValue*>(&slotFor(0));

        // This clearing does not need to consider about concurrent marking from GC since MarkedVector
        // gets marked only while mutator is stopping. So, while clearing in the mutator, concurrent
        // marker will not see the buffer.
#if USE(JSVALUE64)
        memset(std::bit_cast<void*>(buffer), 0, sizeof(JSValue) * count);
#else
        for (unsigned i = 0; i < count; ++i)
            buffer[i] = JSValue();
#endif

        func(buffer);
    }

    void fillWith(MarkedVectorHelper::Iterable auto const& iterable, auto&& mapValue)
    {
        ensureCapacity(iterable.size());
        for (const auto it : iterable)
            append(mapValue(it));
    }

private:
    bool isUsingInlineBuffer() const { return m_buffer == m_inlineBuffer; }
    static JSValue toJSValue(T value) { return std::bit_cast<JSValue>(value); }

    EncodedJSValue m_inlineBuffer[inlineCapacity] { };
};

template<size_t passedInlineCapacity>
class MarkedArgumentBufferWithSize : public MarkedVector<JSValue, passedInlineCapacity, RecordOverflow> {
};

using MarkedArgumentBuffer = MarkedVector<JSValue, 8, RecordOverflow>;

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
