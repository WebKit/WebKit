/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003-2020 Apple Inc. All rights reserved.
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

#include "FunctionRareData.h"
#include "InternalFunction.h"
#include "JSCallee.h"
#include "JSScope.h"

namespace JSC {

class ExecutableBase;
class FunctionExecutable;
class FunctionPrototype;
class JSLexicalEnvironment;
class JSGlobalObject;
class LLIntOffsetsExtractor;
class NativeExecutable;
class SourceCode;
class InternalFunction;
namespace DFG {
class SpeculativeJIT;
class JITCompiler;
}

namespace DOMJIT {
class Signature;
}


JS_EXPORT_PRIVATE JSC_DECLARE_HOST_FUNCTION(callHostFunctionAsConstructor);

JS_EXPORT_PRIVATE String getCalculatedDisplayName(VM&, JSObject*);

class JSFunction : public JSCallee {
    friend class JIT;
    friend class DFG::SpeculativeJIT;
    friend class DFG::JITCompiler;
    friend class VM;
    friend class InternalFunction;

public:
    static constexpr uintptr_t rareDataTag = 0x1;
    
    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.functionSpace;
    }
    
    typedef JSCallee Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | OverridesAnyFormOfGetPropertyNames | OverridesGetCallData;

    static size_t allocationSize(Checked<size_t> inlineCapacity)
    {
        ASSERT_UNUSED(inlineCapacity, !inlineCapacity);
        return sizeof(JSFunction);
    }

    static Structure* selectStructureForNewFuncExp(JSGlobalObject*, FunctionExecutable*);

    JS_EXPORT_PRIVATE static JSFunction* create(VM&, JSGlobalObject*, unsigned length, const String& name, NativeFunction, Intrinsic = NoIntrinsic, NativeFunction nativeConstructor = callHostFunctionAsConstructor, const DOMJIT::Signature* = nullptr);
    
    static JSFunction* createWithInvalidatedReallocationWatchpoint(VM&, FunctionExecutable*, JSScope*);

    JS_EXPORT_PRIVATE static JSFunction* create(VM&, FunctionExecutable*, JSScope*);
    static JSFunction* create(VM&, FunctionExecutable*, JSScope*, Structure*);

    JS_EXPORT_PRIVATE String name(VM&);
    JS_EXPORT_PRIVATE String displayName(VM&);
    JS_EXPORT_PRIVATE const String calculatedDisplayName(VM&);

    ExecutableBase* executable() const
    {
        uintptr_t executableOrRareData = m_executableOrRareData;
        if (executableOrRareData & rareDataTag)
            return bitwise_cast<FunctionRareData*>(executableOrRareData & ~rareDataTag)->executable();
        return bitwise_cast<ExecutableBase*>(executableOrRareData);
    }

    // To call any of these methods include JSFunctionInlines.h
    bool isHostFunction() const;
    FunctionExecutable* jsExecutable() const;
    Intrinsic intrinsic() const;

    JS_EXPORT_PRIVATE const SourceCode* sourceCode() const;

    DECLARE_EXPORT_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype) 
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info()); 
    }

    TaggedNativeFunction nativeFunction();
    TaggedNativeFunction nativeConstructor();

    JS_EXPORT_PRIVATE static CallData getConstructData(JSCell*);
    JS_EXPORT_PRIVATE static CallData getCallData(JSCell*);

    static inline ptrdiff_t offsetOfExecutableOrRareData()
    {
        return OBJECT_OFFSETOF(JSFunction, m_executableOrRareData);
    }

    FunctionRareData* ensureRareData(VM& vm)
    {
        uintptr_t executableOrRareData = m_executableOrRareData;
        if (UNLIKELY(!(executableOrRareData & rareDataTag)))
            return allocateRareData(vm);
        return bitwise_cast<FunctionRareData*>(executableOrRareData & ~rareDataTag);
    }

    FunctionRareData* ensureRareDataAndAllocationProfile(JSGlobalObject*, unsigned inlineCapacity);

    FunctionRareData* rareData() const
    {
        uintptr_t executableOrRareData = m_executableOrRareData;
        if (executableOrRareData & rareDataTag)
            return bitwise_cast<FunctionRareData*>(executableOrRareData & ~rareDataTag);
        return nullptr;
    }

    bool isHostOrBuiltinFunction() const;
    bool isBuiltinFunction() const;
    JS_EXPORT_PRIVATE bool isHostFunctionNonInline() const;
    bool isClassConstructorFunction() const;

    void setFunctionName(JSGlobalObject*, JSValue name);

    // Returns the __proto__ for the |this| value if this JSFunction were to be constructed.
    JSObject* prototypeForConstruction(VM&, JSGlobalObject*);

    bool canUseAllocationProfile();
    bool canUseAllocationProfileNonInline();

    enum class PropertyStatus {
        Eager,
        Lazy,
        Reified,
    };
    PropertyStatus reifyLazyPropertyIfNeeded(VM&, JSGlobalObject*, PropertyName);

    bool canAssumeNameAndLengthAreOriginal(VM&);

protected:
    JS_EXPORT_PRIVATE JSFunction(VM&, NativeExecutable*, JSGlobalObject*, Structure*);
    JSFunction(VM&, FunctionExecutable*, JSScope*, Structure*);

    void finishCreation(VM&, NativeExecutable*, unsigned length, const String& name);
    void finishCreation(VM&);

    static bool getOwnPropertySlot(JSObject*, JSGlobalObject*, PropertyName, PropertySlot&);
    static void getOwnNonIndexPropertyNames(JSObject*, JSGlobalObject*, PropertyNameArray&, EnumerationMode = EnumerationMode());
    static bool defineOwnProperty(JSObject*, JSGlobalObject*, PropertyName, const PropertyDescriptor&, bool shouldThrow);

    static bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&);

    static bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&);

    static void visitChildren(JSCell*, SlotVisitor&);

private:
    static JSFunction* createImpl(VM& vm, FunctionExecutable* executable, JSScope* scope, Structure* structure)
    {
        JSFunction* function = new (NotNull, allocateCell<JSFunction>(vm.heap)) JSFunction(vm, executable, scope, structure);
        ASSERT(function->structure(vm)->globalObject());
        function->finishCreation(vm);
        return function;
    }

    FunctionRareData* allocateRareData(VM&);
    FunctionRareData* allocateAndInitializeRareData(JSGlobalObject*, size_t inlineCapacity);
    FunctionRareData* initializeRareData(JSGlobalObject*, size_t inlineCapacity);

    bool hasReifiedLength() const;
    bool hasReifiedName() const;
    void reifyLength(VM&);
    void reifyName(VM&, JSGlobalObject*);
    void reifyName(VM&, JSGlobalObject*, String name);

    static bool isLazy(PropertyStatus property) { return property == PropertyStatus::Lazy || property == PropertyStatus::Reified; }
    static bool isReified(PropertyStatus property) { return property == PropertyStatus::Reified; }

    PropertyStatus reifyLazyPropertyForHostOrBuiltinIfNeeded(VM&, JSGlobalObject*, PropertyName);
    PropertyStatus reifyLazyLengthIfNeeded(VM&, JSGlobalObject*, PropertyName);
    PropertyStatus reifyLazyNameIfNeeded(VM&, JSGlobalObject*, PropertyName);
    PropertyStatus reifyLazyBoundNameIfNeeded(VM&, JSGlobalObject*, PropertyName);

#if ASSERT_ENABLED
    void assertTypeInfoFlagInvariants();
#else
    void assertTypeInfoFlagInvariants() { }
#endif

    friend class LLIntOffsetsExtractor;

    uintptr_t m_executableOrRareData;
};

class JSStrictFunction final : public JSFunction {
public:
    using Base = JSFunction;

    DECLARE_EXPORT_INFO;

    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
    }
};
static_assert(sizeof(JSStrictFunction) == sizeof(JSFunction), "Allocated in JSFunction IsoSubspace");

class JSSloppyFunction final : public JSFunction {
public:
    using Base = JSFunction;

    DECLARE_EXPORT_INFO;

    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
    }
};
static_assert(sizeof(JSSloppyFunction) == sizeof(JSFunction), "Allocated in JSFunction IsoSubspace");

class JSArrowFunction final : public JSFunction {
public:
    using Base = JSFunction;

    DECLARE_EXPORT_INFO;

    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        ASSERT(globalObject);
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
    }
};
static_assert(sizeof(JSArrowFunction) == sizeof(JSFunction), "Allocated in JSFunction IsoSubspace");

} // namespace JSC
