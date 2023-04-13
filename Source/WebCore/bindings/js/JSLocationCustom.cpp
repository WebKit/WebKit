/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003-2022 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "JSLocation.h"

#include "JSDOMBinding.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMExceptionHandling.h"
#include "JSLocalDOMWindowCustom.h"
#include "RuntimeApplicationChecks.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSFunction.h>
#include <JavaScriptCore/Lookup.h>

namespace WebCore {
using namespace JSC;

static bool getOwnPropertySlotCommon(JSLocation& thisObject, JSGlobalObject& lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* window = thisObject.wrapped().window();

    // When accessing Location cross-domain, functions are always the native built-in ones.
    // See JSLocalDOMWindow::getOwnPropertySlotDelegate for additional details.

    // Our custom code is only needed to implement the Window cross-domain scheme, so if access is
    // allowed, return false so the normal lookup will take place.
    String message;
    if (BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, window, message))
        return false;

    // https://html.spec.whatwg.org/#crossorigingetownpropertyhelper-(-o,-p-)

    // We only allow access to Location.replace() cross origin.
    if (propertyName == vm.propertyNames->replace) {
        auto* entry = JSLocation::info()->staticPropHashTable->entry(propertyName);
        auto* jsFunction = thisObject.globalObject()->createCrossOriginFunction(&lexicalGlobalObject, propertyName, entry->function(), entry->functionLength());
        slot.setValue(&thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, jsFunction);
        return true;
    }

    // Getting location.href cross origin needs to throw. However, getOwnPropertyDescriptor() needs to return
    // a descriptor that has a setter but no getter.
    if (slot.internalMethodType() == PropertySlot::InternalMethodType::GetOwnProperty && propertyName == builtinNames(vm).hrefPublicName()) {
        auto* entry = JSLocation::info()->staticPropHashTable->entry(propertyName);
        auto* getterSetter = thisObject.globalObject()->createCrossOriginGetterSetter(&lexicalGlobalObject, propertyName, nullptr, entry->propertyPutter());
        slot.setGetterSlot(&thisObject, PropertyAttribute::Accessor | PropertyAttribute::DontEnum, getterSetter);
        return true;
    }

    if (handleCommonCrossOriginProperties(&thisObject, vm, propertyName, slot))
        return true;

    throwSecurityError(lexicalGlobalObject, scope, message);
    slot.setUndefined();
    return false;
}

bool JSLocation::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* thisObject = jsCast<JSLocation*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    bool result = getOwnPropertySlotCommon(*thisObject, *lexicalGlobalObject, propertyName, slot);
    EXCEPTION_ASSERT(!scope.exception() || !result);
    RETURN_IF_EXCEPTION(scope, false);
    if (result)
        return true;
    RELEASE_AND_RETURN(scope, JSObject::getOwnPropertySlot(object, lexicalGlobalObject, propertyName, slot));
}

bool JSLocation::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* lexicalGlobalObject, unsigned index, PropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* thisObject = jsCast<JSLocation*>(object);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    bool result = getOwnPropertySlotCommon(*thisObject, *lexicalGlobalObject, Identifier::from(vm, index), slot);
    EXCEPTION_ASSERT(!scope.exception() || !result);
    RETURN_IF_EXCEPTION(scope, false);
    if (result)
        return true;
    RELEASE_AND_RETURN(scope, JSObject::getOwnPropertySlotByIndex(object, lexicalGlobalObject, index, slot));
}

bool JSLocation::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& putPropertySlot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* thisObject = jsCast<JSLocation*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    // Always allow assigning to the whole location.
    // However, allowing assigning of pieces might inadvertently disclose parts of the original location.
    // So fall through to the access check for those.
    String errorMessage;
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(*lexicalGlobalObject, thisObject->wrapped().window(), errorMessage)) {
        if (propertyName == builtinNames(vm).hrefPublicName()) {
            auto setter = s_info.staticPropHashTable->entry(propertyName)->propertyPutter();
            scope.release();
            setter(lexicalGlobalObject, JSValue::encode(putPropertySlot.thisValue()), JSValue::encode(value), propertyName);
            return true;
        }
        throwSecurityError(*lexicalGlobalObject, scope, errorMessage);
        return false;
    }

    scope.release();
    return JSObject::put(thisObject, lexicalGlobalObject, propertyName, value, putPropertySlot);
}

bool JSLocation::putByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned index, JSValue value, bool shouldThrow)
{
    auto* thisObject = jsCast<JSLocation*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());

    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped().window(), ThrowSecurityError))
        return false;

    return JSObject::putByIndex(cell, lexicalGlobalObject, index, value, shouldThrow);
}

bool JSLocation::deleteProperty(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    JSLocation* thisObject = jsCast<JSLocation*>(cell);
    // Only allow deleting by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped().window(), ThrowSecurityError))
        return false;
    return Base::deleteProperty(thisObject, lexicalGlobalObject, propertyName, slot);
}

bool JSLocation::deletePropertyByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned propertyName)
{
    JSLocation* thisObject = jsCast<JSLocation*>(cell);
    // Only allow deleting by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped().window(), ThrowSecurityError))
        return false;
    return Base::deletePropertyByIndex(thisObject, lexicalGlobalObject, propertyName);
}

void JSLocation::getOwnPropertyNames(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped().window(), DoNotReportSecurityError)) {
        if (mode == DontEnumPropertiesMode::Include)
            addCrossOriginOwnPropertyNames<CrossOriginObject::Location>(*lexicalGlobalObject, propertyNames);
        return;
    }
    Base::getOwnPropertyNames(thisObject, lexicalGlobalObject, propertyNames, mode);
}

bool JSLocation::defineOwnProperty(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped().window(), ThrowSecurityError))
        return false;

    return Base::defineOwnProperty(object, lexicalGlobalObject, propertyName, descriptor, throwException);
}

JSValue JSLocation::getPrototype(JSObject* object, JSGlobalObject* lexicalGlobalObject)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped().window(), DoNotReportSecurityError))
        return jsNull();

    return Base::getPrototype(object, lexicalGlobalObject);
}

bool JSLocation::preventExtensions(JSObject*, JSGlobalObject*)
{
    return false;
}

} // namespace WebCore
