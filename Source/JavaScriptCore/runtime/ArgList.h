/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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
#include <wtf/CheckedArithmetic.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/HashSet.h>

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
    Status slowEnsureCapacity(size_t requestedCapacity);

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
        if (LIKELY(!m_markSet)) {
            m_markSet = &vm.heap.markListSet();
            m_markSet->add(this);
        }
        m_size = count;
        auto* buffer = reinterpret_cast<JSValue*>(&slotFor(0));
        for (unsigned i = 0; i < count; ++i)
            buffer[i] = JSValue();
        func(buffer);
    }

private:
    EncodedJSValue m_inlineBuffer[inlineCapacity] { };
};

template<size_t passedInlineCapacity>
class MarkedArgumentBufferWithSize : public MarkedVector<JSValue, passedInlineCapacity, RecordOverflow> {
};

using MarkedArgumentBuffer = MarkedVector<JSValue, 8, RecordOverflow>;

class ArgList {
    WTF_MAKE_FAST_ALLOCATED;
    friend class Interpreter;
    friend class JIT;
public:
    ArgList()
        : m_args(nullptr)
        , m_argCount(0)
    {
    }

    ArgList(CallFrame* callFrame)
        : m_args(reinterpret_cast<JSValue*>(&callFrame[CallFrame::argumentOffset(0)]))
        , m_argCount(callFrame->argumentCount())
    {
    }

    ArgList(const MarkedArgumentBuffer& args)
        : m_args(reinterpret_cast<JSValue*>(args.m_buffer))
        , m_argCount(args.size())
    {
    }

    JSValue at(int i) const
    {
        if (i >= m_argCount)
            return jsUndefined();
        return m_args[i];
    }

    bool isEmpty() const { return !m_argCount; }
    size_t size() const { return m_argCount; }
        
    JS_EXPORT_PRIVATE void getSlice(int startIndex, ArgList& result) const;

private:
    JSValue* data() const { return m_args; }

    JSValue* m_args;
    int m_argCount;
};

} // namespace JSC
