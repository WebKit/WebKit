/*
 * Copyright (C) 2021 Igalia S.L. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "ShadowRealmObject.h"

#include "ObjectPrototype.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(ShadowRealmObject);

} // namespace JSC

namespace JSC {

const ClassInfo ShadowRealmObject::s_info = { "ShadowRealm", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ShadowRealmObject) };

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

ShadowRealmObject* ShadowRealmObject::create(VM& vm, Structure* structure, const GlobalObjectMethodTable* methodTable)
{
    ShadowRealmObject* object = new (NotNull, allocateCell<ShadowRealmObject>(vm.heap)) ShadowRealmObject(vm, structure);
    object->finishCreation(vm);
    JSGlobalObject* globalObject = JSGlobalObject::create(vm, JSGlobalObject::createStructure(vm, jsNull()), methodTable);
    object->m_globalObject.set(vm, object, globalObject);
    return object;
}

void ShadowRealmObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

} // namespace JSC
