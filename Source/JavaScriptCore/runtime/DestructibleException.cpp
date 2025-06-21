/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DestructibleException.h"

#include "Interpreter.h"
#include "JSCJSValueInlines.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"
#include "JSWebAssemblyException.h"
#include "WasmTag.h"

namespace JSC {

const ClassInfo DestructibleException::s_info = { "DestructibleException"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DestructibleException) };

DestructibleException* DestructibleException::create(VM& vm, JSValue thrownValue)
{
    DestructibleException* result = new (NotNull, allocateCell<DestructibleException>(vm)) DestructibleException(vm, thrownValue);
    result->finishCreation(vm);
    return result;
}

void DestructibleException::destroy(JSCell* cell)
{
    static_cast<DestructibleException*>(cell)->~DestructibleException();
}

Structure* DestructibleException::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

template<typename Visitor>
void DestructibleException::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    DestructibleException* thisObject = jsCast<DestructibleException*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    if (SUPPRESS_UNCOUNTED_LOCAL auto* stack = thisObject->m_stack.get())
        stack->visitAggregate(visitor);
}

DEFINE_VISIT_CHILDREN(DestructibleException);

DestructibleException::DestructibleException(VM& vm, JSValue thrownValue)
    : Base(vm, vm.destructibleExceptionStructure.get(), thrownValue)
{
}

DestructibleException::~DestructibleException() = default;

void DestructibleException::finishCreation(VM& vm)
{
    Base::finishCreation(vm);

    auto stackTrace = makeUnique<Vector<StackFrame>>();
    vm.interpreter.getStackTrace(this, *stackTrace, 0, Options::exceptionStackTraceLimit());
    auto stack = ExceptionStackContent::create(WTFMove(stackTrace));
    size_t sizeInBytes = stack->sizeInBytes();
    WTF::storeStoreFence();
    m_stack = WTFMove(stack);
    vm.writeBarrier(this);
    vm.heap.reportExtraMemoryAllocated(this, sizeInBytes);
}

ExceptionStack DestructibleException::stack() const
{
    return ExceptionStack { m_stack, m_replacedLineColumn, m_callDepth };
}

} // namespace JSC
