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

#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/MarkedVector.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

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
        : m_args(std::bit_cast<EncodedJSValue*>(args.m_buffer))
        , m_argCount(args.size())
    {
    }

    ArgList(EncodedJSValue* args, unsigned count)
        : m_args(args)
        , m_argCount(count)
    {
    }

    JSValue at(unsigned i) const
    {
        if (i >= m_argCount)
            return jsUndefined();
        return JSValue::decode(m_args[i]);
    }

    bool isEmpty() const { return !m_argCount; }
    size_t size() const { return m_argCount; }
        
    JS_EXPORT_PRIVATE void NODELETE getSlice(int startIndex, ArgList& result) const;

    EncodedJSValue* data() const { return m_args; }

private:
    EncodedJSValue* m_args { nullptr };
    unsigned m_argCount { 0 };
};

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
