/*
 *  Copyright (C) 2015, 2016 Canon Inc. All rights reserved.
 *  Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "JSDOMConstructorBase.h"

namespace WebCore {

class JSDOMBuiltinConstructorBase : public JSDOMConstructorBase {
public:
    using Base = JSDOMConstructorBase;

protected:
    JSDOMBuiltinConstructorBase(JSC::Structure* structure, JSDOMGlobalObject& globalObject)
        : JSDOMConstructorBase(structure, globalObject)
    {
    }

    static void visitChildren(JSC::JSCell*, JSC::SlotVisitor&);

    JSC::JSFunction* initializeFunction();
    void setInitializeFunction(JSC::VM&, JSC::JSFunction&);

    static void callFunctionWithCurrentArguments(JSC::ExecState&, JSC::JSObject& thisObject, JSC::JSFunction&);

private:
    JSC::WriteBarrier<JSC::JSFunction> m_initializeFunction;
};

inline JSC::JSFunction* JSDOMBuiltinConstructorBase::initializeFunction()
{
    return m_initializeFunction.get();
}

inline void JSDOMBuiltinConstructorBase::setInitializeFunction(JSC::VM& vm, JSC::JSFunction& function)
{
    m_initializeFunction.set(vm, this, &function);
}


} // namespace WebCore
