/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "APICast.h"
#include "ArgList.h"
#include <wtf/CagedUniquePtr.h>
#include <wtf/ForbidHeapAllocation.h>
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>

namespace JSC {

class MarkedJSValueRefArray final : public BasicRawSentinelNode<MarkedJSValueRefArray> {
    WTF_MAKE_NONCOPYABLE(MarkedJSValueRefArray);
    WTF_MAKE_NONMOVABLE(MarkedJSValueRefArray);
    WTF_FORBID_HEAP_ALLOCATION;
public:
    using BufferUniquePtr = CagedUniquePtr<Gigacage::JSValue, JSValueRef>;
    static constexpr size_t inlineCapacity = MarkedArgumentBuffer::inlineCapacity;

    JS_EXPORT_PRIVATE MarkedJSValueRefArray(JSGlobalContextRef, unsigned);
    JS_EXPORT_PRIVATE ~MarkedJSValueRefArray();

    size_t size() const { return m_size; }
    bool isEmpty() const { return !m_size; }

    JSValueRef& operator[](unsigned index) { return data()[index]; }

    const JSValueRef* data() const
    {
        return const_cast<MarkedJSValueRefArray*>(this)->data();
    }

    JSValueRef* data()
    {
        if (m_buffer)
            return m_buffer.get(m_size);
        return m_inlineBuffer;
    }

    void visitAggregate(SlotVisitor&);

private:
    unsigned m_size;
    JSValueRef m_inlineBuffer[inlineCapacity] { };
    BufferUniquePtr m_buffer;
};

} // namespace JSC
