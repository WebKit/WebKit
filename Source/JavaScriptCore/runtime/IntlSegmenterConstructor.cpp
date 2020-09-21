/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "IntlSegmenterConstructor.h"

#include "IntlSegmenter.h"
#include "IntlSegmenterPrototype.h"
#include "JSCInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(IntlSegmenterConstructor);

static EncodedJSValue JSC_HOST_CALL IntlSegmenterConstructorSupportedLocalesOf(JSGlobalObject*, CallFrame*);

}

#include "IntlSegmenterConstructor.lut.h"

namespace JSC {

const ClassInfo IntlSegmenterConstructor::s_info = { "Function", &Base::s_info, &segmenterConstructorTable, nullptr, CREATE_METHOD_TABLE(IntlSegmenterConstructor) };

/* Source for IntlSegmenterConstructor.lut.h
@begin segmenterConstructorTable
  supportedLocalesOf             IntlSegmenterConstructorSupportedLocalesOf             DontEnum|Function 1
@end
*/

IntlSegmenterConstructor* IntlSegmenterConstructor::create(VM& vm, Structure* structure, IntlSegmenterPrototype* segmenterPrototype)
{
    auto* constructor = new (NotNull, allocateCell<IntlSegmenterConstructor>(vm.heap)) IntlSegmenterConstructor(vm, structure);
    constructor->finishCreation(vm, segmenterPrototype);
    return constructor;
}

Structure* IntlSegmenterConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(InternalFunctionType, StructureFlags), info());
}

static EncodedJSValue JSC_HOST_CALL callIntlSegmenter(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL constructIntlSegmenter(JSGlobalObject*, CallFrame*);

IntlSegmenterConstructor::IntlSegmenterConstructor(VM& vm, Structure* structure)
    : Base(vm, structure, callIntlSegmenter, constructIntlSegmenter)
{
}

void IntlSegmenterConstructor::finishCreation(VM& vm, IntlSegmenterPrototype* segmenterPrototype)
{
    Base::finishCreation(vm, 0, "Segmenter"_s, PropertyAdditionMode::WithoutStructureTransition);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, segmenterPrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    segmenterPrototype->putDirectWithoutTransition(vm, vm.propertyNames->constructor, this, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

// https://tc39.es/ecma402/#sec-Intl.Segmenter
static EncodedJSValue JSC_HOST_CALL constructIntlSegmenter(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* newTarget = asObject(callFrame->newTarget());
    Structure* structure = newTarget == callFrame->jsCallee()
        ? globalObject->segmenterStructure()
        : InternalFunction::createSubclassStructure(globalObject, newTarget, getFunctionRealm(vm, newTarget)->segmenterStructure());
    RETURN_IF_EXCEPTION(scope, { });

    IntlSegmenter* segmenter = IntlSegmenter::create(vm, structure);
    ASSERT(segmenter);

    scope.release();
    segmenter->initializeSegmenter(globalObject, callFrame->argument(0), callFrame->argument(1));
    return JSValue::encode(segmenter);
}

// https://tc39.es/ecma402/#sec-Intl.Segmenter
static EncodedJSValue JSC_HOST_CALL callIntlSegmenter(JSGlobalObject* globalObject, CallFrame*)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    return JSValue::encode(throwConstructorCannotBeCalledAsFunctionTypeError(globalObject, scope, "Segmenter"));
}

EncodedJSValue JSC_HOST_CALL IntlSegmenterConstructorSupportedLocalesOf(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // https://tc39.es/proposal-intl-segmenter/#sec-intl.segmenter.supportedlocalesof

    // 1. Let availableLocales be %Segmenter%.[[availableLocales]].
    const HashSet<String>& availableLocales = intlSegmenterAvailableLocales();

    // 2. Let requestedLocales be CanonicalizeLocaleList(locales).
    Vector<String> requestedLocales = canonicalizeLocaleList(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    // 3. Return SupportedLocales(availableLocales, requestedLocales, options).
    RELEASE_AND_RETURN(scope, JSValue::encode(supportedLocales(globalObject, availableLocales, requestedLocales, callFrame->argument(1))));
}


} // namespace JSC
