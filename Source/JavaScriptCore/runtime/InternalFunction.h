/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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

#pragma once

#include "JSDestructibleObject.h"

namespace JSC {

class FunctionPrototype;

class InternalFunction : public JSDestructibleObject {
public:
    typedef JSDestructibleObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | ImplementsHasInstance | ImplementsDefaultHasInstance | TypeOfShouldCallGetCallData;

    DECLARE_EXPORT_INFO;

    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

    JS_EXPORT_PRIVATE const String& name();
    const String displayName(VM&);
    const String calculatedDisplayName(VM&);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    { 
        return Structure::create(vm, globalObject, proto, TypeInfo(ObjectType, StructureFlags), info()); 
    }

    static Structure* createSubclassStructure(ExecState*, JSValue newTarget, Structure*);

protected:
    JS_EXPORT_PRIVATE InternalFunction(VM&, Structure*);

    JS_EXPORT_PRIVATE void finishCreation(VM&, const String& name);

    JS_EXPORT_PRIVATE static Structure* createSubclassStructureSlow(ExecState*, JSValue newTarget, Structure*);

    static CallType getCallData(JSCell*, CallData&);
    WriteBarrier<JSString> m_originalName;
};

InternalFunction* asInternalFunction(JSValue);

inline InternalFunction* asInternalFunction(JSValue value)
{
    ASSERT(asObject(value)->inherits(*value.getObject()->vm(), InternalFunction::info()));
    return static_cast<InternalFunction*>(asObject(value));
}

ALWAYS_INLINE Structure* InternalFunction::createSubclassStructure(ExecState* exec, JSValue newTarget, Structure* baseClass)
{
    // We allow newTarget == JSValue() because the API needs to be able to create classes without having a real JS frame.
    // Since we don't allow subclassing in the API we just treat newTarget == JSValue() as newTarget == exec->jsCallee()
    ASSERT(!newTarget || newTarget.isConstructor());

    if (newTarget && newTarget != exec->jsCallee())
        return createSubclassStructureSlow(exec, newTarget, baseClass);
    return baseClass;
}

} // namespace JSC
