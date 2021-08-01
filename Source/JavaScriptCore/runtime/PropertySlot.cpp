/*
 *  Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "PropertySlot.h"

#include "GetterSetter.h"
#include "JSCJSValueInlines.h"
#include "JSObject.h"

namespace JSC {

JSValue PropertySlot::functionGetter(JSGlobalObject* globalObject) const
{
    ASSERT(m_thisValue);
    return m_data.getter.getterSetter->callGetter(globalObject, m_thisValue);
}

JSValue PropertySlot::customGetter(VM& vm, PropertyName propertyName) const
{
    ASSERT(m_slotBase);
    JSGlobalObject* globalObject = m_slotBase->globalObject(vm);
    JSValue thisValue = m_attributes & PropertyAttribute::CustomAccessor ? m_thisValue : JSValue(slotBase());
    if (auto domAttribute = this->domAttribute()) {
        if (!thisValue.inherits(vm, domAttribute->classInfo)) {
            auto scope = DECLARE_THROW_SCOPE(vm);
            return throwDOMAttributeGetterTypeError(globalObject, scope, domAttribute->classInfo, propertyName);
        }
    }
    return JSValue::decode(m_data.custom.getValue(globalObject, JSValue::encode(thisValue), propertyName));
}

JSValue PropertySlot::getPureResult() const
{
    JSValue result;
    if (isTaintedByOpaqueObject())
        result = jsNull();
    else if (isCacheableValue())
        result = JSValue::decode(m_data.value);
    else if (isCacheableGetter())
        result = getterSetter();
    else if (isUnset())
        result = jsUndefined();
    else
        result = jsNull();
    
    return result;
}

} // namespace JSC
