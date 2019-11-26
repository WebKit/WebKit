/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2019 Apple Inc. All rights reserved.
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

#include "CodeSpecializationKind.h"
#include "JSDestructibleObject.h"

namespace JSC {

class FunctionPrototype;

class InternalFunction : public JSNonFinalObject {
    friend class JIT;
    friend class LLIntOffsetsExtractor;
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | ImplementsHasInstance | ImplementsDefaultHasInstance | OverridesGetCallData;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        static_assert(sizeof(CellType) == sizeof(InternalFunction), "InternalFunction subclasses that add fields need to override subspaceFor<>()");
        return &vm.internalFunctionSpace;
    }

    DECLARE_EXPORT_INFO;

    JS_EXPORT_PRIVATE static void visitChildren(JSCell*, SlotVisitor&);

    JS_EXPORT_PRIVATE const String& name();
    const String displayName(VM&);
    const String calculatedDisplayName(VM&);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    { 
        return Structure::create(vm, globalObject, proto, TypeInfo(InternalFunctionType, StructureFlags), info()); 
    }

    static Structure* createSubclassStructure(JSGlobalObject*, JSObject* baseCallee, JSValue newTarget, Structure*);

    TaggedNativeFunction nativeFunctionFor(CodeSpecializationKind kind)
    {
        if (kind == CodeForCall)
            return m_functionForCall;
        ASSERT(kind == CodeForConstruct);
        return m_functionForConstruct;
    }

    static ptrdiff_t offsetOfNativeFunctionFor(CodeSpecializationKind kind)
    {
        if (kind == CodeForCall)
            return OBJECT_OFFSETOF(InternalFunction, m_functionForCall);
        ASSERT(kind == CodeForConstruct);
        return OBJECT_OFFSETOF(InternalFunction, m_functionForConstruct);
    }

    static ptrdiff_t offsetOfGlobalObject()
    {
        return OBJECT_OFFSETOF(InternalFunction, m_globalObject);
    }

    JSGlobalObject* globalObject() const { return m_globalObject.get(); }

protected:
    JS_EXPORT_PRIVATE InternalFunction(VM&, Structure*, NativeFunction functionForCall, NativeFunction functionForConstruct);

    enum class NameAdditionMode { WithStructureTransition, WithoutStructureTransition };
    JS_EXPORT_PRIVATE void finishCreation(VM&, const String& name, NameAdditionMode = NameAdditionMode::WithStructureTransition);

    JS_EXPORT_PRIVATE static Structure* createSubclassStructureSlow(JSGlobalObject*, JSValue newTarget, Structure*);

    JS_EXPORT_PRIVATE static ConstructType getConstructData(JSCell*, ConstructData&);
    JS_EXPORT_PRIVATE static CallType getCallData(JSCell*, CallData&);

    TaggedNativeFunction m_functionForCall;
    TaggedNativeFunction m_functionForConstruct;
    WriteBarrier<JSString> m_originalName;
    WriteBarrier<JSGlobalObject> m_globalObject;
};

ALWAYS_INLINE Structure* InternalFunction::createSubclassStructure(JSGlobalObject* globalObject, JSObject* baseCallee, JSValue newTarget, Structure* baseClass)
{
    // We allow newTarget == JSValue() because the API needs to be able to create classes without having a real JS frame.
    // Since we don't allow subclassing in the API we just treat newTarget == JSValue() as newTarget == callFrame->jsCallee()
    if (newTarget && newTarget != baseCallee)
        return createSubclassStructureSlow(globalObject, newTarget, baseClass);
    return baseClass;
}

} // namespace JSC
