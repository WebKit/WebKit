/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAssemblyObject.h"

#include "BuiltinNames.h"
#include "FunctionPrototype.h"
#include "InternalFunction.h"
#include "JSCInlines.h"
#include "JSDestructibleObject.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(WebAssemblyObject);

// Name, functionLength
#define FOR_EACH_WASM_CONSTRUCTOR_PROPERTY(DO) \
    DO(Module, 1) \
    DO(Instance, 2) \
    DO(Memory, 1) \
    DO(Table, 1) \
    DO(CompileError, 1) \
    DO(RuntimeError, 1)

// Name, functionLength
#define FOR_EACH_WASM_FUNCTION_PROPERTY(DO) \
    DO(validate, 1) \
    DO(compile, 1)

// FIXME Implement each of these in their own header file, and autogenerate *.lut.h files for *PrototypeTable. https://bugs.webkit.org/show_bug.cgi?id=161709
#define DEFINE_WASM_OBJECT_CONSTRUCTOR_PROPERTY(NAME, functionLength) \
    class JSWebAssembly ## NAME : public JSDestructibleObject { \
    public: \
        typedef JSDestructibleObject Base; \
        \
        static JSWebAssembly ## NAME* create(VM& vm, Structure* structure) \
        { \
            auto* format = new (NotNull, allocateCell<JSWebAssembly ## NAME>(vm.heap)) JSWebAssembly ## NAME(vm, structure); \
            format->finishCreation(vm); \
            return format; \
        } \
        static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype) \
        { \
            return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info()); \
        } \
        \
        DECLARE_INFO; \
        \
    protected: \
        JSWebAssembly ## NAME(VM& vm, Structure* structure) \
            : JSDestructibleObject(vm, structure) \
        { \
        } \
        void finishCreation(VM& vm) \
        { \
            Base::finishCreation(vm); \
            ASSERT(inherits(info())); \
        } \
    }; \
    const ClassInfo JSWebAssembly ## NAME::s_info = { "WebAssembly." #NAME, &Base::s_info, 0, CREATE_METHOD_TABLE(JSWebAssembly ## NAME) }; \
    \
    class WebAssembly ## NAME ## Prototype : public JSDestructibleObject { \
    public: \
        typedef JSDestructibleObject Base; \
        static const unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable; \
        \
        static WebAssembly ## NAME ## Prototype* create(VM& vm, JSGlobalObject*, Structure* structure) \
        { \
            auto* object = new (NotNull, allocateCell<WebAssembly ## NAME ## Prototype>(vm.heap)) WebAssembly ## NAME ## Prototype(vm, structure); \
            object->finishCreation(vm); \
            return object; \
        } \
        static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype) \
        { \
            return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info()); \
        } \
        \
        DECLARE_INFO; \
        \
    protected: \
        void finishCreation(VM& vm) \
        { \
            Base::finishCreation(vm); \
        } \
        \
    private: \
        WebAssembly ## NAME ## Prototype(VM& vm, Structure* structure) \
            : JSDestructibleObject(vm, structure) \
        { \
        } \
    }; \
    static const struct CompactHashIndex NAME ## PrototypeTableIndex[] = { { 0, 0 } }; \
    static const struct HashTableValue NAME ## PrototypeTableValues[] = { { nullptr, 0, NoIntrinsic, { 0 } } }; \
    static const struct HashTable NAME ## PrototypeTable = { 0, 0, true, NAME ## PrototypeTableValues, NAME ## PrototypeTableIndex }; \
    const ClassInfo WebAssembly ## NAME ## Prototype::s_info = { "WebAssembly." #NAME ".prototype", &Base::s_info, &NAME ## PrototypeTable, CREATE_METHOD_TABLE(WebAssembly ## NAME ## Prototype) }; \
    \
    static EncodedJSValue JSC_HOST_CALL constructJSWebAssembly ## NAME(ExecState* state) \
    { \
        VM& vm = state->vm(); \
        auto scope = DECLARE_THROW_SCOPE(vm); \
        return JSValue::encode(throwException(state, scope, createError(state, ASCIILiteral("WebAssembly doesn't yet implement the " #NAME " constructor property")))); \
    } \
    \
    static EncodedJSValue JSC_HOST_CALL callJSWebAssembly ## NAME(ExecState* state) \
    { \
    VM& vm = state->vm(); \
    auto scope = DECLARE_THROW_SCOPE(vm); \
    return JSValue::encode(throwException(state, scope, createError(state, ASCIILiteral("WebAssembly doesn't yet implement the " #NAME " constructor property")))); \
    } \
    \
    class WebAssembly ## NAME ## Constructor : public InternalFunction { \
    public: \
        typedef InternalFunction Base; \
        static const unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable; \
        \
        static WebAssembly ## NAME ## Constructor* create(VM& vm, Structure* structure, WebAssembly ## NAME ## Prototype* NAME ## Prototype, Structure* NAME ## Structure) \
        { \
            WebAssembly ## NAME ## Constructor* constructor = new (NotNull, allocateCell<WebAssembly ## NAME ## Constructor>(vm.heap)) WebAssembly ## NAME ## Constructor(vm, structure); \
            constructor->finishCreation(vm, NAME ## Prototype, NAME ## Structure); \
            return constructor; \
        } \
        static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype) \
        { \
            return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info()); \
        } \
        \
        DECLARE_INFO; \
        \
        Structure* NAME ## Structure() const { return m_ ## NAME ## Structure.get(); } \
        \
    protected: \
        void finishCreation(VM& vm, WebAssembly ## NAME ## Prototype* NAME ## Prototype, Structure* NAME ## Structure) \
        { \
            Base::finishCreation(vm, ASCIILiteral(#NAME)); \
            putDirectWithoutTransition(vm, vm.propertyNames->prototype, NAME ## Prototype, DontEnum | DontDelete | ReadOnly); \
            putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(functionLength), ReadOnly | DontEnum | DontDelete); \
            m_ ## NAME ## Structure.set(vm, this, NAME ## Structure); \
        } \
        \
    private: \
        WebAssembly ## NAME ## Constructor(VM& vm, Structure* structure) \
            : InternalFunction(vm, structure) \
        { \
        } \
        static ConstructType getConstructData(JSCell*, ConstructData& constructData) \
        { \
            constructData.native.function = constructJSWebAssembly ## NAME; \
            return ConstructType::Host; \
        } \
        static CallType getCallData(JSCell*, CallData& callData) \
        { \
            callData.native.function = callJSWebAssembly ## NAME; \
            return CallType::Host; \
        } \
        static void visitChildren(JSCell* cell, SlotVisitor& visitor) \
        { \
            auto* thisObject = jsCast<WebAssembly ## NAME ## Constructor*>(cell); \
            ASSERT_GC_OBJECT_INHERITS(thisObject, info()); \
            Base::visitChildren(thisObject, visitor); \
            visitor.append(&thisObject->m_ ## NAME ## Structure); \
        } \
        \
        WriteBarrier<Structure> m_ ## NAME ## Structure; \
    }; \
    static const struct CompactHashIndex NAME ## ConstructorTableIndex[] = { { 0, 0 } }; \
    static const struct HashTableValue NAME ## ConstructorTableValues[] = { { nullptr, 0, NoIntrinsic, { 0 } } }; \
    static const struct HashTable NAME ## ConstructorTable = { 0, 0, true, NAME ## ConstructorTableValues, NAME ## ConstructorTableIndex }; \
    const ClassInfo WebAssembly ## NAME ## Constructor::s_info = { "Function", &Base::s_info, &NAME ## ConstructorTable, CREATE_METHOD_TABLE(WebAssembly ## NAME ## Constructor) };

FOR_EACH_WASM_CONSTRUCTOR_PROPERTY(DEFINE_WASM_OBJECT_CONSTRUCTOR_PROPERTY)


#define DECLARE_WASM_OBJECT_FUNCTION(NAME, ...) EncodedJSValue JSC_HOST_CALL wasmObjectFunc ## NAME(ExecState*);
FOR_EACH_WASM_FUNCTION_PROPERTY(DECLARE_WASM_OBJECT_FUNCTION)


const ClassInfo WebAssemblyObject::s_info = { "WebAssembly", &Base::s_info, 0, CREATE_METHOD_TABLE(WebAssemblyObject) };

WebAssemblyObject* WebAssemblyObject::create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<WebAssemblyObject>(vm.heap)) WebAssemblyObject(vm, structure);
    object->finishCreation(vm, globalObject);
    return object;
}

Structure* WebAssemblyObject::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

void WebAssemblyObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

#define SET_UP_WASM_CONSTRUCTOR_PROPERTY(NAME, ...) \
    auto* prototype ## NAME = WebAssembly ## NAME ## Prototype::create(vm, globalObject, WebAssembly ## NAME ## Prototype::createStructure(vm, globalObject, globalObject->objectPrototype())); \
    auto* structure ## NAME = JSWebAssembly ## NAME::createStructure(vm, globalObject, prototype ## NAME); \
    auto* constructor ## NAME = WebAssembly ## NAME ## Constructor::create(vm, WebAssembly ## NAME ## Constructor::createStructure(vm, globalObject, globalObject->functionPrototype()), prototype ## NAME, structure ## NAME); \
    prototype ## NAME->putDirectWithoutTransition(vm, vm.propertyNames->constructor, constructor ## NAME, DontEnum);
    FOR_EACH_WASM_CONSTRUCTOR_PROPERTY(SET_UP_WASM_CONSTRUCTOR_PROPERTY)

#define REGISTER_WASM_CONSTRUCTOR_PROPERTY(NAME, ...) \
    putDirectWithoutTransition(vm, Identifier::fromString(globalObject->globalExec(), #NAME), constructor ## NAME, DontEnum);
    FOR_EACH_WASM_CONSTRUCTOR_PROPERTY(REGISTER_WASM_CONSTRUCTOR_PROPERTY)

#define REGISTER_WASM_FUNCTION_PROPERTY(NAME, LENGTH) \
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, #NAME), LENGTH, wasmObjectFunc ## NAME, NoIntrinsic, DontEnum);
    FOR_EACH_WASM_FUNCTION_PROPERTY(REGISTER_WASM_FUNCTION_PROPERTY)
}

WebAssemblyObject::WebAssemblyObject(VM& vm, Structure* structure)
    : JSNonFinalObject(vm, structure)
{
}

// ------------------------------ Constructors -----------------------------

    
// ------------------------------ Functions --------------------------------

EncodedJSValue JSC_HOST_CALL wasmObjectFuncvalidate(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwException(state, scope, createError(state, ASCIILiteral("WebAssembly doesn't yet implement the validate function property"))));
}

EncodedJSValue JSC_HOST_CALL wasmObjectFunccompile(ExecState* state)
{
    VM& vm = state->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return JSValue::encode(throwException(state, scope, createError(state, ASCIILiteral("WebAssembly doesn't yet implement the compile function property"))));
}

} // namespace JSC
