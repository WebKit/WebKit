/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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

class MarkedArgumentBuffer : public RecordOverflow {
    WTF_MAKE_NONCOPYABLE(MarkedArgumentBuffer);
    WTF_FORBID_HEAP_ALLOCATION;
    friend class VM;
    friend class ArgList;

public:
    using Base = RecordOverflow;
    static const size_t inlineCapacity = 8;
    typedef HashSet<MarkedArgumentBuffer*> ListSet;

    // Constructor for a read-write list, to which you may append values.
    // FIXME: Remove all clients of this API, then remove this API.
    MarkedArgumentBuffer()
        : m_size(0)
        , m_capacity(inlineCapacity)
        , m_buffer(m_inlineBuffer)
        , m_markSet(0)
    {
    }

    ~MarkedArgumentBuffer()
    {
        ASSERT(!m_needsOverflowCheck);
        if (m_markSet)
            m_markSet->remove(this);

        if (EncodedJSValue* base = mallocBase())
            Gigacage::free(Gigacage::JSValue, base);
    }

    size_t size() const { return m_size; }
    bool isEmpty() const { return !m_size; }

    JSValue at(int i) const
    {
        if (i >= m_size)
            return jsUndefined();

        return JSValue::decode(slotFor(i));
    }

    void clear()
    {
        ASSERT(!m_needsOverflowCheck);
        clearOverflow();
        m_size = 0;
    }

    enum OverflowCheckAction {
        CrashOnOverflow,
        WillCheckLater
    };
    template<OverflowCheckAction action>
    void appendWithAction(JSValue v)
    {
        ASSERT(m_size <= m_capacity);
        if (m_size == m_capacity || mallocBase()) {
            slowAppend(v);
            if (action == CrashOnOverflow)
                RELEASE_ASSERT(!hasOverflowed());
            return;
        }

        slotFor(m_size) = JSValue::encode(v);
        ++m_size;
    }
    void append(JSValue v) { appendWithAction<WillCheckLater>(v); }
    void appendWithCrashOnOverflow(JSValue v) { appendWithAction<CrashOnOverflow>(v); }

    void removeLast()
    { 
        ASSERT(m_size);
        m_size--;
    }

    JSValue last() 
    {
        ASSERT(m_size);
        return JSValue::decode(slotFor(m_size - 1));
    }

    JSValue takeLast()
    {
        JSValue result = last();
        removeLast();
        return result;
    }
        
    static void markLists(SlotVisitor&, ListSet&);

    void ensureCapacity(size_t requestedCapacity)
    {
        if (requestedCapacity > static_cast<size_t>(m_capacity))
            slowEnsureCapacity(requestedCapacity);
    }

    bool hasOverflowed()
    {
        clearNeedsOverflowCheck();
        return Base::hasOverflowed();
    }

    void overflowCheckNotNeeded() { clearNeedsOverflowCheck(); }

    template<typename Functor>
    void fill(size_t count, const Functor& func)
    {
        ASSERT(!m_size);
        ensureCapacity(count);
        if (Base::hasOverflowed())
            return;
        m_size = count;
        func(reinterpret_cast<JSValue*>(&slotFor(0)));
    }

private:
    void expandCapacity();
    void expandCapacity(int newCapacity);
    void slowEnsureCapacity(size_t requestedCapacity);

    void addMarkSet(JSValue);

    JS_EXPORT_PRIVATE void slowAppend(JSValue);

    EncodedJSValue& slotFor(int item) const
    {
        return m_buffer[item];
    }
        
    EncodedJSValue* mallocBase()
    {
        if (m_buffer == m_inlineBuffer)
            return 0;
        return &slotFor(0);
    }

#if ASSERT_ENABLED
    void setNeedsOverflowCheck() { m_needsOverflowCheck = true; }
    void clearNeedsOverflowCheck() { m_needsOverflowCheck = false; }

    bool m_needsOverflowCheck { false };
#else
    void setNeedsOverflowCheck() { }
    void clearNeedsOverflowCheck() { }
#endif // ASSERT_ENABLED
    int m_size;
    int m_capacity;
    EncodedJSValue m_inlineBuffer[inlineCapacity];
    EncodedJSValue* m_buffer;
    ListSet* m_markSet;
};

class ArgList {
    WTF_MAKE_FAST_ALLOCATED;
    friend class Interpreter;
    friend class JIT;
public:
    ArgList()
        : m_args(0)
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
