/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "JSWasmModule.h"

#if ENABLE(WEBASSEMBLY)

#include "AuxiliaryBarrierInlines.h"
#include "HeapInlines.h"
#include "JSArrayBuffer.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSFunction.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSWasmModule::s_info = { "WasmModule", &Base::s_info, 0, CREATE_METHOD_TABLE(JSWasmModule) };

JSWasmModule::JSWasmModule(VM& vm, Structure* structure, JSArrayBuffer* arrayBuffer)
    : Base(vm, structure)
{
    if (arrayBuffer)
        m_arrayBuffer.set(vm, this, arrayBuffer);
}

void JSWasmModule::destroy(JSCell* cell)
{
    JSWasmModule* thisObject = jsCast<JSWasmModule*>(cell);
    thisObject->JSWasmModule::~JSWasmModule();
}

void JSWasmModule::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSWasmModule* thisObject = jsCast<JSWasmModule*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(&thisObject->m_arrayBuffer);
    for (auto function : thisObject->m_functions)
        visitor.append(&function);
    for (auto importedFunction : thisObject->m_importedFunctions)
        visitor.append(&importedFunction);
}

} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
