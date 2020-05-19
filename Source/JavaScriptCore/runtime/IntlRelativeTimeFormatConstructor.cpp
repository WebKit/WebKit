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
#include "IntlRelativeTimeFormatConstructor.h"

#include "IntlObject.h"
#include "IntlRelativeTimeFormat.h"
#include "IntlRelativeTimeFormatPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlRelativeTimeFormatConstructor);

static EncodedJSValue JSC_HOST_CALL IntlRelativeTimeFormatConstructorFuncSupportedLocalesOf(JSGlobalObject*, CallFrame*);

}

#include "IntlRelativeTimeFormatConstructor.lut.h"

namespace JSC {

const ClassInfo IntlRelativeTimeFormatConstructor::s_info = { "Function", &InternalFunction::s_info, &relativeTimeFormatConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlRelativeTimeFormatConstructor) };

/* Source for IntlRelativeTimeFormatConstructor.lut.h
@begin relativeTimeFormatConstructorTable
  supportedLocalesOf             IntlRelativeTimeFormatConstructorFuncSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlRelativeTimeFormatConstructor* IntlRelativeTimeFormatConstructor::create(VM& vm, Structure* structure, IntlRelativeTimeFormatPrototype* relativeTimeFormatPrototype)
{
    auto* constructor = new (NotNull, allocateCell<IntlRelativeTimeFormatConstructor>(vm.heap)) IntlRelativeTimeFormatConstructor(vm, structure);
    constructor->finishCreation(vm, relativeTimeFormatPrototype);
    return constructor;
}

Structure* IntlRelativeTimeFormatConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static EncodedJSValue JSC_HOST_CALL callIntlRelativeTimeFormat(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructIntlRelativeTimeFormat(JSGlobalObject*, CallFrame*);

IntlRelativeTimeFormatConstructor::IntlRelativeTimeFormatConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callIntlRelativeTimeFormat, constructIntlRelativeTimeFormat)
{
}

void IntlRelativeTimeFormatConstructor::finishCreation(VM& vm, IntlRelativeTimeFormatPrototype* relativeTimeFormatPrototype)
{
    Base::finishCreation(vm, "RelativeTimeFormat"_s, NameAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, relativeTimeFormatPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    relativeTimeFormatPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat
static EncodedJSValue JSC_HOST_CALL constructIntlRelativeTimeFormat(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->relativeTimeFormatStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->relativeTimeFormatStructure());
    RETURN_IF_EXCEPTION(scope, { });

    IntlRelativeTimeFormat* relativeTimeFormat = IntlRelativeTimeFormat::create(vm, structure);
    ASSERT(relativeTimeFormat);

    scope.release();
    relativeTimeFormat->initializeRelativeTimeFormat(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(relativeTimeFormat);
}

// https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat
static EncodedJSValue JSC_HOST_CALL callIntlRelativeTimeFormat(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "RelativeTimeFormat"));
}

// https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat.supportedLocalesOf
EncodedJSValue JSC_HOST_CALL IntlRelativeTimeFormatConstructorFuncSupportedLocalesOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& availableLocales = intlRelativeTimeFormatAvailableLocales();

    auto requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, availableLocales, requestedLocales, callFrame->argument(1))));
}

} // namespace JSC
