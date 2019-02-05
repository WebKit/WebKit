/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#include "JSDollarVM.h"

#include "BuiltinExecutableCreator.h"
#include "CodeBlock.h"
#include "DOMAttributeGetterSetter.h"
#include "DOMJITGetterSetter.h"
#include "FrameTracers.h"
#include "FunctionCodeBlock.h"
#include "GetterSetter.h"
#include "JSArray.h"
#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSONObject.h"
#include "JSProxy.h"
#include "JSString.h"
#include "ShadowChicken.h"
#include "Snippet.h"
#include "SnippetParams.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "VMInspector.h"
#include <wtf/Atomics.h>
#include <wtf/DataLog.h>
#include <wtf/ProcessID.h>
#include <wtf/StringPrintStream.h>

#if ENABLE(WEBASSEMBLY)
#include "JSWebAssemblyHelpers.h"
#include "WasmStreamingParser.h"
#endif

using namespace JSC;

namespace {

class JSDollarVMCallFrame : public JSDestructibleObject {
    using Base = JSDestructibleObject;
public:
    JSDollarVMCallFrame(VM& vm, Structure* structure)
        : Base(vm, structure)
    { }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSDollarVMCallFrame* create(ExecState* exec, unsigned requestedFrameIndex)
    {
        VM& vm = exec->vm();
        JSGlobalObject* globalObject = exec->lexicalGlobalObject();
        Structure* structure = createStructure(vm, globalObject, jsNull());
        JSDollarVMCallFrame* frame = new (NotNull, allocateCell<JSDollarVMCallFrame>(vm.heap, sizeof(JSDollarVMCallFrame))) JSDollarVMCallFrame(vm, structure);
        frame->finishCreation(vm, exec, requestedFrameIndex);
        return frame;
    }

    void finishCreation(VM& vm, CallFrame* frame, unsigned requestedFrameIndex)
    {
        Base::finishCreation(vm);

        auto addProperty = [&] (VM& vm, const char* name, JSValue value) {
            JSDollarVMCallFrame::addProperty(vm, name, value);
        };

        unsigned frameIndex = 0;
        bool isValid = false;
        frame->iterate([&] (StackVisitor& visitor) {

            if (frameIndex++ != requestedFrameIndex)
                return StackVisitor::Continue;

            addProperty(vm, "name", jsString(&vm, visitor->functionName()));

            if (visitor->callee().isCell())
                addProperty(vm, "callee", visitor->callee().asCell());

            CodeBlock* codeBlock = visitor->codeBlock();
            if (codeBlock) {
                addProperty(vm, "codeBlock", codeBlock);
                addProperty(vm, "unlinkedCodeBlock", codeBlock->unlinkedCodeBlock());
                addProperty(vm, "executable", codeBlock->ownerExecutable());
            }
            isValid = true;

            return StackVisitor::Done;
        });

        addProperty(vm, "valid", jsBoolean(isValid));
    }

    DECLARE_INFO;

private:
    void addProperty(VM& vm, const char* name, JSValue value)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        putDirect(vm, identifier, value);
    }
};

const ClassInfo JSDollarVMCallFrame::s_info = { "CallFrame", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDollarVMCallFrame) };

class ElementHandleOwner;
class Root;

class Element : public JSNonFinalObject {
public:
    Element(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    typedef JSNonFinalObject Base;

    Root* root() const { return m_root.get(); }
    void setRoot(VM& vm, Root* root) { m_root.set(vm, this, root); }

    static Element* create(VM& vm, JSGlobalObject* globalObject, Root* root)
    {
        Structure* structure = createStructure(vm, globalObject, jsNull());
        Element* element = new (NotNull, allocateCell<Element>(vm.heap, sizeof(Element))) Element(vm, structure);
        element->finishCreation(vm, root);
        return element;
    }

    void finishCreation(VM&, Root*);

    static void visitChildren(JSCell* cell, SlotVisitor& visitor)
    {
        Element* thisObject = jsCast<Element*>(cell);
        ASSERT_GC_OBJECT_INHERITS(thisObject, info());
        Base::visitChildren(thisObject, visitor);
        visitor.append(thisObject->m_root);
    }

    static ElementHandleOwner* handleOwner();

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;

private:
    WriteBarrier<Root> m_root;
};

class ElementHandleOwner : public WeakHandleOwner {
public:
    bool isReachableFromOpaqueRoots(Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor, const char** reason) override
    {
        if (UNLIKELY(reason))
            *reason = "JSC::Element is opaque root";
        Element* element = jsCast<Element*>(handle.slot()->asCell());
        return visitor.containsOpaqueRoot(element->root());
    }
};

class Root : public JSDestructibleObject {
public:
    Root(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    Element* element()
    {
        return m_element.get();
    }

    void setElement(Element* element)
    {
        Weak<Element> newElement(element, Element::handleOwner());
        m_element.swap(newElement);
    }

    static Root* create(VM& vm, JSGlobalObject* globalObject)
    {
        Structure* structure = createStructure(vm, globalObject, jsNull());
        Root* root = new (NotNull, allocateCell<Root>(vm.heap, sizeof(Root))) Root(vm, structure);
        root->finishCreation(vm);
        return root;
    }

    typedef JSDestructibleObject Base;

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static void visitChildren(JSCell* thisObject, SlotVisitor& visitor)
    {
        Base::visitChildren(thisObject, visitor);
        visitor.addOpaqueRoot(thisObject);
    }

private:
    Weak<Element> m_element;
};

class SimpleObject : public JSNonFinalObject {
public:
    SimpleObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    typedef JSNonFinalObject Base;
    static const bool needsDestruction = false;

    static SimpleObject* create(VM& vm, JSGlobalObject* globalObject)
    {
        Structure* structure = createStructure(vm, globalObject, jsNull());
        SimpleObject* simpleObject = new (NotNull, allocateCell<SimpleObject>(vm.heap, sizeof(SimpleObject))) SimpleObject(vm, structure);
        simpleObject->finishCreation(vm);
        return simpleObject;
    }

    static void visitChildren(JSCell* cell, SlotVisitor& visitor)
    {
        SimpleObject* thisObject = jsCast<SimpleObject*>(cell);
        ASSERT_GC_OBJECT_INHERITS(thisObject, info());
        Base::visitChildren(thisObject, visitor);
        visitor.append(thisObject->m_hiddenValue);
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    JSValue hiddenValue()
    {
        return m_hiddenValue.get();
    }

    void setHiddenValue(VM& vm, JSValue value)
    {
        ASSERT(value.isCell());
        m_hiddenValue.set(vm, this, value);
    }

    DECLARE_INFO;

private:
    WriteBarrier<JSC::Unknown> m_hiddenValue;
};

class ImpureGetter : public JSNonFinalObject {
public:
    ImpureGetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | JSC::GetOwnPropertySlotIsImpure | JSC::OverridesGetOwnPropertySlot;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static ImpureGetter* create(VM& vm, Structure* structure, JSObject* delegate)
    {
        ImpureGetter* getter = new (NotNull, allocateCell<ImpureGetter>(vm.heap, sizeof(ImpureGetter))) ImpureGetter(vm, structure);
        getter->finishCreation(vm, delegate);
        return getter;
    }

    void finishCreation(VM& vm, JSObject* delegate)
    {
        Base::finishCreation(vm);
        if (delegate)
            m_delegate.set(vm, this, delegate);
    }

    static bool getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName name, PropertySlot& slot)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        ImpureGetter* thisObject = jsCast<ImpureGetter*>(object);
        
        if (thisObject->m_delegate) {
            if (thisObject->m_delegate->getPropertySlot(exec, name, slot))
                return true;
            RETURN_IF_EXCEPTION(scope, false);
        }

        return Base::getOwnPropertySlot(object, exec, name, slot);
    }

    static void visitChildren(JSCell* cell, SlotVisitor& visitor)
    {
        Base::visitChildren(cell, visitor);
        ImpureGetter* thisObject = jsCast<ImpureGetter*>(cell);
        visitor.append(thisObject->m_delegate);
    }

    void setDelegate(VM& vm, JSObject* delegate)
    {
        m_delegate.set(vm, this, delegate);
    }

private:
    WriteBarrier<JSObject> m_delegate;
};

class CustomGetter : public JSNonFinalObject {
public:
    CustomGetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | JSC::OverridesGetOwnPropertySlot;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static CustomGetter* create(VM& vm, Structure* structure)
    {
        CustomGetter* getter = new (NotNull, allocateCell<CustomGetter>(vm.heap, sizeof(CustomGetter))) CustomGetter(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    static bool getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
    {
        CustomGetter* thisObject = jsCast<CustomGetter*>(object);
        if (propertyName == PropertyName(Identifier::fromString(exec, "customGetter"))) {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, thisObject->customGetter);
            return true;
        }
        
        if (propertyName == PropertyName(Identifier::fromString(exec, "customGetterAccessor"))) {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor, thisObject->customGetterAcessor);
            return true;
        }
        
        return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
    }

private:
    static EncodedJSValue customGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        CustomGetter* thisObject = jsDynamicCast<CustomGetter*>(vm, JSValue::decode(thisValue));
        if (!thisObject)
            return throwVMTypeError(exec, scope);
        bool shouldThrow = thisObject->get(exec, PropertyName(Identifier::fromString(exec, "shouldThrow"))).toBoolean(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (shouldThrow)
            return throwVMTypeError(exec, scope);
        return JSValue::encode(jsNumber(100));
    }
    
    static EncodedJSValue customGetterAcessor(ExecState* exec, EncodedJSValue thisValue, PropertyName)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        
        JSObject* thisObject = jsDynamicCast<JSObject*>(vm, JSValue::decode(thisValue));
        if (!thisObject)
            return throwVMTypeError(exec, scope);
        bool shouldThrow = thisObject->get(exec, PropertyName(Identifier::fromString(exec, "shouldThrow"))).toBoolean(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        if (shouldThrow)
            return throwVMTypeError(exec, scope);
        return JSValue::encode(jsNumber(100));
    }
};

class RuntimeArray : public JSArray {
public:
    typedef JSArray Base;
    static const unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero | OverridesGetPropertyNames;

    static RuntimeArray* create(ExecState* exec)
    {
        VM& vm = exec->vm();
        JSGlobalObject* globalObject = exec->lexicalGlobalObject();
        Structure* structure = createStructure(vm, globalObject, createPrototype(vm, globalObject));
        RuntimeArray* runtimeArray = new (NotNull, allocateCell<RuntimeArray>(vm.heap)) RuntimeArray(exec, structure);
        runtimeArray->finishCreation(exec);
        vm.heap.addFinalizer(runtimeArray, destroy);
        return runtimeArray;
    }

    ~RuntimeArray() { }

    static void destroy(JSCell* cell)
    {
        static_cast<RuntimeArray*>(cell)->RuntimeArray::~RuntimeArray();
    }

    static const bool needsDestruction = false;

    static bool getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
    {
        VM& vm = exec->vm();
        RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
        if (propertyName == vm.propertyNames->length) {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, thisObject->lengthGetter);
            return true;
        }

        Optional<uint32_t> index = parseIndex(propertyName);
        if (index && index.value() < thisObject->getLength()) {
            slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum, jsNumber(thisObject->m_vector[index.value()]));
            return true;
        }

        return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
    }

    static bool getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned index, PropertySlot& slot)
    {
        RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
        if (index < thisObject->getLength()) {
            slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum, jsNumber(thisObject->m_vector[index]));
            return true;
        }

        return JSObject::getOwnPropertySlotByIndex(thisObject, exec, index, slot);
    }

    static NO_RETURN_DUE_TO_CRASH bool put(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    static NO_RETURN_DUE_TO_CRASH bool deleteProperty(JSCell*, ExecState*, PropertyName)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    unsigned getLength() const { return m_vector.size(); }

    DECLARE_INFO;

    static ArrayPrototype* createPrototype(VM&, JSGlobalObject* globalObject)
    {
        return globalObject->arrayPrototype();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(DerivedArrayType, StructureFlags), info(), ArrayClass);
    }

protected:
    void finishCreation(ExecState* exec)
    {
        VM& vm = exec->vm();
        Base::finishCreation(vm);
        ASSERT(inherits(vm, info()));

        for (size_t i = 0; i < exec->argumentCount(); i++)
            m_vector.append(exec->argument(i).toInt32(exec));
    }

private:
    RuntimeArray(ExecState* exec, Structure* structure)
        : JSArray(exec->vm(), structure, 0)
    {
    }

    static EncodedJSValue lengthGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        RuntimeArray* thisObject = jsDynamicCast<RuntimeArray*>(vm, JSValue::decode(thisValue));
        if (!thisObject)
            return throwVMTypeError(exec, scope);
        return JSValue::encode(jsNumber(thisObject->getLength()));
    }

    Vector<int> m_vector;
};

class DOMJITNode : public JSNonFinalObject {
public:
    DOMJITNode(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

#if ENABLE(JIT)
    static Ref<Snippet> checkSubClassSnippet()
    {
        Ref<Snippet> snippet = Snippet::create();
        snippet->setGenerator([=](CCallHelpers& jit, SnippetParams& params) {
            CCallHelpers::JumpList failureCases;
            failureCases.append(jit.branchIfNotType(params[0].gpr(), JSC::JSType(LastJSCObjectType + 1)));
            return failureCases;
        });
        return snippet;
    }
#endif

    static DOMJITNode* create(VM& vm, Structure* structure)
    {
        DOMJITNode* getter = new (NotNull, allocateCell<DOMJITNode>(vm.heap, sizeof(DOMJITNode))) DOMJITNode(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    int32_t value() const
    {
        return m_value;
    }

    static ptrdiff_t offsetOfValue() { return OBJECT_OFFSETOF(DOMJITNode, m_value); }

private:
    int32_t m_value { 42 };
};

class DOMJITGetter : public DOMJITNode {
public:
    DOMJITGetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static const unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITGetter* create(VM& vm, Structure* structure)
    {
        DOMJITGetter* getter = new (NotNull, allocateCell<DOMJITGetter>(vm.heap, sizeof(DOMJITGetter))) DOMJITGetter(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    class DOMJITAttribute : public DOMJIT::GetterSetter {
    public:
        constexpr DOMJITAttribute()
            : DOMJIT::GetterSetter(
                DOMJITGetter::customGetter,
#if ENABLE(JIT)
                &callDOMGetter,
#else
                nullptr,
#endif
                SpecInt32Only)
        {
        }

#if ENABLE(JIT)
        static EncodedJSValue JIT_OPERATION slowCall(ExecState* exec, void* pointer)
        {
            VM& vm = exec->vm();
            NativeCallFrameTracer tracer(&vm, exec);
            return JSValue::encode(jsNumber(static_cast<DOMJITGetter*>(pointer)->value()));
        }

        static Ref<DOMJIT::CallDOMGetterSnippet> callDOMGetter()
        {
            Ref<DOMJIT::CallDOMGetterSnippet> snippet = DOMJIT::CallDOMGetterSnippet::create();
            snippet->requireGlobalObject = false;
            snippet->setGenerator([=](CCallHelpers& jit, SnippetParams& params) {
                JSValueRegs results = params[0].jsValueRegs();
                GPRReg dom = params[1].gpr();
                params.addSlowPathCall(jit.jump(), jit, slowCall, results, dom);
                return CCallHelpers::JumpList();

            });
            return snippet;
        }
#endif
    };

private:
    void finishCreation(VM&);

    static EncodedJSValue customGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
    {
        VM& vm = exec->vm();
        DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(vm, JSValue::decode(thisValue));
        ASSERT(thisObject);
        return JSValue::encode(jsNumber(thisObject->value()));
    }
};

static const DOMJITGetter::DOMJITAttribute DOMJITGetterDOMJIT;

void DOMJITGetter::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    const DOMJIT::GetterSetter* domJIT = &DOMJITGetterDOMJIT;
    auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJIT->getter(), nullptr, DOMAttributeAnnotation { DOMJITNode::info(), domJIT });
    putDirectCustomAccessor(vm, Identifier::fromString(&vm, "customGetter"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
}

class DOMJITGetterComplex : public DOMJITNode {
public:
    DOMJITGetterComplex(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static const unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITGetterComplex* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        DOMJITGetterComplex* getter = new (NotNull, allocateCell<DOMJITGetterComplex>(vm.heap, sizeof(DOMJITGetterComplex))) DOMJITGetterComplex(vm, structure);
        getter->finishCreation(vm, globalObject);
        return getter;
    }

    class DOMJITAttribute : public DOMJIT::GetterSetter {
    public:
        constexpr DOMJITAttribute()
            : DOMJIT::GetterSetter(
                DOMJITGetterComplex::customGetter,
#if ENABLE(JIT)
                &callDOMGetter,
#else
                nullptr,
#endif
                SpecInt32Only)
        {
        }

#if ENABLE(JIT)
        static EncodedJSValue JIT_OPERATION slowCall(ExecState* exec, void* pointer)
        {
            VM& vm = exec->vm();
            NativeCallFrameTracer tracer(&vm, exec);
            auto scope = DECLARE_THROW_SCOPE(vm);
            auto* object = static_cast<DOMJITNode*>(pointer);
            auto* domjitGetterComplex = jsDynamicCast<DOMJITGetterComplex*>(vm, object);
            if (domjitGetterComplex) {
                if (domjitGetterComplex->m_enableException)
                    return JSValue::encode(throwException(exec, scope, createError(exec, "DOMJITGetterComplex slow call exception"_s)));
            }
            return JSValue::encode(jsNumber(object->value()));
        }

        static Ref<DOMJIT::CallDOMGetterSnippet> callDOMGetter()
        {
            Ref<DOMJIT::CallDOMGetterSnippet> snippet = DOMJIT::CallDOMGetterSnippet::create();
            static_assert(GPRInfo::numberOfRegisters >= 4, "Number of registers should be larger or equal to 4.");
            unsigned numGPScratchRegisters = GPRInfo::numberOfRegisters - 4;
            snippet->numGPScratchRegisters = numGPScratchRegisters;
            snippet->numFPScratchRegisters = 3;
            snippet->setGenerator([=](CCallHelpers& jit, SnippetParams& params) {
                JSValueRegs results = params[0].jsValueRegs();
                GPRReg domGPR = params[1].gpr();
                for (unsigned i = 0; i < numGPScratchRegisters; ++i)
                    jit.move(CCallHelpers::TrustedImm32(42), params.gpScratch(i));

                params.addSlowPathCall(jit.jump(), jit, slowCall, results, domGPR);
                return CCallHelpers::JumpList();
            });
            return snippet;
        }
#endif
    };

private:
    void finishCreation(VM&, JSGlobalObject*);

    static EncodedJSValue JSC_HOST_CALL functionEnableException(ExecState* exec)
    {
        VM& vm = exec->vm();
        auto* object = jsDynamicCast<DOMJITGetterComplex*>(vm, exec->thisValue());
        if (object)
            object->m_enableException = true;
        return JSValue::encode(jsUndefined());
    }

    static EncodedJSValue customGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto* thisObject = jsDynamicCast<DOMJITGetterComplex*>(vm, JSValue::decode(thisValue));
        ASSERT(thisObject);
        if (thisObject->m_enableException)
            return JSValue::encode(throwException(exec, scope, createError(exec, "DOMJITGetterComplex slow call exception"_s)));
        return JSValue::encode(jsNumber(thisObject->value()));
    }

    bool m_enableException { false };
};

static const DOMJITGetterComplex::DOMJITAttribute DOMJITGetterComplexDOMJIT;

void DOMJITGetterComplex::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    const DOMJIT::GetterSetter* domJIT = &DOMJITGetterComplexDOMJIT;
    auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJIT->getter(), nullptr, DOMAttributeAnnotation { DOMJITGetterComplex::info(), domJIT });
    putDirectCustomAccessor(vm, Identifier::fromString(&vm, "customGetter"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "enableException"), 0, functionEnableException, NoIntrinsic, 0);
}

class DOMJITFunctionObject : public DOMJITNode {
public:
    DOMJITFunctionObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static const unsigned StructureFlags = Base::StructureFlags;


    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITFunctionObject* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        DOMJITFunctionObject* object = new (NotNull, allocateCell<DOMJITFunctionObject>(vm.heap, sizeof(DOMJITFunctionObject))) DOMJITFunctionObject(vm, structure);
        object->finishCreation(vm, globalObject);
        return object;
    }

    static EncodedJSValue JSC_HOST_CALL safeFunction(ExecState* exec)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(vm, exec->thisValue());
        if (!thisObject)
            return throwVMTypeError(exec, scope);
        return JSValue::encode(jsNumber(thisObject->value()));
    }

    static EncodedJSValue JIT_OPERATION unsafeFunction(ExecState* exec, DOMJITNode* node)
    {
        VM& vm = exec->vm();
        NativeCallFrameTracer tracer(&vm, exec);
        return JSValue::encode(jsNumber(node->value()));
    }

#if ENABLE(JIT)
    static Ref<Snippet> checkSubClassSnippet()
    {
        Ref<Snippet> snippet = Snippet::create();
        snippet->numFPScratchRegisters = 1;
        snippet->setGenerator([=](CCallHelpers& jit, SnippetParams& params) {
            static const double value = 42.0;
            CCallHelpers::JumpList failureCases;
            // May use scratch registers.
            jit.loadDouble(CCallHelpers::TrustedImmPtr(&value), params.fpScratch(0));
            failureCases.append(jit.branchIfNotType(params[0].gpr(), JSC::JSType(LastJSCObjectType + 1)));
            return failureCases;
        });
        return snippet;
    }
#endif

private:
    void finishCreation(VM&, JSGlobalObject*);
};

static const DOMJIT::Signature DOMJITFunctionObjectSignature((uintptr_t)DOMJITFunctionObject::unsafeFunction, DOMJITFunctionObject::info(), DOMJIT::Effect::forRead(DOMJIT::HeapRange::top()), SpecInt32Only);

void DOMJITFunctionObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "func"), 0, safeFunction, NoIntrinsic, &DOMJITFunctionObjectSignature, static_cast<unsigned>(PropertyAttribute::ReadOnly));
}

class DOMJITCheckSubClassObject : public DOMJITNode {
public:
    DOMJITCheckSubClassObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static const unsigned StructureFlags = Base::StructureFlags;


    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITCheckSubClassObject* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        DOMJITCheckSubClassObject* object = new (NotNull, allocateCell<DOMJITCheckSubClassObject>(vm.heap, sizeof(DOMJITCheckSubClassObject))) DOMJITCheckSubClassObject(vm, structure);
        object->finishCreation(vm, globalObject);
        return object;
    }

    static EncodedJSValue JSC_HOST_CALL safeFunction(ExecState* exec)
    {
        VM& vm = exec->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);

        auto* thisObject = jsDynamicCast<DOMJITCheckSubClassObject*>(vm, exec->thisValue());
        if (!thisObject)
            return throwVMTypeError(exec, scope);
        return JSValue::encode(jsNumber(thisObject->value()));
    }

    static EncodedJSValue JIT_OPERATION unsafeFunction(ExecState* exec, DOMJITNode* node)
    {
        VM& vm = exec->vm();
        NativeCallFrameTracer tracer(&vm, exec);
        return JSValue::encode(jsNumber(node->value()));
    }

private:
    void finishCreation(VM&, JSGlobalObject*);
};

static const DOMJIT::Signature DOMJITCheckSubClassObjectSignature((uintptr_t)DOMJITCheckSubClassObject::unsafeFunction, DOMJITCheckSubClassObject::info(), DOMJIT::Effect::forRead(DOMJIT::HeapRange::top()), SpecInt32Only);

void DOMJITCheckSubClassObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "func"), 0, safeFunction, NoIntrinsic, &DOMJITCheckSubClassObjectSignature, static_cast<unsigned>(PropertyAttribute::ReadOnly));
}

class DOMJITGetterBaseJSObject : public DOMJITNode {
public:
    DOMJITGetterBaseJSObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    DECLARE_INFO;
    using Base = DOMJITNode;
    static const unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITGetterBaseJSObject* create(VM& vm, Structure* structure)
    {
        DOMJITGetterBaseJSObject* getter = new (NotNull, allocateCell<DOMJITGetterBaseJSObject>(vm.heap, sizeof(DOMJITGetterBaseJSObject))) DOMJITGetterBaseJSObject(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    class DOMJITAttribute : public DOMJIT::GetterSetter {
    public:
        constexpr DOMJITAttribute()
            : DOMJIT::GetterSetter(
                DOMJITGetterBaseJSObject::customGetter,
#if ENABLE(JIT)
                &callDOMGetter,
#else
                nullptr,
#endif
                SpecBytecodeTop)
        {
        }

#if ENABLE(JIT)
        static EncodedJSValue JIT_OPERATION slowCall(ExecState* exec, void* pointer)
        {
            VM& vm = exec->vm();
            NativeCallFrameTracer tracer(&vm, exec);
            JSObject* object = static_cast<JSObject*>(pointer);
            return JSValue::encode(object->getPrototypeDirect(vm));
        }

        static Ref<DOMJIT::CallDOMGetterSnippet> callDOMGetter()
        {
            Ref<DOMJIT::CallDOMGetterSnippet> snippet = DOMJIT::CallDOMGetterSnippet::create();
            snippet->requireGlobalObject = false;
            snippet->setGenerator([=](CCallHelpers& jit, SnippetParams& params) {
                JSValueRegs results = params[0].jsValueRegs();
                GPRReg dom = params[1].gpr();
                params.addSlowPathCall(jit.jump(), jit, slowCall, results, dom);
                return CCallHelpers::JumpList();

            });
            return snippet;
        }
#endif
    };

private:
    void finishCreation(VM&);

    static EncodedJSValue customGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
    {
        VM& vm = exec->vm();
        JSObject* thisObject = jsDynamicCast<JSObject*>(vm, JSValue::decode(thisValue));
        RELEASE_ASSERT(thisObject);
        return JSValue::encode(thisObject->getPrototypeDirect(vm));
    }
};

static const DOMJITGetterBaseJSObject::DOMJITAttribute DOMJITGetterBaseJSObjectDOMJIT;

void DOMJITGetterBaseJSObject::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    const DOMJIT::GetterSetter* domJIT = &DOMJITGetterBaseJSObjectDOMJIT;
    auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJIT->getter(), nullptr, DOMAttributeAnnotation { JSObject::info(), domJIT });
    putDirectCustomAccessor(vm, Identifier::fromString(&vm, "customGetter"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
}

class Message : public ThreadSafeRefCounted<Message> {
public:
    Message(ArrayBufferContents&&, int32_t);
    ~Message();

    ArrayBufferContents&& releaseContents() { return WTFMove(m_contents); }
    int32_t index() const { return m_index; }

private:
    ArrayBufferContents m_contents;
    int32_t m_index { 0 };
};

class JSTestCustomGetterSetter : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static const unsigned StructureFlags = Base::StructureFlags;

    JSTestCustomGetterSetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    { }

    static JSTestCustomGetterSetter* create(VM& vm, JSGlobalObject*, Structure* structure)
    {
        JSTestCustomGetterSetter* result = new (NotNull, allocateCell<JSTestCustomGetterSetter>(vm.heap, sizeof(JSTestCustomGetterSetter))) JSTestCustomGetterSetter(vm, structure);
        result->finishCreation(vm);
        return result;
    }

    void finishCreation(VM&);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject)
    {
        return Structure::create(vm, globalObject, globalObject->objectPrototype(), TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;
};


static EncodedJSValue customGetAccessor(ExecState*, EncodedJSValue thisValue, PropertyName)
{
    // Passed |this|
    return thisValue;
}

static EncodedJSValue customGetValue(ExecState* exec, EncodedJSValue slotValue, PropertyName)
{
    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>(exec->vm()));
    // Passed property holder.
    return slotValue;
}

static bool customSetAccessor(ExecState* exec, EncodedJSValue thisObject, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();

    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(value.isObject());
    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, exec, Identifier::fromString(&vm, "result"), JSValue::decode(thisObject), slot);

    return true;
}

static bool customSetValue(ExecState* exec, EncodedJSValue slotValue, EncodedJSValue encodedValue)
{
    VM& vm = exec->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>(exec->vm()));

    JSValue value = JSValue::decode(encodedValue);
    RELEASE_ASSERT(value.isObject());
    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, exec, Identifier::fromString(&vm, "result"), JSValue::decode(slotValue), slot);

    return true;
}

void JSTestCustomGetterSetter::finishCreation(VM& vm)
{
    Base::finishCreation(vm);

    putDirectCustomAccessor(vm, Identifier::fromString(&vm, "customValue"),
        CustomGetterSetter::create(vm, customGetValue, customSetValue), 0);
    putDirectCustomAccessor(vm, Identifier::fromString(&vm, "customAccessor"),
        CustomGetterSetter::create(vm, customGetAccessor, customSetAccessor), static_cast<unsigned>(PropertyAttribute::CustomAccessor));
}

const ClassInfo Element::s_info = { "Element", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(Element) };
const ClassInfo Root::s_info = { "Root", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(Root) };
const ClassInfo SimpleObject::s_info = { "SimpleObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SimpleObject) };
const ClassInfo ImpureGetter::s_info = { "ImpureGetter", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ImpureGetter) };
const ClassInfo CustomGetter::s_info = { "CustomGetter", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(CustomGetter) };
const ClassInfo RuntimeArray::s_info = { "RuntimeArray", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RuntimeArray) };
#if ENABLE(JIT)
const ClassInfo DOMJITNode::s_info = { "DOMJITNode", &Base::s_info, nullptr, &DOMJITNode::checkSubClassSnippet, CREATE_METHOD_TABLE(DOMJITNode) };
#else
const ClassInfo DOMJITNode::s_info = { "DOMJITNode", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITNode) };
#endif
const ClassInfo DOMJITGetter::s_info = { "DOMJITGetter", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetter) };
const ClassInfo DOMJITGetterComplex::s_info = { "DOMJITGetterComplex", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterComplex) };
const ClassInfo DOMJITGetterBaseJSObject::s_info = { "DOMJITGetterBaseJSObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterBaseJSObject) };
#if ENABLE(JIT)
const ClassInfo DOMJITFunctionObject::s_info = { "DOMJITFunctionObject", &Base::s_info, nullptr, &DOMJITFunctionObject::checkSubClassSnippet, CREATE_METHOD_TABLE(DOMJITFunctionObject) };
#else
const ClassInfo DOMJITFunctionObject::s_info = { "DOMJITFunctionObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITFunctionObject) };
#endif
const ClassInfo DOMJITCheckSubClassObject::s_info = { "DOMJITCheckSubClassObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITCheckSubClassObject) };
const ClassInfo JSTestCustomGetterSetter::s_info = { "JSTestCustomGetterSetter", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestCustomGetterSetter) };

ElementHandleOwner* Element::handleOwner()
{
    static ElementHandleOwner* owner = 0;
    if (!owner)
        owner = new ElementHandleOwner();
    return owner;
}

void Element::finishCreation(VM& vm, Root* root)
{
    Base::finishCreation(vm);
    setRoot(vm, root);
    m_root->setElement(this);
}

#if ENABLE(WEBASSEMBLY)

static EncodedJSValue JSC_HOST_CALL functionWasmStreamingParserAddBytes(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionWasmStreamingParserFinalize(ExecState*);

class WasmStreamingParser : public JSDestructibleObject {
public:
    WasmStreamingParser(VM& vm, Structure* structure)
        : Base(vm, structure)
        , m_info(Wasm::ModuleInformation::create())
        , m_streamingParser(m_info.get())
    {
    }

    using Base = JSDestructibleObject;

    static WasmStreamingParser* create(VM& vm, JSGlobalObject* globalObject)
    {
        Structure* structure = createStructure(vm, globalObject, jsNull());
        WasmStreamingParser* result = new (NotNull, allocateCell<WasmStreamingParser>(vm.heap, sizeof(WasmStreamingParser))) WasmStreamingParser(vm, structure);
        result->finishCreation(vm);
        return result;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    Wasm::StreamingParser& streamingParser() { return m_streamingParser; }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);

        JSGlobalObject* globalObject = this->globalObject(vm);
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "addBytes"), 0, functionWasmStreamingParserAddBytes, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "finalize"), 0, functionWasmStreamingParserFinalize, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }

    DECLARE_INFO;

    Ref<Wasm::ModuleInformation> m_info;
    Wasm::StreamingParser m_streamingParser;
};

const ClassInfo WasmStreamingParser::s_info = { "WasmStreamingParser", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WasmStreamingParser) };

EncodedJSValue JSC_HOST_CALL functionWasmStreamingParserAddBytes(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(exec->vm());
    auto* thisObject = jsDynamicCast<WasmStreamingParser*>(vm, exec->thisValue());
    if (!thisObject)
        RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(false)));

    auto data = getWasmBufferFromValue(exec, exec->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(static_cast<int32_t>(thisObject->streamingParser().addBytes(bitwise_cast<const uint8_t*>(data.first), data.second)))));
}

EncodedJSValue JSC_HOST_CALL functionWasmStreamingParserFinalize(ExecState* exec)
{
    VM& vm = exec->vm();
    auto* thisObject = jsDynamicCast<WasmStreamingParser*>(vm, exec->thisValue());
    if (!thisObject)
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsNumber(static_cast<int32_t>(thisObject->streamingParser().finalize())));
}

#endif

} // namespace

namespace JSC {

const ClassInfo JSDollarVM::s_info = { "DollarVM", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDollarVM) };

// Triggers a crash immediately.
// Usage: $vm.crash()
static NO_RETURN_DUE_TO_CRASH EncodedJSValue JSC_HOST_CALL functionCrash(ExecState*)
{
    CRASH();
}

// Executes a breakpoint instruction if the first argument is truthy or is unset.
// Usage: $vm.breakpoint(<condition>)
static EncodedJSValue JSC_HOST_CALL functionBreakpoint(ExecState* exec)
{
    // Nothing should throw here but we might as well double check...
    VM& vm = exec->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    UNUSED_PARAM(scope);
    if (!exec->argumentCount() || exec->argument(0).toBoolean(exec))
        WTFBreakpointTrap();

    return encodedJSUndefined();
}

// Returns true if the current frame is a DFG frame.
// Usage: isDFG = $vm.dfgTrue()
static EncodedJSValue JSC_HOST_CALL functionDFGTrue(ExecState*)
{
    return JSValue::encode(jsBoolean(false));
}

// Returns true if the current frame is a FTL frame.
// Usage: isFTL = $vm.ftlTrue()
static EncodedJSValue JSC_HOST_CALL functionFTLTrue(ExecState*)
{
    return JSValue::encode(jsBoolean(false));
}

static EncodedJSValue JSC_HOST_CALL functionCpuMfence(ExecState*)
{
#if CPU(X86_64) && !OS(WINDOWS)
    asm volatile("mfence" ::: "memory");
#endif
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionCpuRdtsc(ExecState*)
{
#if CPU(X86_64) && !OS(WINDOWS)
    unsigned high;
    unsigned low;
    asm volatile ("rdtsc" : "=a"(low), "=d"(high));
    return JSValue::encode(jsNumber(low));
#else
    return JSValue::encode(jsNumber(0));
#endif
}

static EncodedJSValue JSC_HOST_CALL functionCpuCpuid(ExecState*)
{
#if CPU(X86_64) && !OS(WINDOWS)
    WTF::x86_cpuid();
#endif
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionCpuPause(ExecState*)
{
#if CPU(X86_64) && !OS(WINDOWS)
    asm volatile ("pause" ::: "memory");
#endif
    return JSValue::encode(jsUndefined());
}

// This takes either a JSArrayBuffer, JSArrayBufferView*, or any other object as its first
// argument. The second argument is expected to be an integer.
//
// If the first argument is a JSArrayBuffer, it'll clflush on that buffer
// plus the second argument as a byte offset. It'll also flush on the object
// itself so its length, etc, aren't in the cache.
//
// If the first argument is not a JSArrayBuffer, we load the butterfly
// and clflush at the address of the butterfly.
static EncodedJSValue JSC_HOST_CALL functionCpuClflush(ExecState* exec)
{
#if CPU(X86_64) && !OS(WINDOWS)
    VM& vm = exec->vm();

    if (!exec->argument(1).isInt32())
        return JSValue::encode(jsBoolean(false));

    auto clflush = [] (void* ptr) {
        char* ptrToFlush = static_cast<char*>(ptr);
        asm volatile ("clflush %0" :: "m"(*ptrToFlush) : "memory");
    };

    Vector<void*> toFlush;

    uint32_t offset = exec->argument(1).asUInt32();

    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(vm, exec->argument(0)))
        toFlush.append(bitwise_cast<char*>(view->vector()) + offset);
    else if (JSObject* object = jsDynamicCast<JSObject*>(vm, exec->argument(0))) {
        switch (object->indexingType()) {
        case ALL_INT32_INDEXING_TYPES:
        case ALL_CONTIGUOUS_INDEXING_TYPES:
        case ALL_DOUBLE_INDEXING_TYPES:
            toFlush.append(bitwise_cast<char*>(object->butterfly()) + Butterfly::offsetOfVectorLength());
            toFlush.append(bitwise_cast<char*>(object->butterfly()) + Butterfly::offsetOfPublicLength());
        }
    }

    if (!toFlush.size())
        return JSValue::encode(jsBoolean(false));

    for (void* ptr : toFlush)
        clflush(ptr);
    return JSValue::encode(jsBoolean(true));
#else
    UNUSED_PARAM(exec);
    return JSValue::encode(jsBoolean(false));
#endif
}

class CallerFrameJITTypeFunctor {
public:
    CallerFrameJITTypeFunctor()
        : m_currentFrame(0)
        , m_jitType(JITCode::None)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        if (m_currentFrame++ > 1) {
            m_jitType = visitor->codeBlock()->jitType();
            return StackVisitor::Done;
        }
        return StackVisitor::Continue;
    }
    
    JITCode::JITType jitType() { return m_jitType; }

private:
    mutable unsigned m_currentFrame;
    mutable JITCode::JITType m_jitType;
};

static FunctionExecutable* getExecutableForFunction(JSValue theFunctionValue)
{
    if (!theFunctionValue.isCell())
        return nullptr;
    
    VM& vm = *theFunctionValue.asCell()->vm();
    JSFunction* theFunction = jsDynamicCast<JSFunction*>(vm, theFunctionValue);
    if (!theFunction)
        return nullptr;
    
    FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(vm,
        theFunction->executable());

    return executable;
}

// Returns true if the current frame is a LLInt frame.
// Usage: isLLInt = $vm.llintTrue()
static EncodedJSValue JSC_HOST_CALL functionLLintTrue(ExecState* exec)
{
    if (!exec)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    exec->iterate(functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITCode::InterpreterThunk));
}

// Returns true if the current frame is a baseline JIT frame.
// Usage: isBaselineJIT = $vm.jitTrue()
static EncodedJSValue JSC_HOST_CALL functionJITTrue(ExecState* exec)
{
    if (!exec)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    exec->iterate(functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITCode::BaselineJIT));
}

// Set that the argument function should not be inlined.
// Usage:
// function f() { };
// $vm.noInline(f);
static EncodedJSValue JSC_HOST_CALL functionNoInline(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    
    JSValue theFunctionValue = exec->uncheckedArgument(0);

    if (FunctionExecutable* executable = getExecutableForFunction(theFunctionValue))
        executable->setNeverInline(true);
    
    return JSValue::encode(jsUndefined());
}

// Runs a full GC synchronously.
// Usage: $vm.gc()
static EncodedJSValue JSC_HOST_CALL functionGC(ExecState* exec)
{
    VMInspector::gc(exec);
    return JSValue::encode(jsUndefined());
}

// Runs the edenGC synchronously.
// Usage: $vm.edenGC()
static EncodedJSValue JSC_HOST_CALL functionEdenGC(ExecState* exec)
{
    VMInspector::edenGC(exec);
    return JSValue::encode(jsUndefined());
}

// Gets a JSDollarVMCallFrame for a specified frame index.
// Usage: var callFrame = $vm.callFrame(0) // frame 0 is the top frame.
// Usage: var callFrame = $vm.callFrame() // implies frame 0 i.e. current frame.
//
// This gives you the ability to query the following:
//    callFrame.valid; // false if we asked for a frame beyond the end of the stack, else true.
//    callFrame.callee;
//    callFrame.codeBlock;
//    callFrame.unlinkedCodeBlock;
//    callFrame.executable;
//
// Note: you cannot toString() a codeBlock, unlinkedCodeBlock, or executable because
// there are internal objects and not a JS object. Hence, you cannot do string
// concatenation with them.
static EncodedJSValue JSC_HOST_CALL functionCallFrame(ExecState* exec)
{
    unsigned frameNumber = 1;
    if (exec->argumentCount() >= 1) {
        JSValue value = exec->uncheckedArgument(0);
        if (!value.isUInt32())
            return JSValue::encode(jsUndefined());

        // We need to inc the frame number because the caller would consider
        // its own frame as frame 0. Hence, we need discount the frame for this
        // function.
        frameNumber = value.asUInt32() + 1;
    }

    return JSValue::encode(JSDollarVMCallFrame::create(exec, frameNumber));
}

// Gets a token for the CodeBlock for a specified frame index.
// Usage: codeBlockToken = $vm.codeBlockForFrame(0) // frame 0 is the top frame.
// Usage: codeBlockToken = $vm.codeBlockForFrame() // implies frame 0 i.e. current frame.
static EncodedJSValue JSC_HOST_CALL functionCodeBlockForFrame(ExecState* exec)
{
    unsigned frameNumber = 1;
    if (exec->argumentCount() >= 1) {
        JSValue value = exec->uncheckedArgument(0);
        if (!value.isUInt32())
            return JSValue::encode(jsUndefined());

        // We need to inc the frame number because the caller would consider
        // its own frame as frame 0. Hence, we need discount the frame for this
        // function.
        frameNumber = value.asUInt32() + 1;
    }

    CodeBlock* codeBlock = VMInspector::codeBlockForFrame(exec, frameNumber);
    if (codeBlock)
        return JSValue::encode(codeBlock);
    return JSValue::encode(jsUndefined());
}

static CodeBlock* codeBlockFromArg(ExecState* exec)
{
    VM& vm = exec->vm();
    if (exec->argumentCount() < 1)
        return nullptr;

    JSValue value = exec->uncheckedArgument(0);
    CodeBlock* candidateCodeBlock = nullptr;
    if (value.isCell()) {
        JSFunction* func = jsDynamicCast<JSFunction*>(vm, value.asCell());
        if (func) {
            if (func->isHostFunction())
                candidateCodeBlock = nullptr;
            else
                candidateCodeBlock = func->jsExecutable()->eitherCodeBlock();
        } else
            candidateCodeBlock = reinterpret_cast<CodeBlock*>(value.asCell());
    }

    if (candidateCodeBlock && VMInspector::isValidCodeBlock(exec, candidateCodeBlock))
        return candidateCodeBlock;

    if (candidateCodeBlock)
        dataLog("Invalid codeBlock: ", RawPointer(candidateCodeBlock), " ", value, "\n");
    else
        dataLog("Invalid codeBlock: ", value, "\n");
    return nullptr;
}

// Usage: $vm.print("codeblock = ", $vm.codeBlockFor(functionObj))
// Usage: $vm.print("codeblock = ", $vm.codeBlockFor(codeBlockToken))
// Note: you cannot toString() a codeBlock because it's an internal object and not
// a JS object. Hence, you cannot do string concatenation with it.
static EncodedJSValue JSC_HOST_CALL functionCodeBlockFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    WTF::StringPrintStream stream;
    if (codeBlock) {
        stream.print(*codeBlock);
        return JSValue::encode(jsString(exec, stream.toString()));
    }
    return JSValue::encode(jsUndefined());
}

// Usage: $vm.dumpSourceFor(functionObj)
// Usage: $vm.dumpSourceFor(codeBlockToken)
static EncodedJSValue JSC_HOST_CALL functionDumpSourceFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    if (codeBlock)
        codeBlock->dumpSource();
    return JSValue::encode(jsUndefined());
}

// Usage: $vm.dumpBytecodeFor(functionObj)
// Usage: $vm.dumpBytecodeFor(codeBlock)
static EncodedJSValue JSC_HOST_CALL functionDumpBytecodeFor(ExecState* exec)
{
    CodeBlock* codeBlock = codeBlockFromArg(exec);
    if (codeBlock)
        codeBlock->dumpBytecode();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue doPrint(ExecState* exec, bool addLineFeed)
{
    auto scope = DECLARE_THROW_SCOPE(exec->vm());
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        JSValue arg = exec->uncheckedArgument(i);
        if (arg.isCell()
            && !arg.isObject()
            && !arg.isString()
            && !arg.isBigInt()) {
            dataLog(arg);
            continue;
        }
        String argStr = exec->uncheckedArgument(i).toWTFString(exec);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        dataLog(argStr);
    }
    if (addLineFeed)
        dataLog("\n");
    return JSValue::encode(jsUndefined());
}

// Prints a series of comma separate strings without appending a newline.
// Usage: $vm.dataLog(str1, str2, str3)
static EncodedJSValue JSC_HOST_CALL functionDataLog(ExecState* exec)
{
    const bool addLineFeed = false;
    return doPrint(exec, addLineFeed);
}

// Prints a series of comma separate strings and appends a newline.
// Usage: $vm.print(str1, str2, str3)
static EncodedJSValue JSC_HOST_CALL functionPrint(ExecState* exec)
{
    const bool addLineFeed = true;
    return doPrint(exec, addLineFeed);
}

// Dumps the current CallFrame.
// Usage: $vm.dumpCallFrame()
static EncodedJSValue JSC_HOST_CALL functionDumpCallFrame(ExecState* exec)
{
    // When the callers call this function, they are expecting to dump their
    // own frame. So skip 1 for this frame.
    VMInspector::dumpCallFrame(exec, 1);
    return JSValue::encode(jsUndefined());
}

// Dumps the JS stack.
// Usage: $vm.printStack()
static EncodedJSValue JSC_HOST_CALL functionDumpStack(ExecState* exec)
{
    // When the callers call this function, they are expecting to dump the
    // stack starting their own frame. So skip 1 for this frame.
    VMInspector::dumpStack(exec, 1);
    return JSValue::encode(jsUndefined());
}

// Dumps the current CallFrame.
// Usage: $vm.dumpRegisters(N) // dump the registers of the Nth CallFrame.
// Usage: $vm.dumpRegisters() // dump the registers of the current CallFrame.
// FIXME: Currently, this function dumps the physical frame. We should make
// it dump the logical frame (i.e. be able to dump inlined frames as well).
static EncodedJSValue JSC_HOST_CALL functionDumpRegisters(ExecState* exec)
{
    unsigned requestedFrameIndex = 1;
    if (exec->argumentCount() >= 1) {
        JSValue value = exec->uncheckedArgument(0);
        if (!value.isUInt32())
            return JSValue::encode(jsUndefined());

        // We need to inc the frame number because the caller would consider
        // its own frame as frame 0. Hence, we need discount the frame for this
        // function.
        requestedFrameIndex = value.asUInt32() + 1;
    }

    unsigned frameIndex = 0;
    exec->iterate([&] (StackVisitor& visitor) {
        if (frameIndex++ != requestedFrameIndex)
            return StackVisitor::Continue;
        VMInspector::dumpRegisters(visitor->callFrame());
        return StackVisitor::Done;
    });

    return encodedJSUndefined();
}

// Dumps the internal memory layout of a JSCell.
// Usage: $vm.dumpCell(cell)
static EncodedJSValue JSC_HOST_CALL functionDumpCell(ExecState* exec)
{
    JSValue value = exec->argument(0);
    if (!value.isCell())
        return encodedJSUndefined();
    
    VMInspector::dumpCellMemory(value.asCell());
    return encodedJSUndefined();
}

// Gets the dataLog dump of the indexingMode of the passed value.
// Usage: $vm.print("indexingMode = " + $vm.indexingMode(jsValue))
static EncodedJSValue JSC_HOST_CALL functionIndexingMode(ExecState* exec)
{
    if (!exec->argument(0).isObject())
        return encodedJSUndefined();

    WTF::StringPrintStream stream;
    stream.print(IndexingTypeDump(exec->uncheckedArgument(0).getObject()->indexingMode()));
    return JSValue::encode(jsString(exec, stream.toString()));
}

static EncodedJSValue JSC_HOST_CALL functionInlineCapacity(ExecState* exec)
{
    VM& vm = exec->vm();
    if (auto* object = jsDynamicCast<JSObject*>(vm, exec->argument(0)))
        return JSValue::encode(jsNumber(object->structure(vm)->inlineCapacity()));

    return encodedJSUndefined();
}

// Gets the dataLog dump of a given JS value as a string.
// Usage: $vm.print("value = " + $vm.value(jsValue))
static EncodedJSValue JSC_HOST_CALL functionValue(ExecState* exec)
{
    WTF::StringPrintStream stream;
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        if (i)
            stream.print(", ");
        stream.print(exec->uncheckedArgument(i));
    }
    
    return JSValue::encode(jsString(exec, stream.toString()));
}

// Gets the pid of the current process.
// Usage: $vm.print("pid = " + $vm.getpid())
static EncodedJSValue JSC_HOST_CALL functionGetPID(ExecState*)
{
    return JSValue::encode(jsNumber(getCurrentProcessID()));
}

// Make the globalObject have a bad time. Does nothing if the object is not a JSGlobalObject.
// Usage: $vm.haveABadTime(globalObject)
static EncodedJSValue JSC_HOST_CALL functionHaveABadTime(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    JSValue objValue = exec->argument(0);
    if (!objValue.isObject())
        return JSValue::encode(jsBoolean(false));

    JSObject* obj = asObject(objValue.asCell());
    JSGlobalObject* globalObject = jsDynamicCast<JSGlobalObject*>(vm, obj);
    if (!globalObject)
        JSValue::encode(jsBoolean(false));

    globalObject->haveABadTime(vm);
    return JSValue::encode(jsBoolean(true));
}

// Checks if the object (or its global if the object is not a global) is having a bad time.
// Usage: $vm.isHavingABadTime(obj)
static EncodedJSValue JSC_HOST_CALL functionIsHavingABadTime(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    JSValue objValue = exec->argument(0);
    if (!objValue.isObject())
        return JSValue::encode(jsUndefined());

    JSObject* obj = asObject(objValue.asCell());
    JSGlobalObject* globalObject = jsDynamicCast<JSGlobalObject*>(vm, obj);
    if (globalObject)
        JSValue::encode(jsBoolean(globalObject->isHavingABadTime()));

    globalObject = obj->globalObject();
    if (!globalObject)
        return JSValue::encode(jsUndefined());

    return JSValue::encode(jsBoolean(globalObject->isHavingABadTime()));
}

// Creates a new global object.
// Usage: $vm.createGlobalObject()
static EncodedJSValue JSC_HOST_CALL functionCreateGlobalObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    JSGlobalObject* globalObject = JSGlobalObject::create(vm, JSGlobalObject::createStructure(vm, jsNull()));
    return JSValue::encode(globalObject);
}

static EncodedJSValue JSC_HOST_CALL functionCreateProxy(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(jsUndefined());
    JSObject* jsTarget = asObject(target.asCell());
    Structure* structure = JSProxy::createStructure(vm, exec->lexicalGlobalObject(), jsTarget->getPrototypeDirect(vm), ImpureProxyType);
    JSProxy* proxy = JSProxy::create(vm, structure, jsTarget);
    return JSValue::encode(proxy);
}

static EncodedJSValue JSC_HOST_CALL functionCreateRuntimeArray(ExecState* exec)
{
    JSLockHolder lock(exec);
    RuntimeArray* array = RuntimeArray::create(exec);
    return JSValue::encode(array);
}

static EncodedJSValue JSC_HOST_CALL functionCreateImpureGetter(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    JSValue target = exec->argument(0);
    JSObject* delegate = nullptr;
    if (target.isObject())
        delegate = asObject(target.asCell());
    Structure* structure = ImpureGetter::createStructure(vm, exec->lexicalGlobalObject(), jsNull());
    ImpureGetter* result = ImpureGetter::create(vm, structure, delegate);
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionCreateCustomGetterObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Structure* structure = CustomGetter::createStructure(vm, exec->lexicalGlobalObject(), jsNull());
    CustomGetter* result = CustomGetter::create(vm, structure);
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionCreateDOMJITNodeObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITNode::createStructure(vm, exec->lexicalGlobalObject(), DOMJITGetter::create(vm, DOMJITGetter::createStructure(vm, exec->lexicalGlobalObject(), jsNull())));
    DOMJITNode* result = DOMJITNode::create(vm, structure);
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionCreateDOMJITGetterObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITGetter::createStructure(vm, exec->lexicalGlobalObject(), jsNull());
    DOMJITGetter* result = DOMJITGetter::create(vm, structure);
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionCreateDOMJITGetterComplexObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITGetterComplex::createStructure(vm, exec->lexicalGlobalObject(), jsNull());
    DOMJITGetterComplex* result = DOMJITGetterComplex::create(vm, exec->lexicalGlobalObject(), structure);
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionCreateDOMJITFunctionObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITFunctionObject::createStructure(vm, exec->lexicalGlobalObject(), jsNull());
    DOMJITFunctionObject* result = DOMJITFunctionObject::create(vm, exec->lexicalGlobalObject(), structure);
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionCreateDOMJITCheckSubClassObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITCheckSubClassObject::createStructure(vm, exec->lexicalGlobalObject(), jsNull());
    DOMJITCheckSubClassObject* result = DOMJITCheckSubClassObject::create(vm, exec->lexicalGlobalObject(), structure);
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionCreateDOMJITGetterBaseJSObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITGetterBaseJSObject::createStructure(vm, exec->lexicalGlobalObject(), jsNull());
    DOMJITGetterBaseJSObject* result = DOMJITGetterBaseJSObject::create(vm, structure);
    return JSValue::encode(result);
}

#if ENABLE(WEBASSEMBLY)
static EncodedJSValue JSC_HOST_CALL functionCreateWasmStreamingParser(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(WasmStreamingParser::create(vm, exec->lexicalGlobalObject()));
}
#endif

static EncodedJSValue JSC_HOST_CALL functionSetImpureGetterDelegate(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = exec->argument(0);
    if (!base.isObject())
        return JSValue::encode(jsUndefined());
    JSValue delegate = exec->argument(1);
    if (!delegate.isObject())
        return JSValue::encode(jsUndefined());
    ImpureGetter* impureGetter = jsDynamicCast<ImpureGetter*>(vm, asObject(base.asCell()));
    if (UNLIKELY(!impureGetter)) {
        throwTypeError(exec, scope, "argument is not an ImpureGetter"_s);
        return encodedJSValue();
    }
    impureGetter->setDelegate(vm, asObject(delegate.asCell()));
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionCreateBuiltin(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (exec->argumentCount() < 1 || !exec->argument(0).isString())
        return JSValue::encode(jsUndefined());

    String functionText = asString(exec->argument(0))->value(exec);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    const SourceCode& source = makeSource(functionText, { });
    JSFunction* func = JSFunction::create(vm, createBuiltinExecutable(vm, source, Identifier::fromString(&vm, "foo"), ConstructorKind::None, ConstructAbility::CannotConstruct)->link(vm, source), exec->lexicalGlobalObject());

    return JSValue::encode(func);
}

static EncodedJSValue JSC_HOST_CALL functionGetPrivateProperty(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (exec->argumentCount() < 2 || !exec->argument(1).isString())
        return encodedJSUndefined();

    String str = asString(exec->argument(1))->value(exec);

    const Identifier* ident = vm.propertyNames->lookUpPrivateName(Identifier::fromString(exec, str));
    if (!ident)
        return throwVMError(exec, scope, "Unknown private name.");

    RELEASE_AND_RETURN(scope, JSValue::encode(exec->argument(0).get(exec, *ident)));
}

static EncodedJSValue JSC_HOST_CALL functionCreateRoot(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(Root::create(vm, exec->lexicalGlobalObject()));
}

static EncodedJSValue JSC_HOST_CALL functionCreateElement(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Root* root = jsDynamicCast<Root*>(vm, exec->argument(0));
    if (!root)
        return JSValue::encode(throwException(exec, scope, createError(exec, "Cannot create Element without a Root."_s)));
    return JSValue::encode(Element::create(vm, exec->lexicalGlobalObject(), root));
}

static EncodedJSValue JSC_HOST_CALL functionGetElement(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    Root* root = jsDynamicCast<Root*>(vm, exec->argument(0));
    if (!root)
        return JSValue::encode(jsUndefined());
    Element* result = root->element();
    return JSValue::encode(result ? result : jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionCreateSimpleObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(SimpleObject::create(vm, exec->lexicalGlobalObject()));
}

static EncodedJSValue JSC_HOST_CALL functionGetHiddenValue(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    SimpleObject* simpleObject = jsDynamicCast<SimpleObject*>(vm, exec->argument(0));
    if (UNLIKELY(!simpleObject)) {
        throwTypeError(exec, scope, "Invalid use of getHiddenValue test function"_s);
        return encodedJSValue();
    }
    return JSValue::encode(simpleObject->hiddenValue());
}

static EncodedJSValue JSC_HOST_CALL functionSetHiddenValue(ExecState* exec)
{
    VM& vm = exec->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    SimpleObject* simpleObject = jsDynamicCast<SimpleObject*>(vm, exec->argument(0));
    if (UNLIKELY(!simpleObject)) {
        throwTypeError(exec, scope, "Invalid use of setHiddenValue test function"_s);
        return encodedJSValue();
    }
    JSValue value = exec->argument(1);
    simpleObject->setHiddenValue(vm, value);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionShadowChickenFunctionsOnStack(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (auto* shadowChicken = vm.shadowChicken()) {
        scope.release();
        return JSValue::encode(shadowChicken->functionsOnStack(exec));
    }

    JSArray* result = constructEmptyArray(exec, 0);
    RETURN_IF_EXCEPTION(scope, { });
    StackVisitor::visit(exec, &vm, [&] (StackVisitor& visitor) -> StackVisitor::Status {
        if (visitor->isInlinedFrame())
            return StackVisitor::Continue;
        if (visitor->isWasmFrame())
            return StackVisitor::Continue;
        result->push(exec, jsCast<JSObject*>(visitor->callee().asCell()));
        scope.releaseAssertNoException(); // This function is only called from tests.
        return StackVisitor::Continue;
    });
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionSetGlobalConstRedeclarationShouldNotThrow(ExecState* exec)
{
    VM& vm = exec->vm();
    vm.setGlobalConstRedeclarationShouldThrow(false);
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionFindTypeForExpression(ExecState* exec)
{
    VM& vm = exec->vm();
    RELEASE_ASSERT(vm.typeProfiler());
    vm.typeProfilerLog()->processLogEntries(vm, "jsc Testing API: functionFindTypeForExpression"_s);

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(exec->argument(1).isString());
    String substring = asString(exec->argument(1))->value(exec);
    String sourceCodeText = executable->source().view().toString();
    unsigned offset = static_cast<unsigned>(sourceCodeText.find(substring) + executable->source().startOffset());
    
    String jsonString = vm.typeProfiler()->typeInformationForExpressionAtOffset(TypeProfilerSearchDescriptorNormal, offset, executable->sourceID(), vm);
    return JSValue::encode(JSONParse(exec, jsonString));
}

static EncodedJSValue JSC_HOST_CALL functionReturnTypeFor(ExecState* exec)
{
    VM& vm = exec->vm();
    RELEASE_ASSERT(vm.typeProfiler());
    vm.typeProfilerLog()->processLogEntries(vm, "jsc Testing API: functionReturnTypeFor"_s);

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    unsigned offset = executable->typeProfilingStartOffset(vm);
    String jsonString = vm.typeProfiler()->typeInformationForExpressionAtOffset(TypeProfilerSearchDescriptorFunctionReturn, offset, executable->sourceID(), vm);
    return JSValue::encode(JSONParse(exec, jsonString));
}

static EncodedJSValue JSC_HOST_CALL functionFlattenDictionaryObject(ExecState* exec)
{
    VM& vm = exec->vm();
    JSValue value = exec->argument(0);
    RELEASE_ASSERT(value.isObject() && value.getObject()->structure()->isDictionary());
    value.getObject()->flattenDictionaryObject(vm);
    return encodedJSUndefined();
}

static EncodedJSValue JSC_HOST_CALL functionDumpBasicBlockExecutionRanges(ExecState* exec)
{
    VM& vm = exec->vm();
    RELEASE_ASSERT(vm.controlFlowProfiler());
    vm.controlFlowProfiler()->dumpData();
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionHasBasicBlockExecuted(ExecState* exec)
{
    VM& vm = exec->vm();
    RELEASE_ASSERT(vm.controlFlowProfiler());

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(exec->argument(1).isString());
    String substring = asString(exec->argument(1))->value(exec);
    String sourceCodeText = executable->source().view().toString();
    RELEASE_ASSERT(sourceCodeText.contains(substring));
    int offset = sourceCodeText.find(substring) + executable->source().startOffset();
    
    bool hasExecuted = vm.controlFlowProfiler()->hasBasicBlockAtTextOffsetBeenExecuted(offset, executable->sourceID(), vm);
    return JSValue::encode(jsBoolean(hasExecuted));
}

static EncodedJSValue JSC_HOST_CALL functionBasicBlockExecutionCount(ExecState* exec)
{
    VM& vm = exec->vm();
    RELEASE_ASSERT(vm.controlFlowProfiler());

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(exec->argument(1).isString());
    String substring = asString(exec->argument(1))->value(exec);
    String sourceCodeText = executable->source().view().toString();
    RELEASE_ASSERT(sourceCodeText.contains(substring));
    int offset = sourceCodeText.find(substring) + executable->source().startOffset();
    
    size_t executionCount = vm.controlFlowProfiler()->basicBlockExecutionCountAtTextOffset(offset, executable->sourceID(), vm);
    return JSValue::encode(JSValue(executionCount));
}

static EncodedJSValue JSC_HOST_CALL functionEnableExceptionFuzz(ExecState*)
{
    Options::useExceptionFuzz() = true;
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue changeDebuggerModeWhenIdle(ExecState* exec, DebuggerMode mode)
{
    bool newDebuggerMode = (mode == DebuggerOn);
    if (Options::forceDebuggerBytecodeGeneration() == newDebuggerMode)
        return JSValue::encode(jsUndefined());

    VM* vm = &exec->vm();
    vm->whenIdle([=] () {
        Options::forceDebuggerBytecodeGeneration() = newDebuggerMode;
        vm->deleteAllCode(PreventCollectionAndDeleteAllCode);
        if (mode == DebuggerMode::DebuggerOn)
            vm->ensureShadowChicken();
    });
    return JSValue::encode(jsUndefined());
}

static EncodedJSValue JSC_HOST_CALL functionEnableDebuggerModeWhenIdle(ExecState* exec)
{
    return changeDebuggerModeWhenIdle(exec, DebuggerOn);
}

static EncodedJSValue JSC_HOST_CALL functionDisableDebuggerModeWhenIdle(ExecState* exec)
{
    return changeDebuggerModeWhenIdle(exec, DebuggerOff);
}

static EncodedJSValue JSC_HOST_CALL functionGlobalObjectCount(ExecState* exec)
{
    return JSValue::encode(jsNumber(exec->vm().heap.globalObjectCount()));
}

static EncodedJSValue JSC_HOST_CALL functionGlobalObjectForObject(ExecState* exec)
{
    JSValue value = exec->argument(0);
    RELEASE_ASSERT(value.isObject());
    JSGlobalObject* globalObject = jsCast<JSObject*>(value)->globalObject(exec->vm());
    RELEASE_ASSERT(globalObject);
    return JSValue::encode(globalObject);
}

static EncodedJSValue JSC_HOST_CALL functionGetGetterSetter(ExecState* exec)
{
    JSValue value = exec->argument(0);
    if (!value.isObject())
        return JSValue::encode(jsUndefined());

    JSValue property = exec->argument(1);
    if (!property.isString())
        return JSValue::encode(jsUndefined());

    PropertySlot slot(value, PropertySlot::InternalMethodType::VMInquiry);
    value.getPropertySlot(exec, asString(property)->toIdentifier(exec), slot);

    JSValue result;
    if (slot.isCacheableGetter())
        result = slot.getterSetter();
    else
        result = jsNull();

    return JSValue::encode(result);
}

static EncodedJSValue JSC_HOST_CALL functionLoadGetterFromGetterSetter(ExecState* exec)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    GetterSetter* getterSetter = jsDynamicCast<GetterSetter*>(vm, exec->argument(0));
    if (UNLIKELY(!getterSetter)) {
        throwTypeError(exec, scope, "Invalid use of loadGetterFromGetterSetter test function: argument is not a GetterSetter"_s);
        return encodedJSValue();
    }

    JSObject* getter = getterSetter->getter();
    RELEASE_ASSERT(getter);
    return JSValue::encode(getter);
}

static EncodedJSValue JSC_HOST_CALL functionCreateCustomTestGetterSetter(ExecState* exec)
{
    VM& vm = exec->vm();
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    return JSValue::encode(JSTestCustomGetterSetter::create(vm, globalObject, JSTestCustomGetterSetter::createStructure(vm, globalObject)));
}

static EncodedJSValue JSC_HOST_CALL functionDeltaBetweenButterflies(ExecState* exec)
{
    VM& vm = exec->vm();
    JSObject* a = jsDynamicCast<JSObject*>(vm, exec->argument(0));
    JSObject* b = jsDynamicCast<JSObject*>(vm, exec->argument(1));
    if (!a || !b)
        return JSValue::encode(jsNumber(PNaN));

    ptrdiff_t delta = bitwise_cast<char*>(a->butterfly()) - bitwise_cast<char*>(b->butterfly());
    if (delta < 0)
        return JSValue::encode(jsNumber(PNaN));
    if (delta > std::numeric_limits<int32_t>::max())
        return JSValue::encode(jsNumber(PNaN));
    return JSValue::encode(jsNumber(static_cast<int32_t>(delta)));
}

static EncodedJSValue JSC_HOST_CALL functionTotalGCTime(ExecState* exec)
{
    VM& vm = exec->vm();
    return JSValue::encode(jsNumber(vm.heap.totalGCTime().seconds()));
}

void JSDollarVM::finishCreation(VM& vm)
{
    Base::finishCreation(vm);

    JSGlobalObject* globalObject = this->globalObject(vm);

    auto addFunction = [&] (VM& vm, const char* name, NativeFunction function, unsigned arguments) {
        JSDollarVM::addFunction(vm, globalObject, name, function, arguments);
    };
    auto addConstructibleFunction = [&] (VM& vm, const char* name, NativeFunction function, unsigned arguments) {
        JSDollarVM::addConstructibleFunction(vm, globalObject, name, function, arguments);
    };

    addFunction(vm, "abort", functionCrash, 0);
    addFunction(vm, "crash", functionCrash, 0);
    addFunction(vm, "breakpoint", functionBreakpoint, 0);

    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "dfgTrue"), 0, functionDFGTrue, DFGTrueIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "ftlTrue"), 0, functionFTLTrue, FTLTrueIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));

    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "cpuMfence"), 0, functionCpuMfence, CPUMfenceIntrinsic, 0);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "cpuRdtsc"), 0, functionCpuRdtsc, CPURdtscIntrinsic, 0);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "cpuCpuid"), 0, functionCpuCpuid, CPUCpuidIntrinsic, 0);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(&vm, "cpuPause"), 0, functionCpuPause, CPUPauseIntrinsic, 0);
    addFunction(vm, "cpuClflush", functionCpuClflush, 2);

    addFunction(vm, "llintTrue", functionLLintTrue, 0);
    addFunction(vm, "jitTrue", functionJITTrue, 0);

    addFunction(vm, "noInline", functionNoInline, 1);

    addFunction(vm, "gc", functionGC, 0);
    addFunction(vm, "edenGC", functionEdenGC, 0);

    addFunction(vm, "callFrame", functionCallFrame, 1);
    addFunction(vm, "codeBlockFor", functionCodeBlockFor, 1);
    addFunction(vm, "codeBlockForFrame", functionCodeBlockForFrame, 1);
    addFunction(vm, "dumpSourceFor", functionDumpSourceFor, 1);
    addFunction(vm, "dumpBytecodeFor", functionDumpBytecodeFor, 1);

    addFunction(vm, "dataLog", functionDataLog, 1);
    addFunction(vm, "print", functionPrint, 1);
    addFunction(vm, "dumpCallFrame", functionDumpCallFrame, 0);
    addFunction(vm, "dumpStack", functionDumpStack, 0);
    addFunction(vm, "dumpRegisters", functionDumpRegisters, 1);

    addFunction(vm, "dumpCell", functionDumpCell, 1);

    addFunction(vm, "indexingMode", functionIndexingMode, 1);
    addFunction(vm, "inlineCapacity", functionInlineCapacity, 1);
    addFunction(vm, "value", functionValue, 1);
    addFunction(vm, "getpid", functionGetPID, 0);

    addFunction(vm, "haveABadTime", functionHaveABadTime, 1);
    addFunction(vm, "isHavingABadTime", functionIsHavingABadTime, 1);

    addFunction(vm, "createGlobalObject", functionCreateGlobalObject, 0);
    addFunction(vm, "createProxy", functionCreateProxy, 1);
    addFunction(vm, "createRuntimeArray", functionCreateRuntimeArray, 0);

    addFunction(vm, "createImpureGetter", functionCreateImpureGetter, 1);
    addFunction(vm, "createCustomGetterObject", functionCreateCustomGetterObject, 0);
    addFunction(vm, "createDOMJITNodeObject", functionCreateDOMJITNodeObject, 0);
    addFunction(vm, "createDOMJITGetterObject", functionCreateDOMJITGetterObject, 0);
    addFunction(vm, "createDOMJITGetterComplexObject", functionCreateDOMJITGetterComplexObject, 0);
    addFunction(vm, "createDOMJITFunctionObject", functionCreateDOMJITFunctionObject, 0);
    addFunction(vm, "createDOMJITCheckSubClassObject", functionCreateDOMJITCheckSubClassObject, 0);
    addFunction(vm, "createDOMJITGetterBaseJSObject", functionCreateDOMJITGetterBaseJSObject, 0);
    addFunction(vm, "createBuiltin", functionCreateBuiltin, 2);
#if ENABLE(WEBASSEMBLY)
    addFunction(vm, "createWasmStreamingParser", functionCreateWasmStreamingParser, 0);
#endif
    addFunction(vm, "getPrivateProperty", functionGetPrivateProperty, 2);
    addFunction(vm, "setImpureGetterDelegate", functionSetImpureGetterDelegate, 2);

    addConstructibleFunction(vm, "Root", functionCreateRoot, 0);
    addConstructibleFunction(vm, "Element", functionCreateElement, 1);
    addFunction(vm, "getElement", functionGetElement, 1);

    addConstructibleFunction(vm, "SimpleObject", functionCreateSimpleObject, 0);
    addFunction(vm, "getHiddenValue", functionGetHiddenValue, 1);
    addFunction(vm, "setHiddenValue", functionSetHiddenValue, 2);

    addFunction(vm, "shadowChickenFunctionsOnStack", functionShadowChickenFunctionsOnStack, 0);
    addFunction(vm, "setGlobalConstRedeclarationShouldNotThrow", functionSetGlobalConstRedeclarationShouldNotThrow, 0);

    addFunction(vm, "findTypeForExpression", functionFindTypeForExpression, 2);
    addFunction(vm, "returnTypeFor", functionReturnTypeFor, 1);

    addFunction(vm, "flattenDictionaryObject", functionFlattenDictionaryObject, 1);

    addFunction(vm, "dumpBasicBlockExecutionRanges", functionDumpBasicBlockExecutionRanges , 0);
    addFunction(vm, "hasBasicBlockExecuted", functionHasBasicBlockExecuted, 2);
    addFunction(vm, "basicBlockExecutionCount", functionBasicBlockExecutionCount, 2);

    addFunction(vm, "enableExceptionFuzz", functionEnableExceptionFuzz, 0);

    addFunction(vm, "enableDebuggerModeWhenIdle", functionEnableDebuggerModeWhenIdle, 0);
    addFunction(vm, "disableDebuggerModeWhenIdle", functionDisableDebuggerModeWhenIdle, 0);

    addFunction(vm, "globalObjectCount", functionGlobalObjectCount, 0);
    addFunction(vm, "globalObjectForObject", functionGlobalObjectForObject, 1);

    addFunction(vm, "getGetterSetter", functionGetGetterSetter, 2);
    addFunction(vm, "loadGetterFromGetterSetter", functionLoadGetterFromGetterSetter, 1);
    addFunction(vm, "createCustomTestGetterSetter", functionCreateCustomTestGetterSetter, 1);

    addFunction(vm, "deltaBetweenButterflies", functionDeltaBetweenButterflies, 2);
    
    addFunction(vm, "totalGCTime", functionTotalGCTime, 0);
}

void JSDollarVM::addFunction(VM& vm, JSGlobalObject* globalObject, const char* name, NativeFunction function, unsigned arguments)
{
    Identifier identifier = Identifier::fromString(&vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function));
}

void JSDollarVM::addConstructibleFunction(VM& vm, JSGlobalObject* globalObject, const char* name, NativeFunction function, unsigned arguments)
{
    Identifier identifier = Identifier::fromString(&vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function, NoIntrinsic, function));
}

} // namespace JSC
