/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#include "IntlListFormatPrototype.h"

#include "IntlListFormat.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(intlListFormatPrototypeFuncFormat);
static JSC_DECLARE_HOST_FUNCTION(intlListFormatPrototypeFuncFormatToParts);
static JSC_DECLARE_HOST_FUNCTION(intlListFormatPrototypeFuncResolvedOptions);

}

#include "IntlListFormatPrototype.lut.h"

namespace JSC {

const ClassInfo IntlListFormatPrototype::s_info = { "Intl.ListFormat"_s, &Base::s_info, &listFormatPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlListFormatPrototype) };

/* Source for IntlListFormatPrototype.lut.h
@begin listFormatPrototypeTable
  format           intlListFormatPrototypeFuncFormat             DontEnum|Function 1
  formatToParts    intlListFormatPrototypeFuncFormatToParts      DontEnum|Function 1
  resolvedOptions  intlListFormatPrototypeFuncResolvedOptions    DontEnum|Function 0
@end
*/

IntlListFormatPrototype* IntlListFormatPrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlListFormatPrototype>(vm)) IntlListFormatPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlListFormatPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlListFormatPrototype::IntlListFormatPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlListFormatPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat.prototype.format
JSC_DEFINE_HOST_FUNCTION(intlListFormatPrototypeFuncFormat, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* listFormat = jsDynamicCast<IntlListFormat*>(callFrame->thisValue());
    if (UNLIKELY(!listFormat))
        return throwVMTypeError(globalObject, scope, "Intl.ListFormat.prototype.format called on value that's not a ListFormat"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(listFormat->format(globalObject, callFrame->argument(0))));
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat.prototype.formatToParts
JSC_DEFINE_HOST_FUNCTION(intlListFormatPrototypeFuncFormatToParts, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* listFormat = jsDynamicCast<IntlListFormat*>(callFrame->thisValue());
    if (UNLIKELY(!listFormat))
        return throwVMTypeError(globalObject, scope, "Intl.ListFormat.prototype.formatToParts called on value that's not a ListFormat"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(listFormat->formatToParts(globalObject, callFrame->argument(0))));
}

// https://tc39.es/proposal-intl-list-format/#sec-Intl.ListFormat.prototype.resolvedOptions
JSC_DEFINE_HOST_FUNCTION(intlListFormatPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* listFormat = jsDynamicCast<IntlListFormat*>(callFrame->thisValue());
    if (UNLIKELY(!listFormat))
        return throwVMTypeError(globalObject, scope, "Intl.ListFormat.prototype.resolvedOptions called on value that's not a ListFormat"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(listFormat->resolvedOptions(globalObject)));
}

} // namespace JSC
