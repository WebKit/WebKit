/*
 * Copyright (C) 2019 Apple, Inc. All rights reserved.
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
#include "JSWeakObjectRef.h"

#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSWeakObjectRef::s_info = { "WeakRef", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWeakObjectRef) };

void JSWeakObjectRef::finishCreation(VM& vm, JSObject* value)
{
    m_lastAccessVersion = vm.currentWeakRefVersion();
    m_value.set(vm, this, value);
    Base::finishCreation(vm);
}

void JSWeakObjectRef::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSWeakObjectRef*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    // This doesn't need to be atomic because if we are out of date we will get write barriered and revisit ourselves.
    if (visitor.vm().currentWeakRefVersion() == thisObject->m_lastAccessVersion) {
        ASSERT(thisObject->m_value);
        visitor.append(thisObject->m_value);
    }
}

void JSWeakObjectRef::finalizeUnconditionally(VM& vm)
{
    if (m_value && !vm.heap.isMarked(m_value.get()))
        m_value.clear();
}

String JSWeakObjectRef::toStringName(const JSC::JSObject*, ExecState*)
{
    return "Object"_s;
}

}

