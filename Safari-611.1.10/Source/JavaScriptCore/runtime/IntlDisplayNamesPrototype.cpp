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
#include "IntlDisplayNamesPrototype.h"

#include "IntlDisplayNames.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(IntlDisplayNamesPrototypeFuncOf);
static JSC_DECLARE_HOST_FUNCTION(IntlDisplayNamesPrototypeFuncResolvedOptions);

}

#include "IntlDisplayNamesPrototype.lut.h"

namespace JSC {

const ClassInfo IntlDisplayNamesPrototype::s_info = { "Intl.DisplayNames", &Base::s_info, &displayNamesPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlDisplayNamesPrototype) };

/* Source for IntlDisplayNamesPrototype.lut.h
@begin displayNamesPrototypeTable
  of               IntlDisplayNamesPrototypeFuncOf                 DontEnum|Function 1
  resolvedOptions  IntlDisplayNamesPrototypeFuncResolvedOptions    DontEnum|Function 0
@end
*/

IntlDisplayNamesPrototype* IntlDisplayNamesPrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlDisplayNamesPrototype>(vm.heap)) IntlDisplayNamesPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlDisplayNamesPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlDisplayNamesPrototype::IntlDisplayNamesPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlDisplayNamesPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-intl-displaynames/#sec-Intl.DisplayNames.prototype.of
JSC_DEFINE_HOST_FUNCTION(IntlDisplayNamesPrototypeFuncOf, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* displayNames = jsDynamicCast<IntlDisplayNames*>(vm, callFrame->thisValue());
    if (!displayNames)
        return throwVMTypeError(globalObject, scope, "Intl.DisplayNames.prototype.of called on value that's not an object initialized as a DisplayNames"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(displayNames->of(globalObject, callFrame->argument(0))));
}

// https://tc39.es/proposal-intl-displaynames/#sec-Intl.DisplayNames.prototype.resolvedOptions
JSC_DEFINE_HOST_FUNCTION(IntlDisplayNamesPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* displayNames = jsDynamicCast<IntlDisplayNames*>(vm, callFrame->thisValue());
    if (!displayNames)
        return throwVMTypeError(globalObject, scope, "Intl.DisplayNames.prototype.resolvedOptions called on value that's not an object initialized as a DisplayNames"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(displayNames->resolvedOptions(globalObject)));
}

} // namespace JSC
