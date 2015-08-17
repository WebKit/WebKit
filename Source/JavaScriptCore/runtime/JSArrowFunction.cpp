/*
 * Copyright (C) 2015 Aleksandr Skachkov <gskachkov@gmail.com>.
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
#include "JSArrowFunction.h"

#include "Error.h"
#include "ExceptionHelpers.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSFunction.h"
#include "JSFunctionInlines.h"
#include "JSObject.h"
#include "PropertySlot.h"
#include "VM.h"

namespace JSC {

const ClassInfo JSArrowFunction::s_info = { "ArrowFunction", &Base::s_info, 0, CREATE_METHOD_TABLE(JSArrowFunction) };

void JSArrowFunction::destroy(JSCell* cell)
{
    static_cast<JSArrowFunction*>(cell)->JSArrowFunction::~JSArrowFunction();
}

JSArrowFunction* JSArrowFunction::create(VM& vm, FunctionExecutable* executable, JSScope* scope, JSValue boundThis)
{
    JSArrowFunction* result = createImpl(vm, executable, scope, boundThis);
    executable->singletonFunction()->notifyWrite(vm, result, "Allocating an arrow function");
    return result;
}

JSArrowFunction::JSArrowFunction(VM& vm, FunctionExecutable* executable, JSScope* scope, JSValue boundThis)
    : Base(vm, executable, scope, scope->globalObject()->arrowFunctionStructure())
    , m_boundThis(vm, this, boundThis)
{
}

JSArrowFunction* JSArrowFunction::createWithInvalidatedReallocationWatchpoint(VM& vm, FunctionExecutable* executable, JSScope* scope, JSValue boundThis)
{
    ASSERT(executable->singletonFunction()->hasBeenInvalidated());
    return create(vm, executable, scope, boundThis);
}

void JSArrowFunction::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSArrowFunction* thisObject = jsCast<JSArrowFunction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_boundThis);
}

ConstructType JSArrowFunction::getConstructData(JSCell*, ConstructData&)
{
    return ConstructTypeNone;
}

} // namespace JSC
