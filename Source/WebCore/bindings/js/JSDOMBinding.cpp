/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2011, 2013, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2013 Michael Pruett <michael@68k.org>
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
#include "JSDOMBinding.h"

#include "CommonVM.h"

using namespace JSC;

namespace WebCore {

void addImpureProperty(const AtomicString& propertyName)
{
    commonVM().addImpureProperty(propertyName);
}

JSValue jsStringOrUndefined(ExecState* exec, const String& s)
{
    if (s.isNull())
        return jsUndefined();
    return jsStringWithCache(exec, s);
}

bool hasIteratorMethod(JSC::ExecState& state, JSC::JSValue value)
{
    auto& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject())
        return false;

    JSObject* object = JSC::asObject(value);
    CallData callData;
    CallType callType;
    JSValue applyMethod = object->getMethod(&state, callData, callType, vm.propertyNames->iteratorSymbol, ASCIILiteral("Symbol.iterator property should be callable"));
    RETURN_IF_EXCEPTION(scope, false);

    return !applyMethod.isUndefined();
}

} // namespace WebCore
