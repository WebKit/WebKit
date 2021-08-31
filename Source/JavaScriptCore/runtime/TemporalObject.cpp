/*
 *  Copyright (C) 2021 Igalia S.L. All rights reserved.
 *  Copyright (C) 2021 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "TemporalObject.h"

#include "FunctionPrototype.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObject.h"
#include "JSObjectInlines.h"
#include "ObjectPrototype.h"
#include "TemporalCalendarConstructor.h"
#include "TemporalCalendarPrototype.h"
#include "TemporalNow.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(TemporalObject);

static JSValue createCalendarConstructor(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject(vm);
    return TemporalCalendarConstructor::create(vm, TemporalCalendarConstructor::createStructure(vm, globalObject, globalObject->functionPrototype()), jsCast<TemporalCalendarPrototype*>(globalObject->calendarStructure()->storedPrototypeObject()));
}

static JSValue createNowObject(VM& vm, JSObject* object)
{
    TemporalObject* temporalObject = jsCast<TemporalObject*>(object);
    JSGlobalObject* globalObject = temporalObject->globalObject(vm);
    return TemporalNow::create(vm, TemporalNow::createStructure(vm, globalObject));
}

} // namespace JSC

#include "TemporalObject.lut.h"

namespace JSC {

/* Source for TemporalObject.lut.h
@begin temporalObjectTable
  Calendar       createCalendarConstructor       DontEnum|PropertyCallback
  Now            createNowObject                 DontEnum|PropertyCallback
@end
*/

const ClassInfo TemporalObject::s_info = { "Temporal", &Base::s_info, &temporalObjectTable, nullptr, CREATE_METHOD_TABLE(TemporalObject) };

TemporalObject::TemporalObject(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

TemporalObject* TemporalObject::create(VM& vm, Structure* structure)
{
    TemporalObject* object = new (NotNull, allocateCell<TemporalObject>(vm.heap)) TemporalObject(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* TemporalObject::createStructure(VM& vm, JSGlobalObject* globalObject)
{
    return Structure::create(vm, globalObject, globalObject->objectPrototype(), TypeInfo(ObjectType, StructureFlags), info());
}

void TemporalObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(vm, info()));
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

} // namespace JSC
