/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2023 Apple Inc. All rights reserved.
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

#include "CallFrame.h"
#include "JSCast.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>

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
    typedef HashSet<MarkedVectorBase*> ListSet;

    ~MarkedVectorBase()
    {
        if (m_markSet)
            m_markSet->remove(this);

        if (EncodedJSValue* base = mallocBase())
            Gigacage::free(Gigacage::JSValue, base);
    }

    size_t size() const { return m_size; }
    bool isEmpty() const { return !m_size; }

    const EncodedJSValue* data() const { return m_buffer; }

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
        return bitwise_cast<EncodedJSValue*>(bitwise_cast<uint8_t*>(this) + sizeof(MarkedVectorBase));
    }

    Status expandCapacity();
    Status expandCapacity(int newCapacity);
    JS_EXPORT_PRIVATE Status slowEnsureCapacity(size_t requestedCapacity);

    void addMarkSet(JSValue);

    JS_EXPORT_PRIVATE Status slowAppend(JSValue);

    EncodedJSValue& slotFor(int item) const
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
    int m_size;
    int m_capacity;
    EncodedJSValue* m_buffer;
    ListSet* m_markSet;
};

template<typename T, size_t passedInlineCapacity = 8, class OverflowHandler = CrashOnOverflow>
class MarkedVector : public OverflowHandler, public MarkedVectorBase  {
public:
    static constexpr size_t inlineCapacity = passedInlineCapacity;

    MarkedVector()
        : MarkedVectorBase(inlineCapacity)
    {
        ASSERT(inlineBuffer() == m_inlineBuffer);
        if constexpr (std::is_same_v<OverflowHandler, CrashOnOverflow>) {
            // CrashOnOverflow handles overflows immediately. So, we do not
            // need to check for it after.
            disableNeedsOverflowCheck();
        }
    }

    auto at(int i) const -> decltype(auto)
    {
        if constexpr (std::is_same_v<T, JSValue>) {
            if (i >= m_size)
                return jsUndefined();
            return JSValue::decode(slotFor(i));
        } else {
            if (i >= m_size)
                return static_cast<T>(nullptr);
            return jsCast<T>(JSValue::decode(slotFor(i)).asCell());
        }
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
            if (slowAppend(v) == Status::Overflowed)
                this->overflowed();
            return;
        }

        slotFor(m_size) = JSValue::encode(v);
        ++m_size;
    }

    void appendWithCrashOnOverflow(T v)
    {
        append(v);
        if constexpr (!std::is_same<OverflowHandler, CrashOnOverflow>::value)
            RELEASE_ASSERT(!this->hasOverflowed());
    }

    auto last() const -> decltype(auto)
    {
        if constexpr (std::is_same_v<T, JSValue>) {
            ASSERT(m_size);
            return JSValue::decode(slotFor(m_size - 1));
        } else {
            ASSERT(m_size);
            return jsCast<T>(JSValue::decode(slotFor(m_size - 1)).asCell());
        }
    }

    JSValue takeLast()
    {
        JSValue result = last();
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
            if (LIKELY(!m_markSet)) {
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
        memset(bitwise_cast<void*>(buffer), 0, sizeof(JSValue) * count);
#else
        for (unsigned i = 0; i < count; ++i)
            buffer[i] = JSValue();
#endif

        func(buffer);
    }

private:
    bool isUsingInlineBuffer() const { return m_buffer == m_inlineBuffer; }

    EncodedJSValue m_inlineBuffer[inlineCapacity] { };
};

template<size_t passedInlineCapacity>
class MarkedArgumentBufferWithSize : public MarkedVector<JSValue, passedInlineCapacity, RecordOverflow> {
};

using MarkedArgumentBuffer = MarkedVector<JSValue, 8, RecordOverflow>;

class ArgList {
    WTF_MAKE_TZONE_ALLOCATED(ArgList);
    friend class Interpreter;
    friend class JIT;
public:
    ArgList() = default;

    ArgList(CallFrame* callFrame)
        : m_args(reinterpret_cast<EncodedJSValue*>(&callFrame[CallFrame::argumentOffset(0)]))
        , m_argCount(callFrame->argumentCount())
    {
    }

    ArgList(CallFrame* callFrame, int startingFrom)
        : m_args(reinterpret_cast<EncodedJSValue*>(&callFrame[CallFrame::argumentOffset(startingFrom)]))
        , m_argCount(callFrame->argumentCount() - startingFrom)
    {
        ASSERT(static_cast<int>(callFrame->argumentCount()) >= startingFrom);
    }

    template<size_t inlineCapacity>
    ArgList(const MarkedVector<JSValue, inlineCapacity, RecordOverflow>& args)
        : m_args(args.m_buffer)
        , m_argCount(args.size())
    {
    }

    ArgList(EncodedJSValue* args, int count)
        : m_args(args)
        , m_argCount(count)
    {
    }

    JSValue at(int i) const
    {
        if (i >= m_argCount)
            return jsUndefined();
        return JSValue::decode(m_args[i]);
    }

    bool isEmpty() const { return !m_argCount; }
    size_t size() const { return m_argCount; }
        
    JS_EXPORT_PRIVATE void getSlice(int startIndex, ArgList& result) const;

    EncodedJSValue* data() const { return m_args; }

private:
    EncodedJSValue* m_args { nullptr };
    int m_argCount { 0 };
};

} // namespace JSC
