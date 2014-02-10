/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSFunction_h
#define JSFunction_h

#include "InternalFunction.h"
#include "JSDestructibleObject.h"
#include "JSScope.h"
#include "ObjectAllocationProfile.h"
#include "Watchpoint.h"

namespace JSC {

    class ExecutableBase;
    class FunctionExecutable;
    class FunctionPrototype;
    class JSActivation;
    class JSGlobalObject;
    class LLIntOffsetsExtractor;
    class NativeExecutable;
    class SourceCode;
    namespace DFG {
    class SpeculativeJIT;
    class JITCompiler;
    }

    JS_EXPORT_PRIVATE EncodedJSValue JSC_HOST_CALL callHostFunctionAsConstructor(ExecState*);

    JS_EXPORT_PRIVATE String getCalculatedDisplayName(CallFrame*, JSObject*);
    
    class JSFunction : public JSDestructibleObject {
        friend class JIT;
        friend class DFG::SpeculativeJIT;
        friend class DFG::JITCompiler;
        friend class VM;

    public:
        typedef JSDestructibleObject Base;

        JS_EXPORT_PRIVATE static JSFunction* create(VM&, JSGlobalObject*, int length, const String& name, NativeFunction, Intrinsic = NoIntrinsic, NativeFunction nativeConstructor = callHostFunctionAsConstructor);

        static JSFunction* create(VM& vm, FunctionExecutable* executable, JSScope* scope)
        {
            JSFunction* function = new (NotNull, allocateCell<JSFunction>(vm.heap)) JSFunction(vm, executable, scope);
            ASSERT(function->structure()->globalObject());
            function->finishCreation(vm);
            return function;
        }
        
        static void destroy(JSCell*);
        
        JS_EXPORT_PRIVATE String name(ExecState*);
        JS_EXPORT_PRIVATE String displayName(ExecState*);
        const String calculatedDisplayName(ExecState*);

        JSScope* scope()
        {
            ASSERT(!isHostFunctionNonInline());
            return m_scope.get();
        }
        // This method may be called for host functins, in which case it
        // will return an arbitrary value. This should only be used for
        // optimized paths in which the return value does not matter for
        // host functions, and checking whether the function is a host
        // function is deemed too expensive.
        JSScope* scopeUnchecked()
        {
            return m_scope.get();
        }
        void setScope(VM& vm, JSScope* scope)
        {
            ASSERT(!isHostFunctionNonInline());
            m_scope.set(vm, this, scope);
        }
        void addNameScopeIfNeeded(VM&);

        ExecutableBase* executable() const { return m_executable.get(); }

        // To call either of these methods include Executable.h
        bool isHostFunction() const;
        FunctionExecutable* jsExecutable() const;

        JS_EXPORT_PRIVATE const SourceCode* sourceCode() const;

        DECLARE_EXPORT_INFO;

        static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype) 
        {
            ASSERT(globalObject);
            return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info()); 
        }

        NativeFunction nativeFunction();
        NativeFunction nativeConstructor();

        static ConstructType getConstructData(JSCell*, ConstructData&);
        static CallType getCallData(JSCell*, CallData&);

        static inline ptrdiff_t offsetOfScopeChain()
        {
            return OBJECT_OFFSETOF(JSFunction, m_scope);
        }

        static inline ptrdiff_t offsetOfExecutable()
        {
            return OBJECT_OFFSETOF(JSFunction, m_executable);
        }

        static inline ptrdiff_t offsetOfAllocationProfile()
        {
            return OBJECT_OFFSETOF(JSFunction, m_allocationProfile);
        }

        ObjectAllocationProfile* allocationProfile(ExecState* exec, unsigned inlineCapacity)
        {
            if (UNLIKELY(m_allocationProfile.isNull()))
                return createAllocationProfile(exec, inlineCapacity);
            return &m_allocationProfile;
        }
        
        Structure* allocationStructure() { return m_allocationProfile.structure(); }

        InlineWatchpointSet& allocationProfileWatchpointSet()
        {
            return m_allocationProfileWatchpoint;
        }

    protected:
        const static unsigned StructureFlags = OverridesGetOwnPropertySlot | ImplementsHasInstance | OverridesVisitChildren | OverridesGetPropertyNames | JSObject::StructureFlags;

        JS_EXPORT_PRIVATE JSFunction(VM&, JSGlobalObject*, Structure*);
        JSFunction(VM&, FunctionExecutable*, JSScope*);
        
        void finishCreation(VM&, NativeExecutable*, int length, const String& name);
        using Base::finishCreation;

        ObjectAllocationProfile* createAllocationProfile(ExecState*, size_t inlineCapacity);

        static bool getOwnPropertySlot(JSObject*, ExecState*, PropertyName, PropertySlot&);
        static void getOwnNonIndexPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode = ExcludeDontEnumProperties);
        static bool defineOwnProperty(JSObject*, ExecState*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

        static void put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);

        static bool deleteProperty(JSCell*, ExecState*, PropertyName);

        static void visitChildren(JSCell*, SlotVisitor&);

    private:
        friend class LLIntOffsetsExtractor;
        
        JS_EXPORT_PRIVATE bool isHostFunctionNonInline() const;

        static EncodedJSValue argumentsGetter(ExecState*, JSObject*, EncodedJSValue, PropertyName);
        static EncodedJSValue callerGetter(ExecState*, JSObject*, EncodedJSValue, PropertyName);
        static EncodedJSValue lengthGetter(ExecState*, JSObject*, EncodedJSValue, PropertyName);
        static EncodedJSValue nameGetter(ExecState*, JSObject*, EncodedJSValue, PropertyName);

        WriteBarrier<ExecutableBase> m_executable;
        WriteBarrier<JSScope> m_scope;
        ObjectAllocationProfile m_allocationProfile;
        InlineWatchpointSet m_allocationProfileWatchpoint;
    };

} // namespace JSC

#endif // JSFunction_h
