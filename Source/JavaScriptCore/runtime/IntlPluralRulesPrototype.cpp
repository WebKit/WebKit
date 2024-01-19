/*
 * Copyright (C) 2018 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
#include "IntlPluralRulesPrototype.h"

#include "IntlPluralRules.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(intlPluralRulesPrototypeFuncSelect);
#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
static JSC_DECLARE_HOST_FUNCTION(intlPluralRulesPrototypeFuncSelectRange);
#endif
static JSC_DECLARE_HOST_FUNCTION(intlPluralRulesPrototypeFuncResolvedOptions);

}

#include "IntlPluralRulesPrototype.lut.h"

namespace JSC {

const ClassInfo IntlPluralRulesPrototype::s_info = { "Intl.PluralRules"_s, &Base::s_info, &pluralRulesPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlPluralRulesPrototype) };

/* Source for IntlPluralRulesPrototype.lut.h
@begin pluralRulesPrototypeTable
  select           intlPluralRulesPrototypeFuncSelect           DontEnum|Function 1
  resolvedOptions  intlPluralRulesPrototypeFuncResolvedOptions  DontEnum|Function 0
@end
*/

IntlPluralRulesPrototype* IntlPluralRulesPrototype::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    IntlPluralRulesPrototype* object = new (NotNull, allocateCell<IntlPluralRulesPrototype>(vm)) IntlPluralRulesPrototype(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* IntlPluralRulesPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlPluralRulesPrototype::IntlPluralRulesPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlPluralRulesPrototype::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
    UNUSED_PARAM(globalObject);
#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->selectRange, intlPluralRulesPrototypeFuncSelectRange, static_cast<unsigned>(PropertyAttribute::DontEnum), 2, ImplementationVisibility::Public);
#endif
}

JSC_DEFINE_HOST_FUNCTION(intlPluralRulesPrototypeFuncSelect, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.4.3 Intl.PluralRules.prototype.select (value)
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules.prototype.select
    IntlPluralRules* pluralRules = jsDynamicCast<IntlPluralRules*>(callFrame->thisValue());

    if (UNLIKELY(!pluralRules))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.PluralRules.prototype.select called on value that's not a PluralRules"_s));

    double value = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(pluralRules->select(globalObject, value)));
}

#if HAVE(ICU_U_NUMBER_RANGE_FORMATTER)
JSC_DEFINE_HOST_FUNCTION(intlPluralRulesPrototypeFuncSelectRange, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // https://tc39.es/proposal-intl-numberformat-v3/out/pluralrules/diff.html#sec-intl.pluralrules.prototype.selectrange
    IntlPluralRules* pluralRules = jsDynamicCast<IntlPluralRules*>(callFrame->thisValue());
    if (UNLIKELY(!pluralRules))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.PluralRules.prototype.selectRange called on value that's not a PluralRules"_s));

    JSValue startValue = callFrame->argument(0);
    JSValue endValue = callFrame->argument(1);

    if (startValue.isUndefined() || endValue.isUndefined())
        return throwVMTypeError(globalObject, scope, "start or end is undefined"_s);

    double start = startValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    double end = endValue.toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    RELEASE_AND_RETURN(scope, JSValue::encode(pluralRules->selectRange(globalObject, start, end)));
}
#endif

JSC_DEFINE_HOST_FUNCTION(intlPluralRulesPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.4.4 Intl.PluralRules.prototype.resolvedOptions ()
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
    IntlPluralRules* pluralRules = jsDynamicCast<IntlPluralRules*>(callFrame->thisValue());

    if (UNLIKELY(!pluralRules))
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.PluralRules.prototype.resolvedOptions called on value that's not a PluralRules"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(pluralRules->resolvedOptions(globalObject)));
}

} // namespace JSC
