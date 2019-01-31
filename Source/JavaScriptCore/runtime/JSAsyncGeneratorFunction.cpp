/*
 * Copyright (C) 2017 Oleksandr Skachkov <gskachkov@gmail.com>.
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
#include "JSAsyncGeneratorFunction.h"

#include "Error.h"
#include "JSCInlines.h"
#include "JSCJSValue.h"
#include "JSFunction.h"
#include "JSFunctionInlines.h"
#include "JSObject.h"
#include "PropertySlot.h"
#include "VM.h"

namespace JSC {

const ClassInfo JSAsyncGeneratorFunction    ::s_info = { "JSAsyncGeneratorFunction",  &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAsyncGeneratorFunction) };

JSAsyncGeneratorFunction::JSAsyncGeneratorFunction(VM& vm, FunctionExecutable* executable, JSScope* scope, Structure* structure)
    : Base(vm, executable, scope, structure)
{
}

JSAsyncGeneratorFunction* JSAsyncGeneratorFunction::createImpl(VM& vm, FunctionExecutable* executable, JSScope* scope, Structure* structure)
{
    JSAsyncGeneratorFunction* asyncGenerator = new (NotNull, allocateCell<JSAsyncGeneratorFunction>(vm.heap)) JSAsyncGeneratorFunction(vm, executable, scope, structure);
    ASSERT(asyncGenerator->structure(vm)->globalObject());
    asyncGenerator->finishCreation(vm);
    return asyncGenerator;
}

JSAsyncGeneratorFunction* JSAsyncGeneratorFunction::create(VM& vm, FunctionExecutable* executable, JSScope* scope)
{
    JSAsyncGeneratorFunction* asyncGenerator = createImpl(vm, executable, scope, scope->globalObject(vm)->asyncGeneratorFunctionStructure());
    executable->notifyCreation(vm, asyncGenerator, "Allocating an async generator");
    return asyncGenerator;
}

JSAsyncGeneratorFunction* JSAsyncGeneratorFunction::create(VM& vm, FunctionExecutable* executable, JSScope* scope, Structure* structure)
{
    JSAsyncGeneratorFunction* asyncGenerator = createImpl(vm, executable, scope, structure);
    executable->notifyCreation(vm, asyncGenerator, "Allocating an async generator");
    return asyncGenerator;
}

JSAsyncGeneratorFunction* JSAsyncGeneratorFunction::createWithInvalidatedReallocationWatchpoint(VM& vm, FunctionExecutable* executable, JSScope* scope)
{
    return createImpl(vm, executable, scope, scope->globalObject(vm)->asyncGeneratorFunctionStructure());
}

}
