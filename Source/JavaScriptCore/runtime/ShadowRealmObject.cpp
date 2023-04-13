/*
 * Copyright (C) 2021 Igalia S.L.
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ShadowRealmObject.h"

#include "AuxiliaryBarrierInlines.h"
#include "GlobalObjectMethodTable.h"
#include "JSObjectInlines.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ShadowRealmObject);

} // namespace JSC

namespace JSC {

const ClassInfo ShadowRealmObject::s_info = { "ShadowRealm"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ShadowRealmObject) };

ShadowRealmObject::ShadowRealmObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename Visitor>
void ShadowRealmObject::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    ShadowRealmObject* thisObject = jsCast<ShadowRealmObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);

    visitor.append(thisObject->m_globalObject);
}

DEFINE_VISIT_CHILDREN(ShadowRealmObject);

ShadowRealmObject* ShadowRealmObject::create(VM& vm, Structure* structure, JSGlobalObject* globalObject)
{
    ShadowRealmObject* object = new (NotNull, allocateCell<ShadowRealmObject>(vm)) ShadowRealmObject(vm, structure);
    object->finishCreation(vm);
    object->m_globalObject.set(vm, object, globalObject->globalObjectMethodTable()->deriveShadowRealmGlobalObject(globalObject));
    return object;
}

void ShadowRealmObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

} // namespace JSC
