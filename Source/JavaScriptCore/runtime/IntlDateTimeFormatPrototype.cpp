/*
 * Copyright (C) 2015 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "IntlDateTimeFormatPrototype.h"

#if ENABLE(INTL)

#include "BuiltinNames.h"
#include "DateConstructor.h"
#include "Error.h"
#include "IntlDateTimeFormat.h"
#include "IntlObject.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"
#include "JSObjectInlines.h"
#include <wtf/DateMath.h>

namespace JSC {

static EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeGetterFormat(JSGlobalObject*, CallFrame*);
#if JSC_ICU_HAS_UFIELDPOSITER
static EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeFuncFormatToParts(JSGlobalObject*, CallFrame*);
#endif
static EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeFuncResolvedOptions(JSGlobalObject*, CallFrame*);

}

#include "IntlDateTimeFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlDateTimeFormatPrototype::s_info = { "Object", &Base::s_info, &dateTimeFormatPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlDateTimeFormatPrototype) };

/* Source for IntlDateTimeFormatPrototype.lut.h
@begin dateTimeFormatPrototypeTable
  format           IntlDateTimeFormatPrototypeGetterFormat         DontEnum|Accessor
  resolvedOptions  IntlDateTimeFormatPrototypeFuncResolvedOptions  DontEnum|Function 0
@end
*/

IntlDateTimeFormatPrototype* IntlDateTimeFormatPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    IntlDateTimeFormatPrototype* object = new (NotNull, allocateCell<IntlDateTimeFormatPrototype>(vm.heap)) IntlDateTimeFormatPrototype(vm, structure);
    object->finishCreation(vm, globalObject, structure);
    return object;
}

Structure* IntlDateTimeFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDateTimeFormatPrototype::IntlDateTimeFormatPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlDateTimeFormatPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject, Structure*)
{
    Base::finishCreation(vm);
#if JSC_ICU_HAS_UFIELDPOSITER
    JSFunction* formatToPartsFunction = JSFunction::create(vm, globalObject, 1, vm.propertyNames->formatToParts.string(), IntlDateTimeFormatPrototypeFuncFormatToParts);
    putDirectWithoutTransition(vm, vm.propertyNames->formatToParts, formatToPartsFunction, static_cast<unsigned>(PropertyAttribute::DontEnum));
#else
    UNUSED_PARAM(globalObject);
#endif

    putDirectWithoutTransition(vm, vm.propertyNames->toStringTagSymbol, jsNontrivialString(vm, "Object"_s), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);
}

static EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatFuncFormatDateTime(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    // 12.1.7 DateTime Format Functions (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-formatdatetime

    IntlDateTimeFormat* format = jsCast<IntlDateTimeFormat*>(callFrame->thisValue());

    JSValue date = callFrame->argument(0);
    double value;

    if (date.isUndefined())
        value = dateNowImpl().toNumber(globalObject);
    else {
        value = WTF::timeClip(date.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(format->format(globalObject, value)));
}

EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeGetterFormat(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.3 Intl.DateTimeFormat.prototype.format (ECMA-402 2.0)
    // 1. Let dtf be this DateTimeFormat object.
    IntlDateTimeFormat* dtf = jsDynamicCast<IntlDateTimeFormat*>(vm, callFrame->thisValue());

    // FIXME: Workaround to provide compatibility with ECMA-402 1.0 call/apply patterns.
    // https://bugs.webkit.org/show_bug.cgi?id=153679
    if (!dtf) {
        JSValue value = callFrame->thisValue().get(globalObject, vm.propertyNames->builtinNames().intlSubstituteValuePrivateName());
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        dtf = jsDynamicCast<IntlDateTimeFormat*>(vm, value);
    }

    // 2. ReturnIfAbrupt(dtf).
    if (!dtf)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.format called on value that's not an object initialized as a DateTimeFormat"_s));

    JSBoundFunction* boundFormat = dtf->boundFormat();
    // 3. If the [[boundFormat]] internal slot of this DateTimeFormat object is undefined,
    if (!boundFormat) {
        JSGlobalObject* globalObject = dtf->globalObject(vm);
        // a. Let F be a new built-in function object as defined in 12.3.4.
        // b. The value of F’s length property is 1. (Note: F’s length property was 0 in ECMA-402 1.0)
        JSFunction* targetObject = JSFunction::create(vm, globalObject, 1, "format"_s, IntlDateTimeFormatFuncFormatDateTime, NoIntrinsic);
        // c. Let bf be BoundFunctionCreate(F, «this value»).
        boundFormat = JSBoundFunction::create(vm, globalObject, targetObject, dtf, nullptr, 1, nullptr);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        // d. Set dtf.[[boundFormat]] to bf.
        dtf->setBoundFormat(vm, boundFormat);
    }
    // 4. Return dtf.[[boundFormat]].
    return JSValue::encode(boundFormat);
}

#if JSC_ICU_HAS_UFIELDPOSITER
EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeFuncFormatToParts(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 15.4 Intl.DateTimeFormat.prototype.formatToParts (ECMA-402 4.0)
    // https://tc39.github.io/ecma402/#sec-Intl.DateTimeFormat.prototype.formatToParts

    IntlDateTimeFormat* dateTimeFormat = jsDynamicCast<IntlDateTimeFormat*>(vm, callFrame->thisValue());
    if (!dateTimeFormat)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.formatToParts called on value that's not an object initialized as a DateTimeFormat"_s));

    JSValue date = callFrame->argument(0);
    double value;

    if (date.isUndefined())
        value = dateNowImpl().toNumber(globalObject);
    else {
        value = WTF::timeClip(date.toNumber(globalObject));
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
    }

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->formatToParts(globalObject, value)));
}
#endif

EncodedJSValue JSC_HOST_CALL IntlDateTimeFormatPrototypeFuncResolvedOptions(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 12.3.5 Intl.DateTimeFormat.prototype.resolvedOptions() (ECMA-402 2.0)
    IntlDateTimeFormat* dateTimeFormat = jsDynamicCast<IntlDateTimeFormat*>(vm, callFrame->thisValue());

    // FIXME: Workaround to provide compatibility with ECMA-402 1.0 call/apply patterns.
    // https://bugs.webkit.org/show_bug.cgi?id=153679
    if (!dateTimeFormat) {
        JSValue value = callFrame->thisValue().get(globalObject, vm.propertyNames->builtinNames().intlSubstituteValuePrivateName());
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        dateTimeFormat = jsDynamicCast<IntlDateTimeFormat*>(vm, value);
    }

    if (!dateTimeFormat)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.DateTimeFormat.prototype.resolvedOptions called on value that's not an object initialized as a DateTimeFormat"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(dateTimeFormat->resolvedOptions(globalObject)));
}

} // namespace JSC

#endif // ENABLE(INTL)
