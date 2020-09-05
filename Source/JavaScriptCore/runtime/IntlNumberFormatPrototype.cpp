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
#include "IntlNumberFormatPrototype.h"

#include "BuiltinNames.h"
#include "IntlNumberFormat.h"
#include "JSBoundFunction.h"
#include "JSCInlines.h"

namespace JSC {

static EncodedJSValue JSC_HOST_CALL IntlNumberFormatPrototypeGetterFormat(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlNumberFormatPrototypeFuncFormatToParts(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlNumberFormatPrototypeFuncResolvedOptions(JSGlobalObject*, CallFrame*);

}

#include "IntlNumberFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlNumberFormatPrototype::s_info = { "Intl.NumberFormat", &Base::s_info, &numberFormatPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlNumberFormatPrototype) };

/* Source for IntlNumberFormatPrototype.lut.h
@begin numberFormatPrototypeTable
  format           IntlNumberFormatPrototypeGetterFormat         DontEnum|Accessor
  formatToParts    IntlNumberFormatPrototypeFuncFormatToParts    DontEnum|Function 1
  resolvedOptions  IntlNumberFormatPrototypeFuncResolvedOptions  DontEnum|Function 0
@end
*/

IntlNumberFormatPrototype* IntlNumberFormatPrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    IntlNumberFormatPrototype* object = new (NotNull, allocateCell<IntlNumberFormatPrototype>(vm.heap)) IntlNumberFormatPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlNumberFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlNumberFormatPrototype::IntlNumberFormatPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlNumberFormatPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/ecma402/#sec-number-format-functions
static EncodedJSValue JSC_HOST_CALL IntlNumberFormatFuncFormat(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* numberFormat = jsCast<IntlNumberFormat*>(callFrame->thisValue());

    JSValue bigIntOrNumber = callFrame->argument(0).toNumeric(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    scope.release();
    if (bigIntOrNumber.isNumber()) {
        double value = bigIntOrNumber.asNumber();
        return JSValue::encode(numberFormat->format(globalObject, value));
    }

#if USE(BIGINT32)
    if (bigIntOrNumber.isBigInt32()) {
        JSBigInt* value = JSBigInt::createFrom(globalObject, bigIntOrNumber.bigInt32AsInt32());
        RETURN_IF_EXCEPTION(scope, { });
        RELEASE_AND_RETURN(scope, JSValue::encode(numberFormat->format(globalObject, value)));
    }
#endif

    ASSERT(bigIntOrNumber.isHeapBigInt());
    JSBigInt* value = bigIntOrNumber.asHeapBigInt();
    return JSValue::encode(numberFormat->format(globalObject, value));
}

EncodedJSValue JSC_HOST_CALL IntlNumberFormatPrototypeGetterFormat(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 11.3.3 Intl.NumberFormat.prototype.format (ECMA-402 2.0)
    // 1. Let nf be this NumberFormat object.
    auto* nf = IntlNumberFormat::unwrapForOldFunctions(globalObject, callFrame->thisValue());
    if (UNLIKELY(!nf))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.NumberFormat.prototype.format called on value that's not an object initialized as a NumberFormat"_s));

    JSBoundFunction* boundFormat = nf->boundFormat();
    // 2. If nf.[[boundFormat]] is undefined,
    if (!boundFormat) {
        JSGlobalObject* globalObject = nf->globalObject(vm);
        // a. Let F be a new built-in function object as defined in 11.3.4.
        // b. The value of F’s length property is 1.
        auto* targetObject = JSFunction::create(vm, globalObject, 1, "format"_s, IntlNumberFormatFuncFormat, NoIntrinsic);
        // c. Let bf be BoundFunctionCreate(F, «this value»).
        boundFormat = JSBoundFunction::create(vm, globalObject, targetObject, nf, nullptr, 1, nullptr);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        // d. Set nf.[[boundFormat]] to bf.
        nf->setBoundFormat(vm, boundFormat);
    }
    // 3. Return nf.[[boundFormat]].
    return JSValue::encode(boundFormat);
}

EncodedJSValue JSC_HOST_CALL IntlNumberFormatPrototypeFuncFormatToParts(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // Intl.NumberFormat.prototype.formatToParts (ECMA-402)
    // https://tc39.github.io/ecma402/#sec-intl.numberformat.prototype.formattoparts

    // Do not use unwrapForOldFunctions.
    auto* numberFormat = jsDynamicCast<IntlNumberFormat*>(vm, callFrame->thisValue());
    if (UNLIKELY(!numberFormat))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.NumberFormat.prototype.formatToParts called on value that's not an object initialized as a NumberFormat"_s));

    double value = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(numberFormat->formatToParts(globalObject, value)));
}

EncodedJSValue JSC_HOST_CALL IntlNumberFormatPrototypeFuncResolvedOptions(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 11.3.5 Intl.NumberFormat.prototype.resolvedOptions() (ECMA-402 2.0)

    auto* numberFormat = IntlNumberFormat::unwrapForOldFunctions(globalObject, callFrame->thisValue());
    if (UNLIKELY(!numberFormat))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.NumberFormat.prototype.resolvedOptions called on value that's not an object initialized as a NumberFormat"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(numberFormat->resolvedOptions(globalObject)));
}

} // namespace JSC
