/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights reseved.
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
#include "RuntimeApplicationChecks.h"
#include <runtime/JSFunction.h>
#include <runtime/Lookup.h>

using namespace JSC;

namespace WebCore {

bool JSLocation::getOwnPropertySlotDelegate(ExecState* state, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    Frame* frame = wrapped().frame();
    if (!frame) {
        slot.setUndefined();
        return true;
    }

    // When accessing Location cross-domain, functions are always the native built-in ones.
    // See JSDOMWindow::getOwnPropertySlotDelegate for additional details.

    // Our custom code is only needed to implement the Window cross-domain scheme, so if access is
    // allowed, return false so the normal lookup will take place.
    String message;
    if (BindingSecurity::shouldAllowAccessToFrame(*state, *frame, message))
        return false;

    // https://html.spec.whatwg.org/#crossorigingetownpropertyhelper-(-o,-p-)
    if (propertyName == state->propertyNames().toStringTagSymbol || propertyName == state->propertyNames().hasInstanceSymbol || propertyName == state->propertyNames().isConcatSpreadableSymbol) {
        slot.setValue(this, ReadOnly | DontEnum, jsUndefined());
        return true;
    }

    // We only allow access to Location.replace() cross origin.
    if (propertyName == state->propertyNames().replace) {
        slot.setCustom(this, ReadOnly | DontEnum, nonCachingStaticFunctionGetter<jsLocationInstanceFunctionReplace, 1>);
        return true;
    }

    // Getting location.href cross origin needs to throw. However, getOwnPropertyDescriptor() needs to return
    // a descriptor that has a setter but no getter.
    if (slot.internalMethodType() == PropertySlot::InternalMethodType::GetOwnProperty && propertyName == state->propertyNames().href) {
        auto* entry = JSLocation::info()->staticPropHashTable->entry(propertyName);
        CustomGetterSetter* customGetterSetter = CustomGetterSetter::create(vm, nullptr, entry->propertyPutter());
        slot.setCustomGetterSetter(this, DontEnum | CustomAccessor, customGetterSetter);
        return true;
    }

    throwSecurityError(*state, scope, message);
    slot.setUndefined();
    return true;
}

bool JSLocation::putDelegate(ExecState* state, PropertyName propertyName, JSValue, PutPropertySlot&, bool& putResult)
{
    putResult = false;

    Frame* frame = wrapped().frame();
    if (!frame)
        return true;

    // Silently block access to toString and valueOf.
    if (propertyName == state->propertyNames().toString || propertyName == state->propertyNames().valueOf)
        return true;

    // Always allow assigning to the whole location.
    // However, alllowing assigning of pieces might inadvertently disclose parts of the original location.
    // So fall through to the access check for those.
    if (propertyName == state->propertyNames().href)
        return false;

    // Block access and throw if there is a security error.
    if (!BindingSecurity::shouldAllowAccessToFrame(state, frame, ThrowSecurityError))
        return true;

    return false;
}

bool JSLocation::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSLocation* thisObject = jsCast<JSLocation*>(cell);
    // Only allow deleting by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), ThrowSecurityError))
        return false;
    return Base::deleteProperty(thisObject, exec, propertyName);
}

bool JSLocation::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    JSLocation* thisObject = jsCast<JSLocation*>(cell);
    // Only allow deleting by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), ThrowSecurityError))
        return false;
    return Base::deletePropertyByIndex(thisObject, exec, propertyName);
}

// https://html.spec.whatwg.org/#crossoriginproperties-(-o-)
static void addCrossOriginPropertyNames(ExecState& state, PropertyNameArray& propertyNames)
{
    static const Identifier* const properties[] = { &state.propertyNames().href, &state.propertyNames().replace };
    for (auto* property : properties)
        propertyNames.add(*property);
}

// https://html.spec.whatwg.org/#crossoriginownpropertykeys-(-o-)
static void addCrossOriginOwnPropertyNames(ExecState& state, PropertyNameArray& propertyNames)
{
    addCrossOriginPropertyNames(state, propertyNames);

    propertyNames.add(state.propertyNames().toStringTagSymbol);
    propertyNames.add(state.propertyNames().hasInstanceSymbol);
    propertyNames.add(state.propertyNames().isConcatSpreadableSymbol);
}

void JSLocation::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), DoNotReportSecurityError)) {
        if (mode.includeDontEnumProperties())
            addCrossOriginOwnPropertyNames(*exec, propertyNames);
        return;
    }
    Base::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

bool JSLocation::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), ThrowSecurityError))
        return false;

    if (descriptor.isAccessorDescriptor() && (propertyName == exec->propertyNames().toString || propertyName == exec->propertyNames().valueOf))
        return false;
    return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
}

bool JSLocation::setPrototype(JSObject*, ExecState* exec, JSValue, bool shouldThrowIfCantSet)
{
    auto scope = DECLARE_THROW_SCOPE(exec->vm());

    if (shouldThrowIfCantSet)
        throwTypeError(exec, scope, ASCIILiteral("Cannot set prototype of this object"));

    return false;
}

JSValue JSLocation::getPrototype(JSObject* object, ExecState* exec)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), DoNotReportSecurityError))
        return jsNull();

    return Base::getPrototype(object, exec);
}

bool JSLocation::preventExtensions(JSObject*, ExecState* exec)
{
    auto scope = DECLARE_THROW_SCOPE(exec->vm());

    throwTypeError(exec, scope, ASCIILiteral("Cannot prevent extensions on this object"));
    return false;
}

String JSLocation::toStringName(const JSObject* object, ExecState* exec)
{
    auto* thisObject = jsCast<const JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), DoNotReportSecurityError))
        return ASCIILiteral("Object");
    return ASCIILiteral("Location");
}

bool JSLocationPrototype::putDelegate(ExecState* exec, PropertyName propertyName, JSValue, PutPropertySlot&, bool& putResult)
{
    putResult = false;
    return (propertyName == exec->propertyNames().toString || propertyName == exec->propertyNames().valueOf);
}

bool JSLocationPrototype::defineOwnProperty(JSObject* object, ExecState* exec, PropertyName propertyName, const PropertyDescriptor& descriptor, bool throwException)
{
    if (descriptor.isAccessorDescriptor() && (propertyName == exec->propertyNames().toString || propertyName == exec->propertyNames().valueOf))
        return false;
    return Base::defineOwnProperty(object, exec, propertyName, descriptor, throwException);
}

} // namespace WebCore
