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

#include "JSObjectWithGlobalObject.h"

namespace JSC {

    class ExecutableBase;
    class FunctionExecutable;
    class FunctionPrototype;
    class JSActivation;
    class JSGlobalObject;
    class NativeExecutable;
    class VPtrHackExecutable;
    namespace DFG {
    class JITCodeGenerator;
    }

    EncodedJSValue JSC_HOST_CALL callHostFunctionAsConstructor(ExecState*);

    class JSFunction : public JSObjectWithGlobalObject {
        friend class JIT;
        friend class DFG::JITCodeGenerator;
        friend class JSGlobalData;

        JSFunction(ExecState*, JSGlobalObject*, Structure*, int length, const Identifier&, NativeFunction);
        JSFunction(ExecState*, JSGlobalObject*, Structure*, int length, const Identifier&, NativeExecutable*);
        JSFunction(ExecState*, FunctionExecutable*, ScopeChainNode*);
        
    public:
        typedef JSObjectWithGlobalObject Base;

        static JSFunction* create(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, int length, const Identifier& ident, NativeFunction nativeFunc)
        {
            return new (allocateCell<JSFunction>(*exec->heap())) JSFunction(exec, globalObject, structure, length, ident, nativeFunc);
        }
        static JSFunction* create(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, int length, const Identifier& ident, NativeExecutable* nativeExec)
        {
            return new (allocateCell<JSFunction>(*exec->heap())) JSFunction(exec, globalObject, structure, length, ident, nativeExec);
        }
        static JSFunction* create(ExecState* exec, FunctionExecutable* funcExec, ScopeChainNode* scopeChain)
        {
            return new (allocateCell<JSFunction>(*exec->heap())) JSFunction(exec, funcExec, scopeChain);
        }
        
        virtual ~JSFunction();

        const UString& name(ExecState*);
        const UString displayName(ExecState*);
        const UString calculatedDisplayName(ExecState*);

        ScopeChainNode* scope()
        {
            ASSERT(!isHostFunctionNonInline());
            return m_scopeChain.get();
        }
        // This method may be called for host functins, in which case it
        // will return an arbitrary value. This should only be used for
        // optimized paths in which the return value does not matter for
        // host functions, and checking whether the function is a host
        // function is deemed too expensive.
        ScopeChainNode* scopeUnchecked()
        {
            return m_scopeChain.get();
        }
        void setScope(JSGlobalData& globalData, ScopeChainNode* scopeChain)
        {
            ASSERT(!isHostFunctionNonInline());
            m_scopeChain.set(globalData, this, scopeChain);
        }

        ExecutableBase* executable() const { return m_executable.get(); }

        // To call either of these methods include Executable.h
        inline bool isHostFunction() const;
        FunctionExecutable* jsExecutable() const;

        static JS_EXPORTDATA const ClassInfo s_info;

        static Structure* createStructure(JSGlobalData& globalData, JSValue prototype) 
        { 
            return Structure::create(globalData, prototype, TypeInfo(ObjectType, StructureFlags), AnonymousSlotCount, &s_info); 
        }

        NativeFunction nativeFunction();

        virtual ConstructType getConstructData(ConstructData&);
        virtual CallType getCallData(CallData&);

        static inline size_t offsetOfScopeChain()
        {
            return OBJECT_OFFSETOF(JSFunction, m_scopeChain);
        }

        static inline size_t offsetOfExecutable()
        {
            return OBJECT_OFFSETOF(JSFunction, m_executable);
        }

    protected:
        const static unsigned StructureFlags = OverridesGetOwnPropertySlot | ImplementsHasInstance | OverridesVisitChildren | OverridesGetPropertyNames | JSObject::StructureFlags;

    private:
        explicit JSFunction(VPtrStealingHackType);

        bool isHostFunctionNonInline() const;

        virtual void preventExtensions(JSGlobalData&);
        virtual bool getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&);
        virtual bool getOwnPropertyDescriptor(ExecState*, const Identifier&, PropertyDescriptor&);
        virtual void getOwnPropertyNames(ExecState*, PropertyNameArray&, EnumerationMode mode = ExcludeDontEnumProperties);
        virtual void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        virtual bool deleteProperty(ExecState*, const Identifier& propertyName);

        virtual void visitChildren(SlotVisitor&);

        static JSValue argumentsGetter(ExecState*, JSValue, const Identifier&);
        static JSValue callerGetter(ExecState*, JSValue, const Identifier&);
        static JSValue lengthGetter(ExecState*, JSValue, const Identifier&);

        WriteBarrier<ExecutableBase> m_executable;
        WriteBarrier<ScopeChainNode> m_scopeChain;
    };

    JSFunction* asFunction(JSValue);

    inline JSFunction* asFunction(JSValue value)
    {
        ASSERT(asObject(value)->inherits(&JSFunction::s_info));
        return static_cast<JSFunction*>(asObject(value));
    }

} // namespace JSC

#endif // JSFunction_h
