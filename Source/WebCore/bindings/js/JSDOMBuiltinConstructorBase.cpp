/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2017 Apple Inc. All rights reserved.
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
#include "JSDOMBuiltinConstructorBase.h"

#include "WebCoreJSClientData.h"
#include <JavaScriptCore/JSCInlines.h>

namespace WebCore {
using namespace JSC;
    
void JSDOMBuiltinConstructorBase::callFunctionWithCurrentArguments(JSC::JSGlobalObject& lexicalGlobalObject, JSC::CallFrame& callFrame, JSC::JSObject& thisObject, JSC::JSFunction& function)
{
    JSC::VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSC::CallData callData;
    JSC::CallType callType = JSC::getCallData(vm, &function, callData);
    ASSERT(callType != CallType::None);

    JSC::MarkedArgumentBuffer arguments;
    for (unsigned i = 0; i < callFrame.argumentCount(); ++i)
        arguments.append(callFrame.uncheckedArgument(i));
    if (UNLIKELY(arguments.hasOverflowed())) {
        throwOutOfMemoryError(&lexicalGlobalObject, scope);
        return;
    }
    JSC::call(&lexicalGlobalObject, &function, callType, callData, &thisObject, arguments);
}

void JSDOMBuiltinConstructorBase::visitChildren(JSC::JSCell* cell, JSC::SlotVisitor& visitor)
{
    auto* thisObject = jsCast<JSDOMBuiltinConstructorBase*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_initializeFunction);
}

JSC::IsoSubspace* JSDOMBuiltinConstructorBase::subspaceForImpl(JSC::VM& vm)
{
    return &static_cast<JSVMClientData*>(vm.clientData)->domBuiltinConstructorSpace();
}

} // namespace WebCore
