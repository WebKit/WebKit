/*
 * Copyright (C) 2013 Apple, Inc. All rights reserved.
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
#include "JSArrayIterator.h"

#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSArrayIterator::s_info = { "Array Iterator", &Base::s_info, 0, CREATE_METHOD_TABLE(JSArrayIterator) };

void JSArrayIterator::finishCreation(VM& vm, JSGlobalObject*, ArrayIterationKind kind, JSObject* iteratedObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    putDirect(vm, vm.propertyNames->iteratedObjectPrivateName, iteratedObject);
    putDirect(vm, vm.propertyNames->arrayIteratorNextIndexPrivateName, jsNumber(0));
    putDirect(vm, vm.propertyNames->arrayIterationKindPrivateName, jsNumber(kind));
}

ArrayIterationKind JSArrayIterator::kind(ExecState* exec) const
{
    JSValue kindValue = getDirect(exec->vm(), exec->vm().propertyNames->arrayIterationKindPrivateName);
    return static_cast<ArrayIterationKind>(kindValue.asInt32());
}

JSValue JSArrayIterator::iteratedValue(ExecState* exec) const
{
    return getDirect(exec->vm(), exec->vm().propertyNames->iteratedObjectPrivateName);
}

JSArrayIterator* JSArrayIterator::clone(ExecState* exec)
{
    VM& vm = exec->vm();
    JSValue iteratedObject = getDirect(vm, vm.propertyNames->iteratedObjectPrivateName);
    JSValue nextIndex = getDirect(vm, vm.propertyNames->arrayIteratorNextIndexPrivateName);

    auto clone = JSArrayIterator::create(exec, exec->callee()->globalObject()->arrayIteratorStructure(), kind(exec), asObject(iteratedObject));
    clone->putDirect(vm, vm.propertyNames->arrayIteratorNextIndexPrivateName, nextIndex);
    return clone;
}

}
