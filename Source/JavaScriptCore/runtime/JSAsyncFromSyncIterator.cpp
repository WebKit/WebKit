/*
 * Copyright (C) 2024 Sosuke Suzuki <aosukeke@gmail.com>.
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
#include "JSAsyncFromSyncIterator.h"

#include "JSCInlines.h"

namespace JSC {

const ClassInfo JSAsyncFromSyncIterator::s_info = { "AsyncFromSyncIterator"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAsyncFromSyncIterator) };

JSAsyncFromSyncIterator* JSAsyncFromSyncIterator::create(VM& vm, Structure* structure, JSObject* syncIterator, JSValue nextMethod, IterationMode iterationMode)
{
    JSAsyncFromSyncIterator* result = new (NotNull, allocateCell<JSAsyncFromSyncIterator>(vm)) JSAsyncFromSyncIterator(vm, structure, syncIterator, nextMethod, iterationMode);
    result->finishCreation(vm);
    return result;
}

template<typename Visitor>
void JSAsyncFromSyncIterator::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSAsyncFromSyncIterator>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.appendUnbarriered(thisObject->m_syncIterator.pointer());
    visitor.append(thisObject->m_nextMethod);
    visitor.appendUnbarriered(thisObject->m_target.pointer());
    visitor.append(thisObject->m_cachedResult);
}

DEFINE_VISIT_CHILDREN(JSAsyncFromSyncIterator);

} // namespace JSC
