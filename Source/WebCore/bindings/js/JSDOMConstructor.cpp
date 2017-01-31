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
#include "JSDOMConstructor.h"

#include <runtime/JSCInlines.h>

using namespace JSC;

namespace WebCore {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DOMConstructorObject);
STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(DOMConstructorWithDocument);
    
void callFunctionWithCurrentArguments(JSC::ExecState& state, JSC::JSObject& thisObject, JSC::JSFunction& function)
{
    JSC::CallData callData;
    JSC::CallType callType = JSC::getCallData(&function, callData);
    ASSERT(callType != CallType::None);

    JSC::MarkedArgumentBuffer arguments;
    for (unsigned i = 0; i < state.argumentCount(); ++i)
        arguments.append(state.uncheckedArgument(i));
    JSC::call(&state, &function, callType, callData, &thisObject, arguments);
}

void DOMConstructorJSBuiltinObject::visitChildren(JSC::JSCell* cell, JSC::SlotVisitor& visitor)
{
    auto* thisObject = jsCast<DOMConstructorJSBuiltinObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_initializeFunction);
}

static EncodedJSValue JSC_HOST_CALL callThrowTypeError(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    throwTypeError(exec, scope, ASCIILiteral("Constructor requires 'new' operator"));
    return JSValue::encode(jsNull());
}

CallType DOMConstructorObject::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callThrowTypeError;
    return CallType::Host;
}

} // namespace WebCore
