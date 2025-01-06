/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSRawJSONObject.h"

#include "JSCInlines.h"
#include "PropertyNameArray.h"
#include "TypeError.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSRawJSONObject);

const ClassInfo JSRawJSONObject::s_info = { "Object"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSRawJSONObject) };

JSRawJSONObject::JSRawJSONObject(VM& vm, Structure* structure, Butterfly* butterfly)
    : Base(vm, structure, butterfly)
{
}

void JSRawJSONObject::finishCreation(VM& vm, JSString* string)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    putDirectOffset(vm, rawJSONObjectRawJSONPropertyOffset, string);
}

Structure* JSRawJSONObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    auto* structure = Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    structure->addPropertyWithoutTransition(
        vm, vm.propertyNames->rawJSON, PropertyAttribute::ReadOnly | PropertyAttribute::DontDelete,
        [&] (const GCSafeConcurrentJSLocker&, PropertyOffset offset, PropertyOffset newMaxOffset) {
            RELEASE_ASSERT(offset == rawJSONObjectRawJSONPropertyOffset);
            structure->setMaxOffset(vm, newMaxOffset);
        });
    structure->setDidPreventExtensions(true);
    return structure;
}

JSString* JSRawJSONObject::rawJSON(VM& vm)
{
    if (LIKELY(!structure()->didTransition()))
        return jsCast<JSString*>(getDirect(rawJSONObjectRawJSONPropertyOffset));
    return jsCast<JSString*>(getDirect(vm, vm.propertyNames->rawJSON));
}

} // namespace JSC
