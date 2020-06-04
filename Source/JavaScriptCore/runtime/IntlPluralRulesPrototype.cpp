/*
 * Copyright (C) 2018 Andy VanWagoner (andy@vanwagoner.family)
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

static EncodedJSValue JSC_HOST_CALL IntlPluralRulesPrototypeFuncSelect(JSGlobalObject*, CallFrame*);
static EncodedJSValue JSC_HOST_CALL IntlPluralRulesPrototypeFuncResolvedOptions(JSGlobalObject*, CallFrame*);

}

#include "IntlPluralRulesPrototype.lut.h"

namespace JSC {

const ClassInfo IntlPluralRulesPrototype::s_info = { "Intl.PluralRules", &Base::s_info, &pluralRulesPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlPluralRulesPrototype) };

/* Source for IntlPluralRulesPrototype.lut.h
@begin pluralRulesPrototypeTable
  select           IntlPluralRulesPrototypeFuncSelect           DontEnum|Function 1
  resolvedOptions  IntlPluralRulesPrototypeFuncResolvedOptions  DontEnum|Function 0
@end
*/

IntlPluralRulesPrototype* IntlPluralRulesPrototype::create(VM& vm, JSGlobalObject*, Structure* structure)
{
    IntlPluralRulesPrototype* object = new (NotNull, allocateCell<IntlPluralRulesPrototype>(vm.heap)) IntlPluralRulesPrototype(vm, structure);
    object->finishCreation(vm);
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

void IntlPluralRulesPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

EncodedJSValue JSC_HOST_CALL IntlPluralRulesPrototypeFuncSelect(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.4.3 Intl.PluralRules.prototype.select (value)
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules.prototype.select
    IntlPluralRules* pluralRules = jsDynamicCast<IntlPluralRules*>(vm, callFrame->thisValue());

    if (!pluralRules)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.PluralRules.prototype.select called on value that's not an object initialized as a PluralRules"_s));

    double value = callFrame->argument(0).toNumber(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(pluralRules->select(globalObject, value)));
}

EncodedJSValue JSC_HOST_CALL IntlPluralRulesPrototypeFuncResolvedOptions(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    // 13.4.4 Intl.PluralRules.prototype.resolvedOptions ()
    // https://tc39.github.io/ecma402/#sec-intl.pluralrules.prototype.resolvedoptions
    IntlPluralRules* pluralRules = jsDynamicCast<IntlPluralRules*>(vm, callFrame->thisValue());

    if (!pluralRules)
        return JSValue::encode(throwTypeError(globalObject, scope, "Intl.PluralRules.prototype.resolvedOptions called on value that's not an object initialized as a PluralRules"_s));

    RELEASE_AND_RETURN(scope, JSValue::encode(pluralRules->resolvedOptions(globalObject)));
}

} // namespace JSC
