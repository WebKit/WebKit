/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MarkedJSValueRefArray.h"

#include "JSCJSValue.h"

namespace JSC {

MarkedJSValueRefArray::MarkedJSValueRefArray(JSGlobalContextRef context, unsigned size)
    : m_size(size)
{
    if (m_size > MarkedArgumentBuffer::inlineCapacity) {
        m_buffer = BufferUniquePtr::create(m_size);
        toJS(context)->vm().heap.addMarkedJSValueRefArray(this);
        ASSERT(isOnList());
    }
}

MarkedJSValueRefArray::~MarkedJSValueRefArray()
{
    if (isOnList())
        remove();
}

template<typename Visitor>
void MarkedJSValueRefArray::visitAggregate(Visitor& visitor)
{
    JSValueRef* buffer = data();
    for (unsigned index = 0; index < m_size; ++index) {
        JSValueRef value = buffer[index];
#if !CPU(ADDRESS64)
        JSCell* jsCell = reinterpret_cast<JSCell*>(const_cast<OpaqueJSValue*>(value));
        if (!jsCell)
            continue;
        visitor.appendUnbarriered(jsCell); // We should mark the wrapper itself to keep JSValueRef live.
#else
        visitor.appendUnbarriered(bitwise_cast<JSValue>(value));
#endif
    }
}

template void MarkedJSValueRefArray::visitAggregate(AbstractSlotVisitor&);
template void MarkedJSValueRefArray::visitAggregate(SlotVisitor&);

} // namespace JSC
