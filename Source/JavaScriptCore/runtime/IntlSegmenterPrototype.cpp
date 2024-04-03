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
#include "IntlSegmenterPrototype.h"

#include "IntlSegmenter.h"
#include "JSCInlines.h"

namespace JSC {

static JSC_DECLARE_HOST_FUNCTION(intlSegmenterPrototypeFuncSegment);
static JSC_DECLARE_HOST_FUNCTION(intlSegmenterPrototypeFuncResolvedOptions);

}

#include "IntlSegmenterPrototype.lut.h"

namespace JSC {

const ClassInfo IntlSegmenterPrototype::s_info = { "Intl.Segmenter"_s, &Base::s_info, &segmenterPrototypeTable, nullptr, CREATE_METHOD_TABLE(IntlSegmenterPrototype) };

/* Source for IntlSegmenterPrototype.lut.h
@begin segmenterPrototypeTable
  segment          intlSegmenterPrototypeFuncSegment            DontEnum|Function 1
  resolvedOptions  intlSegmenterPrototypeFuncResolvedOptions    DontEnum|Function 0
@end
*/

IntlSegmenterPrototype* IntlSegmenterPrototype::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<IntlSegmenterPrototype>(vm)) IntlSegmenterPrototype(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* IntlSegmenterPrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

IntlSegmenterPrototype::IntlSegmenterPrototype(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

void IntlSegmenterPrototype::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

// https://tc39.es/proposal-intl-segmenter/#sec-intl.segmenter.prototype.segment
JSC_DEFINE_HOST_FUNCTION(intlSegmenterPrototypeFuncSegment, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* segmenter = jsDynamicCast<IntlSegmenter*>(callFrame->thisValue());
    if (UNLIKELY(!segmenter))
        return throwVMTypeError(globalObject, scope, "Intl.Segmenter.prototype.segment called on value that's not a Segmenter"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(segmenter->segment(globalObject, callFrame->argument(0))));
}

// https://tc39.es/proposal-intl-segmenter/#sec-Intl.Segmenter.prototype.resolvedOptions
JSC_DEFINE_HOST_FUNCTION(intlSegmenterPrototypeFuncResolvedOptions, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* segmenter = jsDynamicCast<IntlSegmenter*>(callFrame->thisValue());
    if (UNLIKELY(!segmenter))
        return throwVMTypeError(globalObject, scope, "Intl.Segmenter.prototype.resolvedOptions called on value that's not a Segmenter"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(segmenter->resolvedOptions(globalObject)));
}

} // namespace JSC
