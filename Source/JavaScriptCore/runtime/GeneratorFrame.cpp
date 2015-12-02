/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GeneratorFrame.h"

#include "CodeBlock.h"
#include "HeapIterationScope.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo GeneratorFrame::s_info = { "GeneratorFrame", nullptr, nullptr, CREATE_METHOD_TABLE(GeneratorFrame) };

GeneratorFrame::GeneratorFrame(VM& vm, size_t numberOfCalleeLocals)
    : Base(vm, vm.generatorFrameStructure.get())
    , m_numberOfCalleeLocals(numberOfCalleeLocals)
{
}

void GeneratorFrame::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    for (size_t i = 0; i < m_numberOfCalleeLocals; ++i)
        localAt(i).clear();
}

Structure* GeneratorFrame::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(CellType, StructureFlags), info());
}

GeneratorFrame* GeneratorFrame::create(VM& vm, size_t numberOfLocals)
{
    GeneratorFrame* result =
        new (
            NotNull,
            allocateCell<GeneratorFrame>(vm.heap, allocationSizeForLocals(numberOfLocals)))
        GeneratorFrame(vm, numberOfLocals);
    result->finishCreation(vm);
    return result;
}

void GeneratorFrame::save(ExecState* exec, const FastBitVector& liveCalleeLocals)
{
    // Only save callee locals.
    // Every time a generator is called (or resumed), parameters should be replaced.
    ASSERT(liveCalleeLocals.numBits() <= m_numberOfCalleeLocals);
    liveCalleeLocals.forEachSetBit([&](size_t index) {
        localAt(index).set(exec->vm(), this, exec->uncheckedR(virtualRegisterForLocal(index)).jsValue());
    });
}

void GeneratorFrame::resume(ExecState* exec, const FastBitVector& liveCalleeLocals)
{
    // Only resume callee locals.
    // Every time a generator is called (or resumed), parameters should be replaced.
    liveCalleeLocals.forEachSetBit([&](size_t index) {
        exec->uncheckedR(virtualRegisterForLocal(index)) = localAt(index).get();
        localAt(index).clear();
    });
}

void GeneratorFrame::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    GeneratorFrame* thisObject = jsCast<GeneratorFrame*>(cell);
    Base::visitChildren(thisObject, visitor);
    // Since only true cell pointers are stored as a cell, we can safely mark them.
    visitor.appendValues(thisObject->locals(), thisObject->m_numberOfCalleeLocals);
}

} // namespace JSC
