/*
 * Copyright (C) 2025 Codeblog CORP.
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
#include "JSPromiseAllContext.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSPromiseAllContext::s_info = { "PromiseAllContext"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromiseAllContext) };

JSPromiseAllContext* JSPromiseAllContext::createWithInitialValues(VM& vm, Structure* structure)
{
    auto values = initialValues();
    JSPromiseAllContext* context = new (NotNull, allocateCell<JSPromiseAllContext>(vm)) JSPromiseAllContext(vm, structure);
    context->finishCreation(vm, values[0], values[1]);
    return context;
}

JSPromiseAllContext* JSPromiseAllContext::create(VM& vm, Structure* structure, JSValue globalContext, JSValue index)
{
    JSPromiseAllContext* result = new (NotNull, allocateCell<JSPromiseAllContext>(vm)) JSPromiseAllContext(vm, structure);
    result->finishCreation(vm, globalContext, index);
    return result;
}

void JSPromiseAllContext::finishCreation(VM& vm, JSValue globalContext, JSValue index)
{
    Base::finishCreation(vm);
    this->setGlobalContext(vm, globalContext);
    this->setIndex(vm, index);
}

template<typename Visitor>
void JSPromiseAllContext::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSPromiseAllContext*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

DEFINE_VISIT_CHILDREN(JSPromiseAllContext);

JSC_DEFINE_HOST_FUNCTION(promiseAllContextPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return JSValue::encode(JSPromiseAllContext::create(globalObject->vm(), globalObject->promiseAllContextStructure(), callFrame->uncheckedArgument(0), callFrame->uncheckedArgument(1)));
}

} // namespace JSC
