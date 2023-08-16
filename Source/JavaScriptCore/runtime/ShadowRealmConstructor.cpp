/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "ShadowRealmConstructor.h"

#include "JSCInlines.h"
#include "ShadowRealmObjectInlines.h"

namespace JSC {

const ClassInfo ShadowRealmConstructor::s_info = { "Function"_s, &InternalFunction::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ShadowRealmConstructor) };

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ShadowRealmConstructor);

static JSC_DECLARE_HOST_FUNCTION(callShadowRealm);
static JSC_DECLARE_HOST_FUNCTION(constructWithShadowRealmConstructor);

ShadowRealmConstructor::ShadowRealmConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure, callShadowRealm, constructWithShadowRealmConstructor)
{
}

void ShadowRealmConstructor::finishCreation(VM& vm, ShadowRealmPrototype* shadowRealmPrototype)
{
    Base::finishCreation(vm, 0, vm.propertyNames->ShadowRealm.string(), PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, shadowRealmPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
}

JSC_DEFINE_HOST_FUNCTION(constructWithShadowRealmConstructor, (JSGlobalObject* globalObject, CallFrame*))
{
    VM& vm = globalObject->vm();
    Structure* shadowRealmStructure = ShadowRealmObject::createStructure(vm, globalObject, globalObject->shadowRealmPrototype());
    JSObject* shadowRealmObject = ShadowRealmObject::create(vm, shadowRealmStructure, globalObject);
    return JSValue::encode(shadowRealmObject);
}

JSC_DEFINE_HOST_FUNCTION(callShadowRealm, (JSGlobalObject* globalObject, CallFrame*))
{
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "ShadowRealm"));
}

} // namespace JSC
