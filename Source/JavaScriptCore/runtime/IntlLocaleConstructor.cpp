/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "IntlLocaleConstructor.h"

#include "IntlLocale.h"
#include "IntlLocalePrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlLocaleConstructor);

const ClassInfo IntlLocaleConstructor::s_info = { "Function", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(IntlLocaleConstructor) };

IntlLocaleConstructor* IntlLocaleConstructor::create(VM& vm, Structure* structure, IntlLocalePrototype* localePrototype)
{
    auto* constructor = new (NotNull, allocateCell<IntlLocaleConstructor>(vm.heap)) IntlLocaleConstructor(vm, structure);
    constructor->finishCreation(vm, localePrototype);
    return constructor;
}

Structure* IntlLocaleConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static EncodedJSValue JSC_HOST_CALL callIntlLocale(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructIntlLocale(JSGlobalObject*, CallFrame*);

IntlLocaleConstructor::IntlLocaleConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callIntlLocale, constructIntlLocale)
{
}

void IntlLocaleConstructor::finishCreation(VM& vm, IntlLocalePrototype* localePrototype)
{
    Base::finishCreation(vm, "Locale"_s, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, localePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    localePrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/ecma402/#sec-Intl.Locale
static EncodedJSValue JSC_HOST_CALL constructIntlLocale(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->localeStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->localeStructure());
    RETURN_IF_EXCEPTION(scope, { });

    IntlLocale* locale = IntlLocale::create(vm, structure);
    ASSERT(locale);

    JSValue tag = callFrame->argument(0);
    if (!tag.isString() && !tag.isObject())
        return throwVMTypeError(globalObject, scope, "First argument to Intl.Locale must be a string or an object"_s);

    scope.release();
    locale->initializeLocale(globalObject, tag, callFrame->argument(1));
    return JSValue::encode(locale);
}

// https://tc39.es/ecma402/#sec-Intl.Locale
static EncodedJSValue JSC_HOST_CALL callIntlLocale(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Locale"));
}

} // namespace JSC
