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
#include "RuntimeApplicationChecks.h"
#include <runtime/JSFunction.h>

using namespace JSC;

namespace WebCore {

bool JSLocation::getOwnPropertySlotDelegate(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    VM& vm = exec->vm();
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
    if (shouldAllowAccessToFrame(exec, frame, message))
        return false;

    // We only allow access to Location.replace() cross origin.
    // Make it read-only / non-configurable to prevent writes via defineProperty.
    if (propertyName == exec->propertyNames().replace) {
        slot.setCustom(this, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsLocationInstanceFunctionReplace, 1>);
        return true;
    }

    throwSecurityError(*exec, scope, message);
    slot.setUndefined();
    return true;
}

bool JSLocation::putDelegate(ExecState* exec, PropertyName propertyName, JSValue, PutPropertySlot&, bool& putResult)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    putResult = false;
    Frame* frame = wrapped().frame();
    if (!frame)
        return true;

    if (propertyName == exec->propertyNames().toString || propertyName == exec->propertyNames().valueOf)
        return true;

    String errorMessage;
    if (shouldAllowAccessToFrame(exec, frame, errorMessage))
        return false;

    // Cross-domain access to the location is allowed when assigning the whole location,
    // but not when assigning the individual pieces, since that might inadvertently
    // disclose other parts of the original location.
    if (propertyName != exec->propertyNames().href) {
        throwSecurityError(*exec, scope, errorMessage);
        return true;
    }
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

static void addCrossOriginLocationPropertyNames(ExecState& state, PropertyNameArray& propertyNames)
{
    // https://html.spec.whatwg.org/#crossoriginproperties-(-o-)
    static const Identifier* const properties[] = { &state.propertyNames().href, &state.propertyNames().replace };
    for (auto* property : properties)
        propertyNames.add(*property);
}

void JSLocation::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), DoNotReportSecurityError)) {
        addCrossOriginLocationPropertyNames(*exec, propertyNames);
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

bool JSLocation::preventExtensions(JSObject* object, ExecState* exec)
{
    JSLocation* thisObject = jsCast<JSLocation*>(object);
    if (!BindingSecurity::shouldAllowAccessToFrame(exec, thisObject->wrapped().frame(), ThrowSecurityError))
        return false;
    // FIXME: The specification says to return false in the same origin case as well but other browsers have
    // not implemented this yet.
    return Base::preventExtensions(object, exec);
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
