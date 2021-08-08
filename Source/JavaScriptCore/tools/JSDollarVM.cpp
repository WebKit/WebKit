/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "ArrayPrototype.h"
#include "BuiltinNames.h"
#include "CodeBlock.h"
#include "ControlFlowProfiler.h"
#include "DOMAttributeGetterSetter.h"
#include "DOMJITGetterSetter.h"
#include "Debugger.h"
#include "FrameTracers.h"
#include "FunctionCodeBlock.h"
#include "GetterSetter.h"
#include "JITSizeStatistics.h"
#include "JSArray.h"
#include "JSCInlines.h"
#include "JSONObject.h"
#include "JSString.h"
#include "LinkBuffer.h"
#include "Options.h"
#include "Parser.h"
#include "ProbeContext.h"
#include "ShadowChicken.h"
#include "Snippet.h"
#include "SnippetParams.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "VMInspector.h"
#include "VMTrapsInlines.h"
#include "WasmCapabilities.h"
#include <unicode/uversion.h>
#include <wtf/Atomics.h>
#include <wtf/CPUTime.h>
#include <wtf/DataLog.h>
#include <wtf/Language.h>
#include <wtf/ProcessID.h>
#include <wtf/StringPrintStream.h>
#include <wtf/unicode/icu/ICUHelpers.h>

#if !USE(SYSTEM_MALLOC)
#include <bmalloc/BPlatform.h>
#if BUSE(LIBPAS)
#ifndef PAS_BMALLOC
#define PAS_BMALLOC 1
#endif
#include <bmalloc/pas_debug_spectrum.h>
#include <bmalloc/pas_fd_stream.h>
#include <bmalloc/pas_heap_lock.h>
#endif
#endif

#if ENABLE(WEBASSEMBLY)
#include "JSWebAssemblyHelpers.h"
#include "WasmModuleInformation.h"
#include "WasmStreamingCompiler.h"
#include "WasmStreamingParser.h"
#endif

using namespace JSC;

IGNORE_WARNINGS_BEGIN("frame-address")

extern "C" void ctiMasmProbeTrampoline();

namespace JSC {

// This class is only here as a simple way to grant JSDollarVM friend privileges
// to all the classes that it needs special access to.
class JSDollarVMHelper {
public:
    JSDollarVMHelper(VM& vm)
        : m_vm(vm)
    { }

    void updateVMStackLimits() { return m_vm.updateStackLimits(); };

    VM& m_vm;
};

} // namespace JSC

namespace {

static JSC_DECLARE_HOST_FUNCTION(functionDOMJITGetterComplexEnableException);
static JSC_DECLARE_HOST_FUNCTION(functionDOMJITFunctionObjectWithTypeCheck);
static JSC_DECLARE_HOST_FUNCTION(functionDOMJITCheckJSCastObjectWithTypeCheck);

// We must RELEASE_ASSERT(Options::useDollarVM()) in all JSDollarVM functions
// that are non-trivial at an eye's glance. This includes (but is not limited to):
//      constructors
//      create() factory
//      createStructure() factory
//      finishCreation()
//      HOST_CALL or operation functions
//      Constructors and methods of utility and test classes
//      lambda functions
//
// The way to do this RELEASE_ASSERT is with the DollarVMAssertScope below.
//
// The only exception are some constexpr constructors used for instantiating
// globals (since these must have trivial constructors) e.g. DOMJITAttribute.
// Instead, these constructors should always be ALWAYS_INLINE.

class JSDollarVMCallFrame : public JSNonFinalObject {
    using Base = JSNonFinalObject;
public:
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    JSDollarVMCallFrame(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static JSDollarVMCallFrame* create(JSGlobalObject* globalObject, CallFrame* callFrame, unsigned requestedFrameIndex)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        Structure* structure = createStructure(vm, globalObject, jsNull());
        JSDollarVMCallFrame* frame = new (NotNull, allocateCell<JSDollarVMCallFrame>(vm.heap)) JSDollarVMCallFrame(vm, structure);
        frame->finishCreation(vm, callFrame, requestedFrameIndex);
        return frame;
    }

    void finishCreation(VM& vm, CallFrame* callFrame, unsigned requestedFrameIndex)
    {
        DollarVMAssertScope assertScope;
        Base::finishCreation(vm);

        auto addProperty = [&] (VM& vm, const char* name, JSValue value) {
            DollarVMAssertScope assertScope;
            JSDollarVMCallFrame::addProperty(vm, name, value);
        };

        unsigned frameIndex = 0;
        bool isValid = false;
        callFrame->iterate(vm, [&] (StackVisitor& visitor) {
            DollarVMAssertScope assertScope;

            if (frameIndex++ != requestedFrameIndex)
                return StackVisitor::Continue;

            addProperty(vm, "name", jsString(vm, visitor->functionName()));

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
        DollarVMAssertScope assertScope;
        Identifier identifier = Identifier::fromString(vm, name);
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
        DollarVMAssertScope assertScope;
    }

    typedef JSNonFinalObject Base;
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    Root* root() const { return m_root.get(); }
    void setRoot(VM& vm, Root* root) { m_root.set(vm, this, root); }

    static Element* create(VM& vm, JSGlobalObject* globalObject, Root* root)
    {
        DollarVMAssertScope assertScope;
        Structure* structure = createStructure(vm, globalObject, jsNull());
        Element* element = new (NotNull, allocateCell<Element>(vm.heap)) Element(vm, structure);
        element->finishCreation(vm, root);
        return element;
    }

    void finishCreation(VM&, Root*);

    DECLARE_VISIT_CHILDREN;

    static ElementHandleOwner* handleOwner();

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;

private:
    WriteBarrier<Root> m_root;
};

template<typename Visitor>
void Element::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    DollarVMAssertScope assertScope;
    Element* thisObject = jsCast<Element*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_root);
}

DEFINE_VISIT_CHILDREN(Element);

class ElementHandleOwner final : public WeakHandleOwner {
    WTF_MAKE_FAST_ALLOCATED;
public:
    bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, AbstractSlotVisitor& visitor, const char** reason) final
    {
        DollarVMAssertScope assertScope;
        if (UNLIKELY(reason))
            *reason = "JSC::Element is opaque root";
        Element* element = jsCast<Element*>(handle.slot()->asCell());
        return visitor.containsOpaqueRoot(element->root());
    }
};

class Root final : public JSDestructibleObject {
public:
    using Base = JSDestructibleObject;
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.destructibleObjectSpace;
    }

    Root(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    Element* element()
    {
        return m_element.get();
    }

    void setElement(Element* element)
    {
        DollarVMAssertScope assertScope;
        Weak<Element> newElement(element, Element::handleOwner());
        m_element.swap(newElement);
    }

    static Root* create(VM& vm, JSGlobalObject* globalObject)
    {
        DollarVMAssertScope assertScope;
        Structure* structure = createStructure(vm, globalObject, jsNull());
        Root* root = new (NotNull, allocateCell<Root>(vm.heap)) Root(vm, structure);
        root->finishCreation(vm);
        return root;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_VISIT_CHILDREN;

private:
    Weak<Element> m_element;
};

template<typename Visitor>
void Root::visitChildrenImpl(JSCell* thisObject, Visitor& visitor)
{
    DollarVMAssertScope assertScope;
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.addOpaqueRoot(thisObject);
}

DEFINE_VISIT_CHILDREN(Root);

class SimpleObject : public JSNonFinalObject {
public:
    SimpleObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    typedef JSNonFinalObject Base;
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    static SimpleObject* create(VM& vm, JSGlobalObject* globalObject)
    {
        DollarVMAssertScope assertScope;
        Structure* structure = createStructure(vm, globalObject, jsNull());
        SimpleObject* simpleObject = new (NotNull, allocateCell<SimpleObject>(vm.heap)) SimpleObject(vm, structure);
        simpleObject->finishCreation(vm);
        return simpleObject;
    }

    DECLARE_VISIT_CHILDREN;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
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

    static CallData getConstructData(JSCell*)
    {
        CallData constructData;
        constructData.type = CallData::Type::Native;
        constructData.native.function = callHostFunctionAsConstructor;
        return constructData;
    }

    DECLARE_INFO;

private:
    WriteBarrier<JSC::Unknown> m_hiddenValue;
};

template<typename Visitor>
void SimpleObject::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    DollarVMAssertScope assertScope;
    SimpleObject* thisObject = jsCast<SimpleObject*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_hiddenValue);
}

DEFINE_VISIT_CHILDREN(SimpleObject);

class ImpureGetter : public JSNonFinalObject {
public:
    ImpureGetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef JSNonFinalObject Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | JSC::GetOwnPropertySlotIsImpure | JSC::OverridesGetOwnPropertySlot;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static ImpureGetter* create(VM& vm, Structure* structure, JSObject* delegate)
    {
        DollarVMAssertScope assertScope;
        ImpureGetter* getter = new (NotNull, allocateCell<ImpureGetter>(vm.heap)) ImpureGetter(vm, structure);
        getter->finishCreation(vm, delegate);
        return getter;
    }

    void finishCreation(VM& vm, JSObject* delegate)
    {
        DollarVMAssertScope assertScope;
        Base::finishCreation(vm);
        if (delegate)
            m_delegate.set(vm, this, delegate);
    }

    static bool getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName name, PropertySlot& slot)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        ImpureGetter* thisObject = jsCast<ImpureGetter*>(object);
        
        if (thisObject->m_delegate) {
            if (thisObject->m_delegate->getPropertySlot(globalObject, name, slot))
                return true;
            RETURN_IF_EXCEPTION(scope, false);
        }

        return Base::getOwnPropertySlot(object, globalObject, name, slot);
    }

    DECLARE_VISIT_CHILDREN;

    void setDelegate(VM& vm, JSObject* delegate)
    {
        m_delegate.set(vm, this, delegate);
    }

private:
    WriteBarrier<JSObject> m_delegate;
};

template<typename Visitor>
void ImpureGetter::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    DollarVMAssertScope assertScope;
    ASSERT_GC_OBJECT_INHERITS(cell, info());
    Base::visitChildren(cell, visitor);
    ImpureGetter* thisObject = jsCast<ImpureGetter*>(cell);
    visitor.append(thisObject->m_delegate);
}

DEFINE_VISIT_CHILDREN(ImpureGetter);

static JSC_DECLARE_CUSTOM_GETTER(customGetterValueGetter);
static JSC_DECLARE_CUSTOM_GETTER(customGetterAcessorGetter);

class CustomGetter : public JSNonFinalObject {
public:
    CustomGetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef JSNonFinalObject Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | JSC::OverridesGetOwnPropertySlot;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static CustomGetter* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        CustomGetter* getter = new (NotNull, allocateCell<CustomGetter>(vm.heap)) CustomGetter(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    static bool getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        CustomGetter* thisObject = jsCast<CustomGetter*>(object);
        if (propertyName == PropertyName(Identifier::fromString(vm, "customGetter"))) {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, customGetterValueGetter);
            return true;
        }
        
        if (propertyName == PropertyName(Identifier::fromString(vm, "customGetterAccessor"))) {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor, customGetterAcessorGetter);
            return true;
        }
        
        return JSObject::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
    }
};

JSC_DEFINE_CUSTOM_GETTER(customGetterValueGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    CustomGetter* thisObject = jsDynamicCast<CustomGetter*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    bool shouldThrow = thisObject->get(globalObject, PropertyName(Identifier::fromString(vm, "shouldThrow"))).toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (shouldThrow)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(jsNumber(100));
}

JSC_DEFINE_CUSTOM_GETTER(customGetterAcessorGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    bool shouldThrow = thisObject->get(globalObject, PropertyName(Identifier::fromString(vm, "shouldThrow"))).toBoolean(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    if (shouldThrow)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(jsNumber(100));
}

static JSC_DECLARE_CUSTOM_GETTER(runtimeArrayLengthGetter);

class RuntimeArray : public JSArray {
public:
    typedef JSArray Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesGetOwnPropertySlot | InterceptsGetOwnPropertySlotByIndexEvenWhenLengthIsNotZero;

IGNORE_WARNINGS_BEGIN("unused-const-variable")
    static constexpr bool needsDestruction = false;
IGNORE_WARNINGS_END

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    static RuntimeArray* create(JSGlobalObject* globalObject, CallFrame* callFrame)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        Structure* structure = createStructure(vm, globalObject, createPrototype(vm, globalObject));
        RuntimeArray* runtimeArray = new (NotNull, allocateCell<RuntimeArray>(vm.heap)) RuntimeArray(globalObject, structure);
        runtimeArray->finishCreation(globalObject, callFrame);
        vm.heap.addFinalizer(runtimeArray, destroy);
        return runtimeArray;
    }

    ~RuntimeArray() { }

    static void destroy(JSCell* cell)
    {
        DollarVMAssertScope assertScope;
        static_cast<RuntimeArray*>(cell)->RuntimeArray::~RuntimeArray();
    }

    static bool getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
        if (propertyName == vm.propertyNames->length) {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, runtimeArrayLengthGetter);
            return true;
        }

        std::optional<uint32_t> index = parseIndex(propertyName);
        if (index && index.value() < thisObject->getLength()) {
            slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum, jsNumber(thisObject->m_vector[index.value()]));
            return true;
        }

        return JSObject::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
    }

    static bool getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* globalObject, unsigned index, PropertySlot& slot)
    {
        DollarVMAssertScope assertScope;
        RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
        if (index < thisObject->getLength()) {
            slot.setValue(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::DontEnum, jsNumber(thisObject->m_vector[index]));
            return true;
        }

        return JSObject::getOwnPropertySlotByIndex(thisObject, globalObject, index, slot);
    }

    static NO_RETURN_DUE_TO_CRASH bool put(JSCell*, JSGlobalObject*, PropertyName, JSValue, PutPropertySlot&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    static NO_RETURN_DUE_TO_CRASH bool deleteProperty(JSCell*, JSGlobalObject*, PropertyName, DeletePropertySlot&)
    {
        RELEASE_ASSERT_NOT_REACHED();
    }

    unsigned getLength() const { return m_vector.size(); }

    DECLARE_INFO;

    static ArrayPrototype* createPrototype(VM&, JSGlobalObject* globalObject)
    {
        DollarVMAssertScope assertScope;
        return globalObject->arrayPrototype();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(DerivedArrayType, StructureFlags), info(), ArrayClass);
    }

protected:
    void finishCreation(JSGlobalObject* globalObject, CallFrame* callFrame)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        Base::finishCreation(vm);
        ASSERT(inherits(vm, info()));

        for (size_t i = 0; i < callFrame->argumentCount(); i++)
            m_vector.append(callFrame->argument(i).toInt32(globalObject));
    }

private:
    RuntimeArray(JSGlobalObject* globalObject, Structure* structure)
        : JSArray(globalObject->vm(), structure, nullptr)
    {
        DollarVMAssertScope assertScope;
    }

    Vector<int> m_vector;
};

JSC_DEFINE_CUSTOM_GETTER(runtimeArrayLengthGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    RuntimeArray* thisObject = jsDynamicCast<RuntimeArray*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(jsNumber(thisObject->getLength()));
}

static JSC_DECLARE_CUSTOM_GETTER(testStaticAccessorGetter);
static JSC_DECLARE_CUSTOM_SETTER(testStaticAccessorPutter);

JSC_DEFINE_CUSTOM_GETTER(testStaticAccessorGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);

    if (JSValue result = thisObject->getDirect(vm, PropertyName(Identifier::fromString(vm, "testField"))))
        return JSValue::encode(result);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_CUSTOM_SETTER(testStaticAccessorPutter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSValue receiver = JSValue::decode(thisValue);
    JSObject* thisObject = receiver.isObject() ? asObject(receiver) : receiver.synthesizePrototype(globalObject);
    RETURN_IF_EXCEPTION(scope, false);
    RELEASE_ASSERT(thisObject);

    return thisObject->putDirect(vm, PropertyName(Identifier::fromString(vm, "testField")), JSValue::decode(value));
}

static const struct CompactHashIndex staticCustomAccessorTableIndex[9] = {
    { -1, -1 },
    { -1, -1 },
    { 2, -1 },
    { -1, -1 },
    { 0, 8 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 1, -1 },
};

static const struct HashTableValue staticCustomAccessorTableValues[3] = {
    { "testStaticAccessor", static_cast<unsigned>(PropertyAttribute::CustomAccessor), NoIntrinsic, { (intptr_t)static_cast<PropertySlot::GetValueFunc>(testStaticAccessorGetter), (intptr_t)static_cast<PutPropertySlot::PutValueFunc>(testStaticAccessorPutter) } },
    { "testStaticAccessorDontEnum", PropertyAttribute::CustomAccessor | PropertyAttribute::DontEnum, NoIntrinsic, { (intptr_t)static_cast<GetValueFunc>(testStaticAccessorGetter), (intptr_t)static_cast<PutValueFunc>(testStaticAccessorPutter) } },
    { "testStaticAccessorReadOnly", PropertyAttribute::CustomAccessor | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, NoIntrinsic, { (intptr_t)static_cast<GetValueFunc>(testStaticAccessorGetter), 0 } },
};

static const struct HashTable staticCustomAccessorTable =
    { 3, 7, true, nullptr, staticCustomAccessorTableValues, staticCustomAccessorTableIndex };

class StaticCustomAccessor : public JSNonFinalObject {
    using Base = JSNonFinalObject;
public:
    StaticCustomAccessor(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;

    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable | OverridesGetOwnPropertySlot;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static StaticCustomAccessor* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        StaticCustomAccessor* accessor = new (NotNull, allocateCell<StaticCustomAccessor>(vm.heap)) StaticCustomAccessor(vm, structure);
        accessor->finishCreation(vm);
        return accessor;
    }

    static bool getOwnPropertySlot(JSObject* thisObject, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
    {
        if (String(propertyName.uid()) == "thinAirCustomGetter") {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::CustomAccessor, testStaticAccessorGetter);
            return true;
        }
        return JSNonFinalObject::getOwnPropertySlot(thisObject, globalObject, propertyName, slot);
    }
};

static JSC_DECLARE_CUSTOM_GETTER(testStaticValueGetter);
static JSC_DECLARE_CUSTOM_SETTER(testStaticValuePutter);
static JSC_DECLARE_CUSTOM_SETTER(testStaticValuePutterSetFlag);

JSC_DEFINE_CUSTOM_GETTER(testStaticValueGetter, (JSGlobalObject*, EncodedJSValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_CUSTOM_SETTER(testStaticValuePutter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue value, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);

    return thisObject->putDirect(vm, PropertyName(Identifier::fromString(vm, "testStaticValue")), JSValue::decode(value));
}

JSC_DEFINE_CUSTOM_SETTER(testStaticValuePutterSetFlag, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);

    return thisObject->putDirect(vm, PropertyName(Identifier::fromString(vm, "testStaticValueSetterCalled")), jsBoolean(true));
}

static const struct CompactHashIndex staticCustomValueTableIndex[8] = {
    { 1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 3, -1 },
    { 2, -1 },
    { 0, -1 },
    { -1, -1 },
};

static const struct HashTableValue staticCustomValueTableValues[4] = {
    { "testStaticValue", static_cast<unsigned>(PropertyAttribute::CustomValue), NoIntrinsic, { (intptr_t)static_cast<GetValueFunc>(testStaticValueGetter), (intptr_t)static_cast<PutValueFunc>(testStaticValuePutter) } },
    { "testStaticValueNoSetter", PropertyAttribute::CustomValue | PropertyAttribute::DontEnum, NoIntrinsic, { (intptr_t)static_cast<GetValueFunc>(testStaticValueGetter), 0 } },
    { "testStaticValueReadOnly", PropertyAttribute::CustomValue | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, NoIntrinsic, { (intptr_t)static_cast<GetValueFunc>(testStaticValueGetter), 0 } },
    { "testStaticValueSetFlag", static_cast<unsigned>(PropertyAttribute::CustomValue), NoIntrinsic, { (intptr_t)static_cast<GetValueFunc>(testStaticValueGetter), (intptr_t)static_cast<PutValueFunc>(testStaticValuePutterSetFlag) } },
};

static const struct HashTable staticCustomValueTable =
    { 4, 7, true, nullptr, staticCustomValueTableValues, staticCustomValueTableIndex };

class StaticCustomValue : public JSNonFinalObject {
    using Base = JSNonFinalObject;
public:
    StaticCustomValue(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;

    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static StaticCustomValue* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        StaticCustomValue* accessor = new (NotNull, allocateCell<StaticCustomValue>(vm.heap)) StaticCustomValue(vm, structure);
        accessor->finishCreation(vm);
        return accessor;
    }
};

class ObjectDoingSideEffectPutWithoutCorrectSlotStatus : public JSNonFinalObject {
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesPut;
public:
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    ObjectDoingSideEffectPutWithoutCorrectSlotStatus(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static ObjectDoingSideEffectPutWithoutCorrectSlotStatus* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        ObjectDoingSideEffectPutWithoutCorrectSlotStatus* accessor = new (NotNull, allocateCell<ObjectDoingSideEffectPutWithoutCorrectSlotStatus>(vm.heap)) ObjectDoingSideEffectPutWithoutCorrectSlotStatus(vm, structure);
        accessor->finishCreation(vm);
        return accessor;
    }

    static bool put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
    {
        DollarVMAssertScope assertScope;
        auto* thisObject = jsCast<ObjectDoingSideEffectPutWithoutCorrectSlotStatus*>(cell);
        auto throwScope = DECLARE_THROW_SCOPE(globalObject->vm());
        auto* string = value.toString(globalObject);
        RETURN_IF_EXCEPTION(throwScope, false);
        RELEASE_AND_RETURN(throwScope, Base::put(thisObject, globalObject, propertyName, string, slot));
    }
};

class DOMJITNode : public JSNonFinalObject {
public:
    DOMJITNode(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef JSNonFinalObject Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

#if ENABLE(JIT)
    static Ref<Snippet> checkSubClassSnippet()
    {
        DollarVMAssertScope assertScope;
        Ref<Snippet> snippet = Snippet::create();
        snippet->setGenerator([=] (CCallHelpers& jit, SnippetParams& params) {
            DollarVMAssertScope assertScope;
            CCallHelpers::JumpList failureCases;
            failureCases.append(jit.branchIfNotType(params[0].gpr(), JSC::JSType(LastJSCObjectType + 1)));
            return failureCases;
        });
        return snippet;
    }
#endif

    static DOMJITNode* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        DOMJITNode* getter = new (NotNull, allocateCell<DOMJITNode>(vm.heap)) DOMJITNode(vm, structure);
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


static JSC_DECLARE_CUSTOM_GETTER(domJITGetterCustomGetter);
static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterSlowCall, EncodedJSValue, (JSGlobalObject*, void*));

class DOMJITGetter : public DOMJITNode {
public:
    DOMJITGetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITGetter* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        DOMJITGetter* getter = new (NotNull, allocateCell<DOMJITGetter>(vm.heap)) DOMJITGetter(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    class DOMJITAttribute : public DOMJIT::GetterSetter {
    public:
        ALWAYS_INLINE constexpr DOMJITAttribute()
            : DOMJIT::GetterSetter(
                domJITGetterCustomGetter,
#if ENABLE(JIT)
                &callDOMGetter,
#else
                nullptr,
#endif
                SpecInt32Only)
        {
        }

#if ENABLE(JIT)
        static Ref<DOMJIT::CallDOMGetterSnippet> callDOMGetter()
        {
            DollarVMAssertScope assertScope;
            Ref<DOMJIT::CallDOMGetterSnippet> snippet = DOMJIT::CallDOMGetterSnippet::create();
            snippet->requireGlobalObject = true;
            snippet->setGenerator([=] (CCallHelpers& jit, SnippetParams& params) {
                DollarVMAssertScope assertScope;
                JSValueRegs results = params[0].jsValueRegs();
                GPRReg domGPR = params[1].gpr();
                GPRReg globalObjectGPR = params[2].gpr();
                params.addSlowPathCall(jit.jump(), jit, domJITGetterSlowCall, results, globalObjectGPR, domGPR);
                return CCallHelpers::JumpList();

            });
            return snippet;
        }
#endif
    };

private:
    void finishCreation(VM&);
};

static const DOMJITGetter::DOMJITAttribute DOMJITGetterDOMJIT;

void DOMJITGetter::finishCreation(VM& vm)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);
    {
        const DOMJIT::GetterSetter* domJIT = &DOMJITGetterDOMJIT;
        auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJIT->getter(), nullptr, DOMAttributeAnnotation { DOMJITNode::info(), domJIT });
        putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
    }
    {
        auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJITGetterCustomGetter, nullptr, DOMAttributeAnnotation { DOMJITNode::info(), nullptr });
        putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter2"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
    }
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(jsNumber(thisObject->value()));
}

JSC_DEFINE_JIT_OPERATION(domJITGetterSlowCall, EncodedJSValue, (JSGlobalObject* globalObject, void* pointer))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(jsNumber(static_cast<DOMJITGetter*>(pointer)->value()));
}


static JSC_DECLARE_CUSTOM_GETTER(domJITGetterNoEffectCustomGetter);
static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterNoEffectSlowCall, EncodedJSValue, (JSGlobalObject*, void*));

class DOMJITGetterNoEffects : public DOMJITNode {
public:
    DOMJITGetterNoEffects(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITGetterNoEffects* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        DOMJITGetterNoEffects* getter = new (NotNull, allocateCell<DOMJITGetterNoEffects>(vm.heap)) DOMJITGetterNoEffects(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    class DOMJITAttribute : public DOMJIT::GetterSetter {
    public:
        ALWAYS_INLINE constexpr DOMJITAttribute()
            : DOMJIT::GetterSetter(
                domJITGetterNoEffectCustomGetter,
#if ENABLE(JIT)
                &callDOMGetter,
#else
                nullptr,
#endif
                SpecInt32Only)
        {
        }

#if ENABLE(JIT)
        static Ref<DOMJIT::CallDOMGetterSnippet> callDOMGetter()
        {
            DollarVMAssertScope assertScope;
            Ref<DOMJIT::CallDOMGetterSnippet> snippet = DOMJIT::CallDOMGetterSnippet::create();
            snippet->effect = DOMJIT::Effect::forRead(DOMJIT::HeapRange(10, 11));
            snippet->requireGlobalObject = true;
            snippet->setGenerator([=] (CCallHelpers& jit, SnippetParams& params) {
                DollarVMAssertScope assertScope;
                JSValueRegs results = params[0].jsValueRegs();
                GPRReg domGPR = params[1].gpr();
                GPRReg globalObjectGPR = params[2].gpr();
                params.addSlowPathCall(jit.jump(), jit, domJITGetterNoEffectSlowCall, results, globalObjectGPR, domGPR);
                return CCallHelpers::JumpList();

            });
            return snippet;
        }
#endif
    };

private:
    void finishCreation(VM&);
};

static const DOMJITGetterNoEffects::DOMJITAttribute DOMJITGetterNoEffectsDOMJIT;

void DOMJITGetterNoEffects::finishCreation(VM& vm)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);
    const DOMJIT::GetterSetter* domJIT = &DOMJITGetterNoEffectsDOMJIT;
    auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJIT->getter(), nullptr, DOMAttributeAnnotation { DOMJITNode::info(), domJIT });
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterNoEffectCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(jsNumber(thisObject->value()));
}

JSC_DEFINE_JIT_OPERATION(domJITGetterNoEffectSlowCall, EncodedJSValue, (JSGlobalObject* globalObject, void* pointer))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(jsNumber(static_cast<DOMJITGetterNoEffects*>(pointer)->value()));
}

static JSC_DECLARE_CUSTOM_GETTER(domJITGetterComplexCustomGetter);
static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterComplexSlowCall, EncodedJSValue, (JSGlobalObject*, void*));

class DOMJITGetterComplex : public DOMJITNode {
public:
    DOMJITGetterComplex(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITGetterComplex* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        DOMJITGetterComplex* getter = new (NotNull, allocateCell<DOMJITGetterComplex>(vm.heap)) DOMJITGetterComplex(vm, structure);
        getter->finishCreation(vm, globalObject);
        return getter;
    }

    class DOMJITAttribute : public DOMJIT::GetterSetter {
    public:
        ALWAYS_INLINE constexpr DOMJITAttribute()
            : DOMJIT::GetterSetter(
                domJITGetterComplexCustomGetter,
#if ENABLE(JIT)
                &callDOMGetter,
#else
                nullptr,
#endif
                SpecInt32Only)
        {
        }

#if ENABLE(JIT)
        static Ref<DOMJIT::CallDOMGetterSnippet> callDOMGetter()
        {
            DollarVMAssertScope assertScope;
            Ref<DOMJIT::CallDOMGetterSnippet> snippet = DOMJIT::CallDOMGetterSnippet::create();
            static_assert(GPRInfo::numberOfRegisters >= 4, "Number of registers should be larger or equal to 4.");
            unsigned numGPScratchRegisters = GPRInfo::numberOfRegisters - 4;
            snippet->numGPScratchRegisters = numGPScratchRegisters;
            snippet->numFPScratchRegisters = 3;
            snippet->requireGlobalObject = true;
            snippet->setGenerator([=] (CCallHelpers& jit, SnippetParams& params) {
                DollarVMAssertScope assertScope;
                JSValueRegs results = params[0].jsValueRegs();
                GPRReg domGPR = params[1].gpr();
                GPRReg globalObjectGPR = params[2].gpr();
                for (unsigned i = 0; i < numGPScratchRegisters; ++i)
                    jit.move(CCallHelpers::TrustedImm32(42), params.gpScratch(i));

                params.addSlowPathCall(jit.jump(), jit, domJITGetterComplexSlowCall, results, globalObjectGPR, domGPR);
                return CCallHelpers::JumpList();
            });
            return snippet;
        }
#endif
    };

    void finishCreation(VM&, JSGlobalObject*);

    bool m_enableException { false };
};

JSC_DEFINE_HOST_FUNCTION(functionDOMJITGetterComplexEnableException, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto* object = jsDynamicCast<DOMJITGetterComplex*>(vm, callFrame->thisValue());
    if (object)
        object->m_enableException = true;
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterComplexCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsDynamicCast<DOMJITGetterComplex*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    if (thisObject->m_enableException)
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "DOMJITGetterComplex slow call exception"_s)));
    return JSValue::encode(jsNumber(thisObject->value()));
}

JSC_DEFINE_JIT_OPERATION(domJITGetterComplexSlowCall, EncodedJSValue, (JSGlobalObject* globalObject, void* pointer))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto* object = static_cast<DOMJITNode*>(pointer);
    auto* domjitGetterComplex = jsDynamicCast<DOMJITGetterComplex*>(vm, object);
    if (domjitGetterComplex) {
        if (domjitGetterComplex->m_enableException)
            return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "DOMJITGetterComplex slow call exception"_s)));
    }
    return JSValue::encode(jsNumber(object->value()));
}

static const DOMJITGetterComplex::DOMJITAttribute DOMJITGetterComplexDOMJIT;

void DOMJITGetterComplex::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);
    const DOMJIT::GetterSetter* domJIT = &DOMJITGetterComplexDOMJIT;
    auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJIT->getter(), nullptr, DOMAttributeAnnotation { DOMJITGetterComplex::info(), domJIT });
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "enableException"), 0, functionDOMJITGetterComplexEnableException, NoIntrinsic, 0);
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionDOMJITFunctionObjectWithoutTypeCheck, EncodedJSValue, (JSGlobalObject* globalObject, DOMJITNode*));

class DOMJITFunctionObject : public DOMJITNode {
public:
    DOMJITFunctionObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITFunctionObject* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        DOMJITFunctionObject* object = new (NotNull, allocateCell<DOMJITFunctionObject>(vm.heap)) DOMJITFunctionObject(vm, structure);
        object->finishCreation(vm, globalObject);
        return object;
    }

#if ENABLE(JIT)
    static Ref<Snippet> checkSubClassSnippet()
    {
        DollarVMAssertScope assertScope;
        Ref<Snippet> snippet = Snippet::create();
        snippet->numFPScratchRegisters = 1;
        snippet->setGenerator([=] (CCallHelpers& jit, SnippetParams& params) {
            DollarVMAssertScope assertScope;
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

JSC_DEFINE_HOST_FUNCTION(functionDOMJITFunctionObjectWithTypeCheck, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(vm, callFrame->thisValue());
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(jsNumber(thisObject->value()));
}

JSC_DEFINE_JIT_OPERATION(functionDOMJITFunctionObjectWithoutTypeCheck, EncodedJSValue, (JSGlobalObject* globalObject, DOMJITNode* node))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(jsNumber(node->value()));
}

static const DOMJIT::Signature DOMJITFunctionObjectSignature(functionDOMJITFunctionObjectWithoutTypeCheck, DOMJITFunctionObject::info(), DOMJIT::Effect::forRead(DOMJIT::HeapRange(DOMJIT::HeapRange::ConstExpr, 0, 1)), SpecInt32Only);

void DOMJITFunctionObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "func"), 0, functionDOMJITFunctionObjectWithTypeCheck, NoIntrinsic, &DOMJITFunctionObjectSignature, static_cast<unsigned>(PropertyAttribute::ReadOnly));
}

static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionDOMJITCheckJSCastObjectWithoutTypeCheck, EncodedJSValue, (JSGlobalObject* globalObject, DOMJITNode* node));

class DOMJITCheckJSCastObject : public DOMJITNode {
public:
    DOMJITCheckJSCastObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    typedef DOMJITNode Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITCheckJSCastObject* create(VM& vm, JSGlobalObject* globalObject, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        DOMJITCheckJSCastObject* object = new (NotNull, allocateCell<DOMJITCheckJSCastObject>(vm.heap)) DOMJITCheckJSCastObject(vm, structure);
        object->finishCreation(vm, globalObject);
        return object;
    }

private:
    void finishCreation(VM&, JSGlobalObject*);
};

JSC_DEFINE_HOST_FUNCTION(functionDOMJITCheckJSCastObjectWithTypeCheck, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsDynamicCast<DOMJITCheckJSCastObject*>(vm, callFrame->thisValue());
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(jsNumber(thisObject->value()));
}

JSC_DEFINE_JIT_OPERATION(functionDOMJITCheckJSCastObjectWithoutTypeCheck, EncodedJSValue, (JSGlobalObject* globalObject, DOMJITNode* node))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    return JSValue::encode(jsNumber(node->value()));
}

static const DOMJIT::Signature DOMJITCheckJSCastObjectSignature(functionDOMJITCheckJSCastObjectWithoutTypeCheck, DOMJITCheckJSCastObject::info(), DOMJIT::Effect::forRead(DOMJIT::HeapRange::top()), SpecInt32Only);

void DOMJITCheckJSCastObject::finishCreation(VM& vm, JSGlobalObject* globalObject)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "func"), 0, functionDOMJITCheckJSCastObjectWithTypeCheck, NoIntrinsic, &DOMJITCheckJSCastObjectSignature, static_cast<unsigned>(PropertyAttribute::ReadOnly));
}

static JSC_DECLARE_CUSTOM_GETTER(domJITGetterBaseJSObjectCustomGetter);
static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterBaseJSObjectSlowCall, EncodedJSValue, (JSGlobalObject*, void*));

class DOMJITGetterBaseJSObject : public DOMJITNode {
public:
    DOMJITGetterBaseJSObject(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;
    using Base = DOMJITNode;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(JSC::JSType(LastJSCObjectType + 1), StructureFlags), info());
    }

    static DOMJITGetterBaseJSObject* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        DOMJITGetterBaseJSObject* getter = new (NotNull, allocateCell<DOMJITGetterBaseJSObject>(vm.heap)) DOMJITGetterBaseJSObject(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    class DOMJITAttribute : public DOMJIT::GetterSetter {
    public:
        ALWAYS_INLINE constexpr DOMJITAttribute()
            : DOMJIT::GetterSetter(
                domJITGetterBaseJSObjectCustomGetter,
#if ENABLE(JIT)
                &callDOMGetter,
#else
                nullptr,
#endif
                SpecBytecodeTop)
        {
        }

#if ENABLE(JIT)
        static Ref<DOMJIT::CallDOMGetterSnippet> callDOMGetter()
        {
            DollarVMAssertScope assertScope;
            Ref<DOMJIT::CallDOMGetterSnippet> snippet = DOMJIT::CallDOMGetterSnippet::create();
            snippet->requireGlobalObject = true;
            snippet->setGenerator([=] (CCallHelpers& jit, SnippetParams& params) {
                DollarVMAssertScope assertScope;
                JSValueRegs results = params[0].jsValueRegs();
                GPRReg domGPR = params[1].gpr();
                GPRReg globalObjectGPR = params[2].gpr();
                params.addSlowPathCall(jit.jump(), jit, domJITGetterBaseJSObjectSlowCall, results, globalObjectGPR, domGPR);
                return CCallHelpers::JumpList();

            });
            return snippet;
        }
#endif
    };

private:
    void finishCreation(VM&);
};

static const DOMJITGetterBaseJSObject::DOMJITAttribute DOMJITGetterBaseJSObjectDOMJIT;

void DOMJITGetterBaseJSObject::finishCreation(VM& vm)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);
    const DOMJIT::GetterSetter* domJIT = &DOMJITGetterBaseJSObjectDOMJIT;
    auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJIT->getter(), nullptr, DOMAttributeAnnotation { JSObject::info(), domJIT });
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterBaseJSObjectCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* thisObject = jsDynamicCast<JSObject*>(vm, JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(thisObject->getPrototypeDirect(vm));
}

JSC_DEFINE_JIT_OPERATION(domJITGetterBaseJSObjectSlowCall, EncodedJSValue, (JSGlobalObject* globalObject, void* pointer))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSObject* object = static_cast<JSObject*>(pointer);
    return JSValue::encode(object->getPrototypeDirect(vm));
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
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace;
    }

    JSTestCustomGetterSetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    static JSTestCustomGetterSetter* create(VM& vm, JSGlobalObject*, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        JSTestCustomGetterSetter* result = new (NotNull, allocateCell<JSTestCustomGetterSetter>(vm.heap)) JSTestCustomGetterSetter(vm, structure);
        result->finishCreation(vm);
        return result;
    }

    void finishCreation(VM&);

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, globalObject->objectPrototype(), TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;
};


static JSC_DECLARE_CUSTOM_GETTER(customGetAccessor);
static JSC_DECLARE_CUSTOM_GETTER(customGetValue);
static JSC_DECLARE_CUSTOM_GETTER(customGetValue2);
static JSC_DECLARE_CUSTOM_GETTER(customGetAccessorGlobalObject);
static JSC_DECLARE_CUSTOM_GETTER(customGetValueGlobalObject);
static JSC_DECLARE_CUSTOM_SETTER(customSetAccessor);
static JSC_DECLARE_CUSTOM_SETTER(customSetAccessorGlobalObject);
static JSC_DECLARE_CUSTOM_SETTER(customSetValue);
static JSC_DECLARE_CUSTOM_SETTER(customSetValue2);
static JSC_DECLARE_CUSTOM_SETTER(customSetValueGlobalObject);
static JSC_DECLARE_CUSTOM_SETTER(customFunctionSetter);

JSC_DEFINE_CUSTOM_GETTER(customGetAccessor, (JSGlobalObject*, EncodedJSValue thisValue, PropertyName))
{
    // Passed |this|
    return thisValue;
}

JSC_DEFINE_CUSTOM_GETTER(customGetValue, (JSGlobalObject* globalObject, EncodedJSValue slotValue, PropertyName))
{
    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>(globalObject->vm()));
    // Passed property holder.
    return slotValue;
}

JSC_DEFINE_CUSTOM_GETTER(customGetValue2, (JSGlobalObject* globalObject, EncodedJSValue slotValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>(globalObject->vm()));

    auto* target = jsCast<JSTestCustomGetterSetter*>(JSValue::decode(slotValue));
    JSValue value = target->getDirect(vm, Identifier::fromString(vm, "value2"));
    return JSValue::encode(value ? value : jsUndefined());
}

JSC_DEFINE_CUSTOM_GETTER(customGetAccessorGlobalObject, (JSGlobalObject* globalObject, EncodedJSValue, PropertyName))
{
    return JSValue::encode(globalObject);
}

JSC_DEFINE_CUSTOM_GETTER(customGetValueGlobalObject, (JSGlobalObject* globalObject, EncodedJSValue, PropertyName))
{
    return JSValue::encode(globalObject);
}

JSC_DEFINE_CUSTOM_SETTER(customSetAccessor, (JSGlobalObject* globalObject, EncodedJSValue thisObject, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    JSValue value = JSValue::decode(encodedValue);
    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, globalObject, Identifier::fromString(vm, "result"), JSValue::decode(thisObject), slot);

    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customSetAccessorGlobalObject, (JSGlobalObject* globalObject, EncodedJSValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    JSValue value = JSValue::decode(encodedValue);
    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, globalObject, Identifier::fromString(vm, "result"), globalObject, slot);

    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customSetValue, (JSGlobalObject* globalObject, EncodedJSValue slotValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>(globalObject->vm()));

    JSValue value = JSValue::decode(encodedValue);
    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, globalObject, Identifier::fromString(vm, "result"), JSValue::decode(slotValue), slot);

    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customSetValueGlobalObject, (JSGlobalObject* globalObject, EncodedJSValue slotValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>(globalObject->vm()));

    JSValue value = JSValue::decode(encodedValue);
    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, globalObject, Identifier::fromString(vm, "result"), globalObject, slot);

    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customSetValue2, (JSGlobalObject* globalObject, EncodedJSValue slotValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>(vm));
    auto* target = jsCast<JSTestCustomGetterSetter*>(JSValue::decode(slotValue));
    PutPropertySlot slot(target);
    target->putDirect(vm, Identifier::fromString(vm, "value2"), JSValue::decode(encodedValue));
    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customFunctionSetter, (JSGlobalObject* globalObject, EncodedJSValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    JSValue value = JSValue::decode(encodedValue);
    JSFunction* function = jsDynamicCast<JSFunction*>(vm, value);
    if (!function)
        return false;

    auto callData = getCallData(vm, function);
    MarkedArgumentBuffer args;
    call(globalObject, function, callData, jsUndefined(), args);

    return true;
}

void JSTestCustomGetterSetter::finishCreation(VM& vm)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);

    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValue"),
        CustomGetterSetter::create(vm, customGetValue, customSetValue), 0);
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValue2"),
        CustomGetterSetter::create(vm, customGetValue2, customSetValue2), static_cast<unsigned>(PropertyAttribute::CustomValue));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customAccessor"),
        CustomGetterSetter::create(vm, customGetAccessor, customSetAccessor), static_cast<unsigned>(PropertyAttribute::CustomAccessor));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValueGlobalObject"),
        CustomGetterSetter::create(vm, customGetValueGlobalObject, customSetValueGlobalObject), static_cast<unsigned>(PropertyAttribute::CustomValue));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customAccessorGlobalObject"),
        CustomGetterSetter::create(vm, customGetAccessorGlobalObject, customSetAccessorGlobalObject), static_cast<unsigned>(PropertyAttribute::CustomAccessor));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValueNoSetter"),
        CustomGetterSetter::create(vm, customGetValue, nullptr), static_cast<unsigned>(PropertyAttribute::CustomValue));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customAccessorReadOnly"),
        CustomGetterSetter::create(vm, customGetAccessor, nullptr), PropertyAttribute::CustomAccessor | PropertyAttribute::ReadOnly);

    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customFunction"),
        CustomGetterSetter::create(vm, customGetAccessor, customFunctionSetter), static_cast<unsigned>(PropertyAttribute::CustomAccessor));

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
const ClassInfo DOMJITGetterNoEffects::s_info = { "DOMJITGetterNoEffects", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterNoEffects) };
const ClassInfo DOMJITGetterComplex::s_info = { "DOMJITGetterComplex", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterComplex) };
const ClassInfo DOMJITGetterBaseJSObject::s_info = { "DOMJITGetterBaseJSObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterBaseJSObject) };
#if ENABLE(JIT)
const ClassInfo DOMJITFunctionObject::s_info = { "DOMJITFunctionObject", &Base::s_info, nullptr, &DOMJITFunctionObject::checkSubClassSnippet, CREATE_METHOD_TABLE(DOMJITFunctionObject) };
#else
const ClassInfo DOMJITFunctionObject::s_info = { "DOMJITFunctionObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITFunctionObject) };
#endif
const ClassInfo DOMJITCheckJSCastObject::s_info = { "DOMJITCheckJSCastObject", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITCheckJSCastObject) };
const ClassInfo JSTestCustomGetterSetter::s_info = { "JSTestCustomGetterSetter", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestCustomGetterSetter) };

const ClassInfo StaticCustomAccessor::s_info = { "StaticCustomAccessor", &Base::s_info, &staticCustomAccessorTable, nullptr, CREATE_METHOD_TABLE(StaticCustomAccessor) };
const ClassInfo StaticCustomValue::s_info = { "StaticCustomValue", &Base::s_info, &staticCustomValueTable, nullptr, CREATE_METHOD_TABLE(StaticCustomValue) };
const ClassInfo ObjectDoingSideEffectPutWithoutCorrectSlotStatus::s_info = { "ObjectDoingSideEffectPutWithoutCorrectSlotStatus", &Base::s_info, &staticCustomAccessorTable, nullptr, CREATE_METHOD_TABLE(ObjectDoingSideEffectPutWithoutCorrectSlotStatus) };

ElementHandleOwner* Element::handleOwner()
{
    DollarVMAssertScope assertScope;
    static ElementHandleOwner* owner = nullptr;
    if (!owner)
        owner = new ElementHandleOwner();
    return owner;
}

void Element::finishCreation(VM& vm, Root* root)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);
    setRoot(vm, root);
    m_root->setElement(this);
}

#if ENABLE(WEBASSEMBLY)

static JSC_DECLARE_HOST_FUNCTION(functionWasmStreamingParserAddBytes);
static JSC_DECLARE_HOST_FUNCTION(functionWasmStreamingParserFinalize);

class WasmStreamingParser : public JSDestructibleObject {
public:
    using Base = JSDestructibleObject;
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.destructibleObjectSpace;
    }

    class Client final : public Wasm::StreamingParserClient {
    public:
        explicit Client(WasmStreamingParser* parser)
            : m_parser(parser)
        {
        }

        bool didReceiveSectionData(Wasm::Section) final { return true; }
        bool didReceiveFunctionData(unsigned, const Wasm::FunctionData&) final { return true; }
        void didFinishParsing() final { }

        WasmStreamingParser* m_parser;
    };

    WasmStreamingParser(VM& vm, Structure* structure)
        : Base(vm, structure)
        , m_info(Wasm::ModuleInformation::create())
        , m_client(this)
        , m_streamingParser(m_info.get(), m_client)
    {
        DollarVMAssertScope assertScope;
    }

    static WasmStreamingParser* create(VM& vm, JSGlobalObject* globalObject)
    {
        DollarVMAssertScope assertScope;
        Structure* structure = createStructure(vm, globalObject, jsNull());
        WasmStreamingParser* result = new (NotNull, allocateCell<WasmStreamingParser>(vm.heap)) WasmStreamingParser(vm, structure);
        result->finishCreation(vm);
        return result;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    Wasm::StreamingParser& streamingParser() { return m_streamingParser; }

    void finishCreation(VM& vm)
    {
        DollarVMAssertScope assertScope;
        Base::finishCreation(vm);

        JSGlobalObject* globalObject = this->globalObject(vm);
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "addBytes"), 0, functionWasmStreamingParserAddBytes, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "finalize"), 0, functionWasmStreamingParserFinalize, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }

    DECLARE_INFO;

    Ref<Wasm::ModuleInformation> m_info;
    Client m_client;
    Wasm::StreamingParser m_streamingParser;
};

const ClassInfo WasmStreamingParser::s_info = { "WasmStreamingParser", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WasmStreamingParser) };

JSC_DEFINE_HOST_FUNCTION(functionWasmStreamingParserAddBytes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

    auto* thisObject = jsDynamicCast<WasmStreamingParser*>(vm, callFrame->thisValue());
    if (!thisObject)
        RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(false)));

    auto data = getWasmBufferFromValue(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(static_cast<int32_t>(thisObject->streamingParser().addBytes(bitwise_cast<const uint8_t*>(data.first), data.second)))));
}

JSC_DEFINE_HOST_FUNCTION(functionWasmStreamingParserFinalize, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto* thisObject = jsDynamicCast<WasmStreamingParser*>(vm, callFrame->thisValue());
    if (!thisObject)
        return JSValue::encode(jsBoolean(false));
    return JSValue::encode(jsNumber(static_cast<int32_t>(thisObject->streamingParser().finalize())));
}

static JSC_DECLARE_HOST_FUNCTION(functionWasmStreamingCompilerAddBytes);

class WasmStreamingCompiler : public JSDestructibleObject {
public:
    using Base = JSDestructibleObject;
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.destructibleObjectSpace;
    }

    WasmStreamingCompiler(VM& vm, Structure* structure, Wasm::CompilerMode compilerMode, JSGlobalObject* globalObject, JSPromise* promise, JSObject* importObject)
        : Base(vm, structure)
        , m_promise(vm, this, promise)
        , m_streamingCompiler(Wasm::StreamingCompiler::create(vm, compilerMode, globalObject, promise, importObject))
    {
        DollarVMAssertScope assertScope;
    }

    static WasmStreamingCompiler* create(VM& vm, JSGlobalObject* globalObject, Wasm::CompilerMode compilerMode, JSObject* importObject)
    {
        DollarVMAssertScope assertScope;
        JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
        Structure* structure = createStructure(vm, globalObject, jsNull());
        WasmStreamingCompiler* result = new (NotNull, allocateCell<WasmStreamingCompiler>(vm.heap)) WasmStreamingCompiler(vm, structure, compilerMode, globalObject, promise, importObject);
        result->finishCreation(vm);
        return result;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    Wasm::StreamingCompiler& streamingCompiler() { return m_streamingCompiler.get(); }

    JSPromise* promise() const { return m_promise.get(); }

    void finishCreation(VM& vm)
    {
        DollarVMAssertScope assertScope;
        Base::finishCreation(vm);

        JSGlobalObject* globalObject = this->globalObject(vm);
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "addBytes"), 0, functionWasmStreamingCompilerAddBytes, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }

    DECLARE_VISIT_CHILDREN;

    DECLARE_INFO;

    WriteBarrier<JSPromise> m_promise;
    Ref<Wasm::StreamingCompiler> m_streamingCompiler;
};

template<typename Visitor>
void WasmStreamingCompiler::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    DollarVMAssertScope assertScope;
    auto* thisObject = jsCast<WasmStreamingCompiler*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_promise);
}

DEFINE_VISIT_CHILDREN(WasmStreamingCompiler);

const ClassInfo WasmStreamingCompiler::s_info = { "WasmStreamingCompiler", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WasmStreamingCompiler) };

JSC_DEFINE_HOST_FUNCTION(functionWasmStreamingCompilerAddBytes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());

    auto* thisObject = jsDynamicCast<WasmStreamingCompiler*>(vm, callFrame->thisValue());
    if (!thisObject)
        RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(false)));

    auto data = getWasmBufferFromValue(globalObject, callFrame->argument(0));
    RETURN_IF_EXCEPTION(scope, { });
    thisObject->streamingCompiler().addBytes(bitwise_cast<const uint8_t*>(data.first), data.second);
    return JSValue::encode(jsUndefined());
}

#endif

} // namespace

namespace JSC {

static NO_RETURN_DUE_TO_CRASH JSC_DECLARE_HOST_FUNCTION(functionCrash);
static JSC_DECLARE_HOST_FUNCTION(functionBreakpoint);
static JSC_DECLARE_HOST_FUNCTION(functionDFGTrue);
static JSC_DECLARE_HOST_FUNCTION(functionFTLTrue);
static JSC_DECLARE_HOST_FUNCTION(functionCpuMfence);
static JSC_DECLARE_HOST_FUNCTION(functionCpuRdtsc);
static JSC_DECLARE_HOST_FUNCTION(functionCpuCpuid);
static JSC_DECLARE_HOST_FUNCTION(functionCpuPause);
static JSC_DECLARE_HOST_FUNCTION(functionCpuClflush);
static JSC_DECLARE_HOST_FUNCTION(functionLLintTrue);
static JSC_DECLARE_HOST_FUNCTION(functionBaselineJITTrue);
static JSC_DECLARE_HOST_FUNCTION(functionNoInline);
static JSC_DECLARE_HOST_FUNCTION(functionGC);
static JSC_DECLARE_HOST_FUNCTION(functionEdenGC);
static JSC_DECLARE_HOST_FUNCTION(functionGCSweepAsynchronously);
static JSC_DECLARE_HOST_FUNCTION(functionDumpSubspaceHashes);
static JSC_DECLARE_HOST_FUNCTION(functionCallFrame);
static JSC_DECLARE_HOST_FUNCTION(functionCodeBlockForFrame);
static JSC_DECLARE_HOST_FUNCTION(functionCodeBlockFor);
static JSC_DECLARE_HOST_FUNCTION(functionDumpSourceFor);
static JSC_DECLARE_HOST_FUNCTION(functionDumpBytecodeFor);
static JSC_DECLARE_HOST_FUNCTION(functionDataLog);
static JSC_DECLARE_HOST_FUNCTION(functionPrint);
static JSC_DECLARE_HOST_FUNCTION(functionDumpCallFrame);
static JSC_DECLARE_HOST_FUNCTION(functionDumpStack);
static JSC_DECLARE_HOST_FUNCTION(functionDumpRegisters);
static JSC_DECLARE_HOST_FUNCTION(functionDumpCell);
static JSC_DECLARE_HOST_FUNCTION(functionIndexingMode);
static JSC_DECLARE_HOST_FUNCTION(functionInlineCapacity);
static JSC_DECLARE_HOST_FUNCTION(functionClearLinkBufferStats);
static JSC_DECLARE_HOST_FUNCTION(functionLinkBufferStats);
static JSC_DECLARE_HOST_FUNCTION(functionValue);
static JSC_DECLARE_HOST_FUNCTION(functionGetPID);
static JSC_DECLARE_HOST_FUNCTION(functionHaveABadTime);
static JSC_DECLARE_HOST_FUNCTION(functionIsHavingABadTime);
static JSC_DECLARE_HOST_FUNCTION(functionCallWithStackSize);
static JSC_DECLARE_HOST_FUNCTION(functionCreateGlobalObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateProxy);
static JSC_DECLARE_HOST_FUNCTION(functionCreateRuntimeArray);
static JSC_DECLARE_HOST_FUNCTION(functionCreateNullRopeString);
static JSC_DECLARE_HOST_FUNCTION(functionCreateImpureGetter);
static JSC_DECLARE_HOST_FUNCTION(functionCreateCustomGetterObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateDOMJITNodeObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateDOMJITGetterObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateDOMJITGetterNoEffectsObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateDOMJITGetterComplexObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateDOMJITFunctionObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateDOMJITCheckJSCastObject);
static JSC_DECLARE_HOST_FUNCTION(functionCreateDOMJITGetterBaseJSObject);
#if ENABLE(WEBASSEMBLY)
static JSC_DECLARE_HOST_FUNCTION(functionCreateWasmStreamingParser);
static JSC_DECLARE_HOST_FUNCTION(functionCreateWasmStreamingCompilerForCompile);
static JSC_DECLARE_HOST_FUNCTION(functionCreateWasmStreamingCompilerForInstantiate);
#endif
static JSC_DECLARE_HOST_FUNCTION(functionCreateStaticCustomAccessor);
static JSC_DECLARE_HOST_FUNCTION(functionCreateStaticCustomValue);
static JSC_DECLARE_HOST_FUNCTION(functionCreateObjectDoingSideEffectPutWithoutCorrectSlotStatus);
static JSC_DECLARE_HOST_FUNCTION(functionCreateEmptyFunctionWithName);
static JSC_DECLARE_HOST_FUNCTION(functionSetImpureGetterDelegate);
static JSC_DECLARE_HOST_FUNCTION(functionCreateBuiltin);
static JSC_DECLARE_HOST_FUNCTION(functionGetPrivateProperty);
static JSC_DECLARE_HOST_FUNCTION(functionCreateRoot);
static JSC_DECLARE_HOST_FUNCTION(functionCreateElement);
static JSC_DECLARE_HOST_FUNCTION(functionGetElement);
static JSC_DECLARE_HOST_FUNCTION(functionCreateSimpleObject);
static JSC_DECLARE_HOST_FUNCTION(functionGetHiddenValue);
static JSC_DECLARE_HOST_FUNCTION(functionSetHiddenValue);
static JSC_DECLARE_HOST_FUNCTION(functionShadowChickenFunctionsOnStack);
static JSC_DECLARE_HOST_FUNCTION(functionSetGlobalConstRedeclarationShouldNotThrow);
static JSC_DECLARE_HOST_FUNCTION(functionFindTypeForExpression);
static JSC_DECLARE_HOST_FUNCTION(functionReturnTypeFor);
static JSC_DECLARE_HOST_FUNCTION(functionFlattenDictionaryObject);
static JSC_DECLARE_HOST_FUNCTION(functionDumpBasicBlockExecutionRanges);
static JSC_DECLARE_HOST_FUNCTION(functionHasBasicBlockExecuted);
static JSC_DECLARE_HOST_FUNCTION(functionBasicBlockExecutionCount);
static JSC_DECLARE_HOST_FUNCTION(functionEnableDebuggerModeWhenIdle);
static JSC_DECLARE_HOST_FUNCTION(functionDisableDebuggerModeWhenIdle);
static JSC_DECLARE_HOST_FUNCTION(functionDeleteAllCodeWhenIdle);
static JSC_DECLARE_HOST_FUNCTION(functionGlobalObjectCount);
static JSC_DECLARE_HOST_FUNCTION(functionGlobalObjectForObject);
static JSC_DECLARE_HOST_FUNCTION(functionGetGetterSetter);
static JSC_DECLARE_HOST_FUNCTION(functionLoadGetterFromGetterSetter);
static JSC_DECLARE_HOST_FUNCTION(functionCreateCustomTestGetterSetter);
static JSC_DECLARE_HOST_FUNCTION(functionDeltaBetweenButterflies);
static JSC_DECLARE_HOST_FUNCTION(functionCurrentCPUTime);
static JSC_DECLARE_HOST_FUNCTION(functionTotalGCTime);
static JSC_DECLARE_HOST_FUNCTION(functionParseCount);
static JSC_DECLARE_HOST_FUNCTION(functionIsWasmSupported);
static JSC_DECLARE_HOST_FUNCTION(functionMake16BitStringIfPossible);
static JSC_DECLARE_HOST_FUNCTION(functionGetStructureTransitionList);;
static JSC_DECLARE_HOST_FUNCTION(functionGetConcurrently);
static JSC_DECLARE_HOST_FUNCTION(functionHasOwnLengthProperty);
static JSC_DECLARE_HOST_FUNCTION(functionRejectPromiseAsHandled);
static JSC_DECLARE_HOST_FUNCTION(functionSetUserPreferredLanguages);
static JSC_DECLARE_HOST_FUNCTION(functionICUVersion);
static JSC_DECLARE_HOST_FUNCTION(functionICUHeaderVersion);
static JSC_DECLARE_HOST_FUNCTION(functionAssertEnabled);
static JSC_DECLARE_HOST_FUNCTION(functionSecurityAssertEnabled);
static JSC_DECLARE_HOST_FUNCTION(functionAsanEnabled);
static JSC_DECLARE_HOST_FUNCTION(functionIsMemoryLimited);
static JSC_DECLARE_HOST_FUNCTION(functionUseJIT);
static JSC_DECLARE_HOST_FUNCTION(functionIsGigacageEnabled);
static JSC_DECLARE_HOST_FUNCTION(functionToUncacheableDictionary);
static JSC_DECLARE_HOST_FUNCTION(functionIsPrivateSymbol);
static JSC_DECLARE_HOST_FUNCTION(functionDumpAndResetPasDebugSpectrum);
#if ENABLE(JIT)
static JSC_DECLARE_HOST_FUNCTION(functionJITSizeStatistics);
static JSC_DECLARE_HOST_FUNCTION(functionDumpJITSizeStatistics);
static JSC_DECLARE_HOST_FUNCTION(functionResetJITSizeStatistics);
#endif

const ClassInfo JSDollarVM::s_info = { "DollarVM", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDollarVM) };

static EncodedJSValue doPrint(JSGlobalObject* globalObject, CallFrame* callFrame, bool addLineFeed)
{
    DollarVMAssertScope assertScope;
    auto scope = DECLARE_THROW_SCOPE(globalObject->vm());
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        JSValue arg = callFrame->uncheckedArgument(i);
        if (arg.isCell()
            && !arg.isObject()
            && !arg.isString()
            && !arg.isBigInt()) {
            dataLog(arg);
            continue;
        }
        String argStr = callFrame->uncheckedArgument(i).toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        dataLog(argStr);
    }
    if (addLineFeed)
        dataLog("\n");
    return JSValue::encode(jsUndefined());
}

// Triggers a crash after dumping any paramater passed to it.
// Usage: $vm.crash(...)
JSC_DEFINE_HOST_FUNCTION_WITH_ATTRIBUTES(functionCrash, NO_RETURN_DUE_TO_CRASH, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;

    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    if (callFrame->argumentCount()) {
        dataLogLn("Dumping ", callFrame->argumentCount(), " values before crashing:");
        const bool addLineFeed = true;
        doPrint(globalObject, callFrame, addLineFeed);
        if (scope.exception()) {
            JSValue value = scope.exception()->value();
            scope.clearException();
            dataLogLn("Error thrown while crashing: ", value.toWTFString(globalObject));
        }
    }

    CRASH();
}

// Executes a breakpoint instruction if the first argument is truthy or is unset.
// Usage: $vm.breakpoint(<condition>)
JSC_DEFINE_HOST_FUNCTION(functionBreakpoint, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    // Nothing should throw here but we might as well double check...
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    UNUSED_PARAM(scope);
    if (!callFrame->argumentCount() || callFrame->argument(0).toBoolean(globalObject))
        WTFBreakpointTrap();

    return encodedJSUndefined();
}

// Returns true if the current frame is a DFG frame.
// Usage: isDFG = $vm.dfgTrue()
JSC_DEFINE_HOST_FUNCTION(functionDFGTrue, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsBoolean(false));
}

// Returns true if the current frame is a FTL frame.
// Usage: isFTL = $vm.ftlTrue()
JSC_DEFINE_HOST_FUNCTION(functionFTLTrue, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsBoolean(false));
}

JSC_DEFINE_HOST_FUNCTION(functionCpuMfence, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if CPU(X86_64) && !OS(WINDOWS)
    asm volatile("mfence" ::: "memory");
#endif
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionCpuRdtsc, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if CPU(X86_64) && !OS(WINDOWS)
    unsigned high;
    unsigned low;
    asm volatile ("rdtsc" : "=a"(low), "=d"(high));
    return JSValue::encode(jsNumber(low));
#else
    return JSValue::encode(jsNumber(0));
#endif
}

JSC_DEFINE_HOST_FUNCTION(functionCpuCpuid, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if CPU(X86_64) && !OS(WINDOWS)
    WTF::x86_cpuid();
#endif
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionCpuPause, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
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
JSC_DEFINE_HOST_FUNCTION(functionCpuClflush, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
#if CPU(X86_64) && !OS(WINDOWS)
    VM& vm = globalObject->vm();

    if (!callFrame->argument(1).isUInt32())
        return JSValue::encode(jsBoolean(false));

    auto clflush = [] (void* ptr) {
        DollarVMAssertScope assertScope;
        char* ptrToFlush = static_cast<char*>(ptr);
        asm volatile ("clflush %0" :: "m"(*ptrToFlush) : "memory");
    };

    Vector<void*> toFlush;

    uint32_t offset = callFrame->argument(1).asUInt32();

    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(vm, callFrame->argument(0)))
        toFlush.append(bitwise_cast<char*>(view->vector()) + offset);
    else if (JSObject* object = jsDynamicCast<JSObject*>(vm, callFrame->argument(0))) {
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
    UNUSED_PARAM(globalObject);
    UNUSED_PARAM(callFrame);
    return JSValue::encode(jsBoolean(false));
#endif
}

class CallerFrameJITTypeFunctor {
public:
    CallerFrameJITTypeFunctor()
    {
        DollarVMAssertScope assertScope;
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        unsigned index = m_currentFrame++;
        // First frame (index 0) is `llintTrue` etc. function itself.
        if (index == 1) {
            if (visitor->codeBlock())
                m_jitType = visitor->codeBlock()->jitType();
            return StackVisitor::Done;
        }
        return StackVisitor::Continue;
    }
    
    JITType jitType() { return m_jitType; }

private:
    mutable unsigned m_currentFrame { 0 };
    mutable JITType m_jitType { JITType::None };
};

static FunctionExecutable* getExecutableForFunction(JSValue theFunctionValue)
{
    DollarVMAssertScope assertScope;
    if (!theFunctionValue.isCell())
        return nullptr;
    
    VM& vm = theFunctionValue.asCell()->vm();
    JSFunction* theFunction = jsDynamicCast<JSFunction*>(vm, theFunctionValue);
    if (!theFunction)
        return nullptr;
    
    FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(vm,
        theFunction->executable());

    return executable;
}

// Returns true if the current frame is a LLInt frame.
// Usage: isLLInt = $vm.llintTrue()
JSC_DEFINE_HOST_FUNCTION(functionLLintTrue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    if (!callFrame)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    callFrame->iterate(vm, functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITType::InterpreterThunk));
}

// Returns true if the current frame is a baseline JIT frame.
// Usage: isBaselineJIT = $vm.baselineJITTrue()
JSC_DEFINE_HOST_FUNCTION(functionBaselineJITTrue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    if (!callFrame)
        return JSValue::encode(jsUndefined());
    CallerFrameJITTypeFunctor functor;
    callFrame->iterate(vm, functor);
    return JSValue::encode(jsBoolean(functor.jitType() == JITType::BaselineJIT));
}

// Set that the argument function should not be inlined.
// Usage:
// function f() { };
// $vm.noInline(f);
JSC_DEFINE_HOST_FUNCTION(functionNoInline, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    if (callFrame->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    
    JSValue theFunctionValue = callFrame->uncheckedArgument(0);

    if (FunctionExecutable* executable = getExecutableForFunction(theFunctionValue))
        executable->setNeverInline(true);
    
    return JSValue::encode(jsUndefined());
}

// Runs a full GC synchronously.
// Usage: $vm.gc()
JSC_DEFINE_HOST_FUNCTION(functionGC, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VMInspector::gc(&globalObject->vm());
    return JSValue::encode(jsUndefined());
}

// Runs the edenGC synchronously.
// Usage: $vm.edenGC()
JSC_DEFINE_HOST_FUNCTION(functionEdenGC, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VMInspector::edenGC(&globalObject->vm());
    return JSValue::encode(jsUndefined());
}

// Runs a full GC, but sweep asynchronously.
// Usage: $vm.gcSweepAsynchronously()
JSC_DEFINE_HOST_FUNCTION(functionGCSweepAsynchronously, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    globalObject->vm().heap.collectNow(Async, CollectionScope::Full);
    return JSValue::encode(jsUndefined());
}

// Dumps the hashes of all subspaces currently registered with the VM.
// Usage: $vm.dumpSubspaceHashes()
JSC_DEFINE_HOST_FUNCTION(functionDumpSubspaceHashes, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    VMInspector::dumpSubspaceHashes(&vm);
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
JSC_DEFINE_HOST_FUNCTION(functionCallFrame, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    unsigned frameNumber = 1;
    if (callFrame->argumentCount() >= 1) {
        JSValue value = callFrame->uncheckedArgument(0);
        if (!value.isUInt32())
            return JSValue::encode(jsUndefined());

        // We need to inc the frame number because the caller would consider
        // its own frame as frame 0. Hence, we need discount the frame for this
        // function.
        frameNumber = value.asUInt32() + 1;
    }

    return JSValue::encode(JSDollarVMCallFrame::create(globalObject, callFrame, frameNumber));
}

// Gets a token for the CodeBlock for a specified frame index.
// Usage: codeBlockToken = $vm.codeBlockForFrame(0) // frame 0 is the top frame.
// Usage: codeBlockToken = $vm.codeBlockForFrame() // implies frame 0 i.e. current frame.
JSC_DEFINE_HOST_FUNCTION(functionCodeBlockForFrame, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    unsigned frameNumber = 1;
    if (callFrame->argumentCount() >= 1) {
        JSValue value = callFrame->uncheckedArgument(0);
        if (!value.isUInt32())
            return JSValue::encode(jsUndefined());

        // We need to inc the frame number because the caller would consider
        // its own frame as frame 0. Hence, we need discount the frame for this
        // function.
        frameNumber = value.asUInt32() + 1;
    }

    CodeBlock* codeBlock = VMInspector::codeBlockForFrame(&globalObject->vm(), callFrame, frameNumber);
    if (codeBlock)
        return JSValue::encode(codeBlock);
    return JSValue::encode(jsUndefined());
}

static CodeBlock* codeBlockFromArg(JSGlobalObject* globalObject, CallFrame* callFrame)
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    if (callFrame->argumentCount() < 1)
        return nullptr;

    JSValue value = callFrame->uncheckedArgument(0);
    CodeBlock* candidateCodeBlock = nullptr;
    if (value.isCell()) {
        JSFunction* func = jsDynamicCast<JSFunction*>(vm, value.asCell());
        if (func) {
            if (func->isHostFunction())
                candidateCodeBlock = nullptr;
            else
                candidateCodeBlock = func->jsExecutable()->eitherCodeBlock();
        } else
            candidateCodeBlock = static_cast<CodeBlock*>(value.asCell());
    }

    if (candidateCodeBlock && VMInspector::isValidCodeBlock(&vm, candidateCodeBlock))
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
JSC_DEFINE_HOST_FUNCTION(functionCodeBlockFor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    CodeBlock* codeBlock = codeBlockFromArg(globalObject, callFrame);
    WTF::StringPrintStream stream;
    if (codeBlock) {
        stream.print(*codeBlock);
        return JSValue::encode(jsString(globalObject->vm(), stream.toString()));
    }
    return JSValue::encode(jsUndefined());
}

// Usage: $vm.dumpSourceFor(functionObj)
// Usage: $vm.dumpSourceFor(codeBlockToken)
JSC_DEFINE_HOST_FUNCTION(functionDumpSourceFor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    CodeBlock* codeBlock = codeBlockFromArg(globalObject, callFrame);
    if (codeBlock)
        codeBlock->dumpSource();
    return JSValue::encode(jsUndefined());
}

// Usage: $vm.dumpBytecodeFor(functionObj)
// Usage: $vm.dumpBytecodeFor(codeBlock)
JSC_DEFINE_HOST_FUNCTION(functionDumpBytecodeFor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    CodeBlock* codeBlock = codeBlockFromArg(globalObject, callFrame);
    if (codeBlock)
        codeBlock->dumpBytecode();
    return JSValue::encode(jsUndefined());
}

// Prints a series of comma separate strings without appending a newline.
// Usage: $vm.dataLog(str1, str2, str3)
JSC_DEFINE_HOST_FUNCTION(functionDataLog, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    const bool addLineFeed = false;
    return doPrint(globalObject, callFrame, addLineFeed);
}

// Prints a series of comma separate strings and appends a newline.
// Usage: $vm.print(str1, str2, str3)
JSC_DEFINE_HOST_FUNCTION(functionPrint, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    const bool addLineFeed = true;
    return doPrint(globalObject, callFrame, addLineFeed);
}

// Dumps the current CallFrame.
// Usage: $vm.dumpCallFrame()
JSC_DEFINE_HOST_FUNCTION(functionDumpCallFrame, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    // When the callers call this function, they are expecting to dump their
    // own frame. So skip 1 for this frame.
    VMInspector::dumpCallFrame(&globalObject->vm(), callFrame, 1);
    return JSValue::encode(jsUndefined());
}

// Dumps the JS stack.
// Usage: $vm.printStack()
JSC_DEFINE_HOST_FUNCTION(functionDumpStack, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    // When the callers call this function, they are expecting to dump the
    // stack starting their own frame. So skip 1 for this frame.
    VMInspector::dumpStack(&globalObject->vm(), callFrame, 1);
    return JSValue::encode(jsUndefined());
}

// Dumps the current CallFrame.
// Usage: $vm.dumpRegisters(N) // dump the registers of the Nth CallFrame.
// Usage: $vm.dumpRegisters() // dump the registers of the current CallFrame.
// FIXME: Currently, this function dumps the physical frame. We should make
// it dump the logical frame (i.e. be able to dump inlined frames as well).
JSC_DEFINE_HOST_FUNCTION(functionDumpRegisters, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    unsigned requestedFrameIndex = 1;
    if (callFrame->argumentCount() >= 1) {
        JSValue value = callFrame->uncheckedArgument(0);
        if (!value.isUInt32())
            return JSValue::encode(jsUndefined());

        // We need to inc the frame number because the caller would consider
        // its own frame as frame 0. Hence, we need discount the frame for this
        // function.
        requestedFrameIndex = value.asUInt32() + 1;
    }

    unsigned frameIndex = 0;
    callFrame->iterate(vm, [&] (StackVisitor& visitor) {
        DollarVMAssertScope assertScope;
        if (frameIndex++ != requestedFrameIndex)
            return StackVisitor::Continue;
        VMInspector::dumpRegisters(visitor->callFrame());
        return StackVisitor::Done;
    });

    return encodedJSUndefined();
}

// Dumps the internal memory layout of a JSCell.
// Usage: $vm.dumpCell(cell)
JSC_DEFINE_HOST_FUNCTION(functionDumpCell, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    JSValue value = callFrame->argument(0);
    if (!value.isCell())
        return encodedJSUndefined();
    
    VMInspector::dumpCellMemory(value.asCell());
    return encodedJSUndefined();
}

// Gets the dataLog dump of the indexingMode of the passed value.
// Usage: $vm.print("indexingMode = " + $vm.indexingMode(jsValue))
JSC_DEFINE_HOST_FUNCTION(functionIndexingMode, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    if (!callFrame->argument(0).isObject())
        return encodedJSUndefined();

    WTF::StringPrintStream stream;
    stream.print(IndexingTypeDump(callFrame->uncheckedArgument(0).getObject()->indexingMode()));
    return JSValue::encode(jsString(globalObject->vm(), stream.toString()));
}

JSC_DEFINE_HOST_FUNCTION(functionInlineCapacity, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    if (auto* object = jsDynamicCast<JSObject*>(vm, callFrame->argument(0)))
        return JSValue::encode(jsNumber(object->structure(vm)->inlineCapacity()));

    return encodedJSUndefined();
}

// Clears the LinkBuffer profile statistics.
// Usage: $vm.clearLinkBufferStats()
JSC_DEFINE_HOST_FUNCTION(functionClearLinkBufferStats, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if ENABLE(ASSEMBLER)
    LinkBuffer::clearProfileStatistics();
#endif
    return JSValue::encode(jsUndefined());
}

// Dumps the LinkBuffer profile statistics as a string.
// Usage: $vm.print($vm.linkBufferStats())
JSC_DEFINE_HOST_FUNCTION(functionLinkBufferStats, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if ENABLE(ASSEMBLER)
    WTF::StringPrintStream stream;
    LinkBuffer::dumpProfileStatistics(&stream);
    return JSValue::encode(jsString(globalObject->vm(), stream.toString()));
#else
    UNUSED_PARAM(globalObject);
    return JSValue::encode(jsUndefined());
#endif
}

// Gets the dataLog dump of a given JS value as a string.
// Usage: $vm.print("value = " + $vm.value(jsValue))
JSC_DEFINE_HOST_FUNCTION(functionValue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    WTF::StringPrintStream stream;
    for (unsigned i = 0; i < callFrame->argumentCount(); ++i) {
        if (i)
            stream.print(", ");
        stream.print(callFrame->uncheckedArgument(i));
    }
    
    return JSValue::encode(jsString(globalObject->vm(), stream.toString()));
}

// Gets the pid of the current process.
// Usage: $vm.print("pid = " + $vm.getpid())
JSC_DEFINE_HOST_FUNCTION(functionGetPID, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsNumber(getCurrentProcessID()));
}

// Make the globalObject have a bad time. Does nothing if the object is not a JSGlobalObject.
// Usage: $vm.haveABadTime(globalObject)
JSC_DEFINE_HOST_FUNCTION(functionHaveABadTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* target = globalObject;
    if (!callFrame->argument(0).isUndefined()) {
        JSObject* obj = callFrame->argument(0).getObject();
        if (!obj)
            return throwVMTypeError(globalObject, scope, "haveABadTime expects first argument to be an object if provided");
        target = obj->globalObject();
    }

    target->haveABadTime(vm);
    return JSValue::encode(jsBoolean(true));
}

// Checks if the object (or its global if the object is not a global) is having a bad time.
// Usage: $vm.isHavingABadTime(obj)
JSC_DEFINE_HOST_FUNCTION(functionIsHavingABadTime, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSGlobalObject* target = globalObject;
    if (!callFrame->argument(0).isUndefined()) {
        JSObject* obj = callFrame->argument(0).getObject();
        if (!obj)
            return throwVMTypeError(globalObject, scope, "isHavingABadTime expects first argument to be an object if provided");
        target = obj->globalObject();
    }

    return JSValue::encode(jsBoolean(target->isHavingABadTime()));
}

// Calls the specified test function after adjusting the stack to have the specified
// remaining size from the end of the physical stack.
// Usage: $vm.callWithStackSize(funcToCall, desiredStackSize)
//
// This function will only work in test configurations, specifically, only if JSC
// options are not frozen. For the jsc shell, the --disableOptionsFreezingForTesting
// argument needs to be passed in on the command line.

#if ENABLE(ASSEMBLER)
static void callWithStackSizeProbeFunction(Probe::State* state)
{
    JSGlobalObject* globalObject = bitwise_cast<JSGlobalObject*>(state->arg);
    // The bits loaded from state->probeFunction will be tagged like
    // a C function. So, we'll need to untag it to extract the bits
    // for the JSFunction*.
    JSFunction* function = bitwise_cast<JSFunction*>(untagCodePtr<CFunctionPtrTag>(state->probeFunction));
    state->initializeStackFunction = nullptr;
    state->initializeStackArg = nullptr;

    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    auto callData = getCallData(vm, function);
    MarkedArgumentBuffer args;
    call(globalObject, function, callData, jsUndefined(), args);
}
#endif // ENABLE(ASSEMBLER)

JSC_DEFINE_HOST_FUNCTION_WITH_ATTRIBUTES(functionCallWithStackSize, SUPPRESS_ASAN, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

#if OS(DARWIN) && CPU(X86_64)
    constexpr bool isSupportedByPlatform = true;
#else
    constexpr bool isSupportedByPlatform = false;
#endif

    if (!isSupportedByPlatform)
        return throwVMError(globalObject, throwScope, "Not supported for this platform");

#if ENABLE(ASSEMBLER)
    if (g_jscConfig.isPermanentlyFrozen() || !g_jscConfig.disabledFreezingForTesting)
        return throwVMError(globalObject, throwScope, "Options are frozen");

    if (callFrame->argumentCount() < 2)
        return throwVMError(globalObject, throwScope, "Invalid number of arguments");
    JSValue arg0 = callFrame->argument(0);
    JSValue arg1 = callFrame->argument(1);
    if (!arg0.isCallable(vm))
        return throwVMError(globalObject, throwScope, "arg0 should be a function");
    if (!arg1.isNumber())
        return throwVMError(globalObject, throwScope, "arg1 should be a number");

    JSFunction* function = jsCast<JSFunction*>(arg0);
    size_t desiredStackSize = arg1.asNumber();

    const StackBounds& bounds = Thread::current().stack();
    uint8_t* currentStackPosition = bitwise_cast<uint8_t*>(currentStackPointer());
    uint8_t* end = bitwise_cast<uint8_t*>(bounds.end());
    uint8_t* desiredStart = end + desiredStackSize;
    if (desiredStart >= currentStackPosition)
        return throwVMError(globalObject, throwScope, "Unable to setup desired stack size");

    JSDollarVMHelper helper(vm);

    unsigned originalMaxPerThreadStackUsage = Options::maxPerThreadStackUsage();
    void* originalVMSoftStackLimit = vm.softStackLimit();
    void* originalVMStackLimit = vm.stackLimit();

    // This is a hack to make the VM think it's stack limits are near the end
    // of the physical stack.
    uint8_t* vmStackStart = bitwise_cast<uint8_t*>(vm.stackPointerAtVMEntry());
    uint8_t* vmStackEnd = vmStackStart - originalMaxPerThreadStackUsage;
    ptrdiff_t sizeDiff = vmStackEnd - end;
    RELEASE_ASSERT(sizeDiff >= 0);
    RELEASE_ASSERT(static_cast<uint64_t>(sizeDiff) < UINT_MAX);

    Options::maxPerThreadStackUsage() = originalMaxPerThreadStackUsage + sizeDiff;
    helper.updateVMStackLimits();

#if OS(DARWIN) && CPU(X86_64)
    __asm__ volatile (
        "subq %[sizeDiff], %%rsp" "\n"
        "pushq %%rax" "\n"
        "pushq %%rcx" "\n"
        "pushq %%rdx" "\n"
        "pushq %%rbx" "\n"
        "callq *%%rax" "\n"
        "addq %[sizeDiff], %%rsp" "\n"
        :
        : "a" (ctiMasmProbeTrampoline)
        , "c" (callWithStackSizeProbeFunction)
        , "d" (function)
        , "b" (globalObject)
        , [sizeDiff] "rm" (sizeDiff)
        : "memory"
    );
#else
    UNUSED_PARAM(function);
#if !COMPILER(MSVC)
    UNUSED_PARAM(callWithStackSizeProbeFunction);
#endif
#endif // OS(DARWIN) && CPU(X86_64)

    Options::maxPerThreadStackUsage() = originalMaxPerThreadStackUsage;
    helper.updateVMStackLimits();
    RELEASE_ASSERT(vm.softStackLimit() == originalVMSoftStackLimit);
    RELEASE_ASSERT(vm.stackLimit() == originalVMStackLimit);

    throwScope.release();
    return encodedJSUndefined();

#else // not ENABLE(ASSEMBLER)
    UNUSED_PARAM(callFrame);
    return throwVMError(globalObject, throwScope, "Not supported for this platform");
#endif // ENABLE(ASSEMBLER)
}

// Creates a new global object.
// Usage: $vm.createGlobalObject()
JSC_DEFINE_HOST_FUNCTION(functionCreateGlobalObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(JSGlobalObject::create(vm, JSGlobalObject::createStructure(vm, jsNull())));
}

JSC_DEFINE_HOST_FUNCTION(functionCreateProxy, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    JSValue target = callFrame->argument(0);
    if (!target.isObject())
        return JSValue::encode(jsUndefined());
    JSObject* jsTarget = asObject(target.asCell());
    Structure* structure = JSProxy::createStructure(vm, globalObject, jsTarget->getPrototypeDirect(vm));
    JSProxy* proxy = JSProxy::create(vm, structure, jsTarget);
    return JSValue::encode(proxy);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateRuntimeArray, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    JSLockHolder lock(globalObject);
    RuntimeArray* array = RuntimeArray::create(globalObject, callFrame);
    return JSValue::encode(array);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateNullRopeString, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(JSRopeString::createNullForTesting(vm));
}

JSC_DEFINE_HOST_FUNCTION(functionCreateImpureGetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    JSValue target = callFrame->argument(0);
    JSObject* delegate = nullptr;
    if (target.isObject())
        delegate = asObject(target.asCell());
    Structure* structure = ImpureGetter::createStructure(vm, globalObject, jsNull());
    ImpureGetter* result = ImpureGetter::create(vm, structure, delegate);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateCustomGetterObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = CustomGetter::createStructure(vm, globalObject, jsNull());
    CustomGetter* result = CustomGetter::create(vm, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateDOMJITNodeObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITNode::createStructure(vm, globalObject, DOMJITGetter::create(vm, DOMJITGetter::createStructure(vm, globalObject, jsNull())));
    DOMJITNode* result = DOMJITNode::create(vm, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateDOMJITGetterObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITGetter::createStructure(vm, globalObject, jsNull());
    DOMJITGetter* result = DOMJITGetter::create(vm, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateDOMJITGetterNoEffectsObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITGetterNoEffects::createStructure(vm, globalObject, jsNull());
    DOMJITGetterNoEffects* result = DOMJITGetterNoEffects::create(vm, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateDOMJITGetterComplexObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITGetterComplex::createStructure(vm, globalObject, jsNull());
    DOMJITGetterComplex* result = DOMJITGetterComplex::create(vm, globalObject, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateDOMJITFunctionObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITFunctionObject::createStructure(vm, globalObject, jsNull());
    DOMJITFunctionObject* result = DOMJITFunctionObject::create(vm, globalObject, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateDOMJITCheckJSCastObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITCheckJSCastObject::createStructure(vm, globalObject, jsNull());
    DOMJITCheckJSCastObject* result = DOMJITCheckJSCastObject::create(vm, globalObject, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateDOMJITGetterBaseJSObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = DOMJITGetterBaseJSObject::createStructure(vm, globalObject, jsNull());
    DOMJITGetterBaseJSObject* result = DOMJITGetterBaseJSObject::create(vm, structure);
    return JSValue::encode(result);
}

#if ENABLE(WEBASSEMBLY)
JSC_DEFINE_HOST_FUNCTION(functionCreateWasmStreamingParser, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(WasmStreamingParser::create(vm, globalObject));
}

JSC_DEFINE_HOST_FUNCTION(functionCreateWasmStreamingCompilerForCompile, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto callback = jsDynamicCast<JSFunction*>(vm, callFrame->argument(0));
    if (!callback)
        return throwVMTypeError(globalObject, scope, "First argument is not a JS function"_s);

    auto compiler = WasmStreamingCompiler::create(vm, globalObject, Wasm::CompilerMode::Validation, nullptr);
    MarkedArgumentBuffer args;
    args.append(compiler);
    call(globalObject, callback, jsUndefined(), args, "You shouldn't see this...");
    if (UNLIKELY(scope.exception()))
        scope.clearException();
    compiler->streamingCompiler().finalize(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(compiler->promise());
}

JSC_DEFINE_HOST_FUNCTION(functionCreateWasmStreamingCompilerForInstantiate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto callback = jsDynamicCast<JSFunction*>(vm, callFrame->argument(0));
    if (!callback)
        return throwVMTypeError(globalObject, scope, "First argument is not a JS function"_s);

    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (UNLIKELY(!importArgument.isUndefined() && !importObject))
        return throwVMTypeError(globalObject, scope);

    auto compiler = WasmStreamingCompiler::create(vm, globalObject, Wasm::CompilerMode::FullCompile, importObject);
    MarkedArgumentBuffer args;
    args.append(compiler);
    call(globalObject, callback, jsUndefined(), args, "You shouldn't see this...");
    if (UNLIKELY(scope.exception()))
        scope.clearException();
    compiler->streamingCompiler().finalize(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(compiler->promise());
}
#endif

JSC_DEFINE_HOST_FUNCTION(functionCreateStaticCustomAccessor, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = StaticCustomAccessor::createStructure(vm, globalObject, jsNull());
    auto* result = StaticCustomAccessor::create(vm, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateStaticCustomValue, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = StaticCustomValue::createStructure(vm, globalObject, jsNull());
    auto* result = StaticCustomValue::create(vm, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateObjectDoingSideEffectPutWithoutCorrectSlotStatus, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);

    auto* dollarVM = jsDynamicCast<JSDollarVM*>(vm, callFrame->thisValue());
    RELEASE_ASSERT(dollarVM);
    auto* result = ObjectDoingSideEffectPutWithoutCorrectSlotStatus::create(vm, dollarVM->objectDoingSideEffectPutWithoutCorrectSlotStatusStructure());
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateEmptyFunctionWithName, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    const String name = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    RELEASE_AND_RETURN(scope, JSValue::encode(JSFunction::create(vm, globalObject, 1, name, functionCreateEmptyFunctionWithName)));
}

JSC_DEFINE_HOST_FUNCTION(functionSetImpureGetterDelegate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue base = callFrame->argument(0);
    if (!base.isObject())
        return JSValue::encode(jsUndefined());
    JSValue delegate = callFrame->argument(1);
    if (!delegate.isObject())
        return JSValue::encode(jsUndefined());
    ImpureGetter* impureGetter = jsDynamicCast<ImpureGetter*>(vm, asObject(base.asCell()));
    if (UNLIKELY(!impureGetter)) {
        throwTypeError(globalObject, scope, "argument is not an ImpureGetter"_s);
        return encodedJSValue();
    }
    impureGetter->setDelegate(vm, asObject(delegate.asCell()));
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionCreateBuiltin, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (callFrame->argumentCount() < 1 || !callFrame->argument(0).isString())
        return JSValue::encode(jsUndefined());

    String functionText = asString(callFrame->argument(0))->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    SourceCode source = makeSource(functionText, { });
    JSFunction* func = JSFunction::create(vm, createBuiltinExecutable(vm, source, Identifier::fromString(vm, "foo"), ConstructorKind::None, ConstructAbility::CannotConstruct)->link(vm, nullptr, source), globalObject);

    return JSValue::encode(func);
}

JSC_DEFINE_HOST_FUNCTION(functionGetPrivateProperty, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (callFrame->argumentCount() < 2 || !callFrame->argument(1).isString())
        return encodedJSUndefined();

    String str = asString(callFrame->argument(1))->value(globalObject);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    SymbolImpl* symbol = vm.propertyNames->builtinNames().lookUpPrivateName(str);
    if (!symbol)
        return throwVMError(globalObject, scope, "Unknown private name.");

    RELEASE_AND_RETURN(scope, JSValue::encode(callFrame->argument(0).get(globalObject, symbol)));
}

JSC_DEFINE_HOST_FUNCTION(functionCreateRoot, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(Root::create(vm, globalObject));
}

JSC_DEFINE_HOST_FUNCTION(functionCreateElement, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    Root* root = jsDynamicCast<Root*>(vm, callFrame->argument(0));
    if (!root)
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Cannot create Element without a Root."_s)));
    return JSValue::encode(Element::create(vm, globalObject, root));
}

JSC_DEFINE_HOST_FUNCTION(functionGetElement, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Root* root = jsDynamicCast<Root*>(vm, callFrame->argument(0));
    if (!root)
        return JSValue::encode(jsUndefined());
    Element* result = root->element();
    return JSValue::encode(result ? result : jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionCreateSimpleObject, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    return JSValue::encode(SimpleObject::create(vm, globalObject));
}

JSC_DEFINE_HOST_FUNCTION(functionGetHiddenValue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    SimpleObject* simpleObject = jsDynamicCast<SimpleObject*>(vm, callFrame->argument(0));
    if (UNLIKELY(!simpleObject)) {
        throwTypeError(globalObject, scope, "Invalid use of getHiddenValue test function"_s);
        return encodedJSValue();
    }
    return JSValue::encode(simpleObject->hiddenValue());
}

JSC_DEFINE_HOST_FUNCTION(functionSetHiddenValue, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);

    SimpleObject* simpleObject = jsDynamicCast<SimpleObject*>(vm, callFrame->argument(0));
    if (UNLIKELY(!simpleObject)) {
        throwTypeError(globalObject, scope, "Invalid use of setHiddenValue test function"_s);
        return encodedJSValue();
    }
    JSValue value = callFrame->argument(1);
    simpleObject->setHiddenValue(vm, value);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionShadowChickenFunctionsOnStack, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    DeferTermination deferScope(vm);
    auto scope = DECLARE_THROW_SCOPE(vm);
    if (auto* shadowChicken = vm.shadowChicken()) {
        scope.release();
        return JSValue::encode(shadowChicken->functionsOnStack(globalObject, callFrame));
    }

    JSArray* result = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });
    StackVisitor::visit(callFrame, vm, [&] (StackVisitor& visitor) -> StackVisitor::Status {
        DollarVMAssertScope assertScope;
        if (visitor->isInlinedFrame())
            return StackVisitor::Continue;
        if (visitor->isWasmFrame())
            return StackVisitor::Continue;
        result->push(globalObject, jsCast<JSObject*>(visitor->callee().asCell()));
        scope.releaseAssertNoException(); // This function is only called from tests.
        return StackVisitor::Continue;
    });
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionSetGlobalConstRedeclarationShouldNotThrow, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    vm.setGlobalConstRedeclarationShouldThrow(false);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionFindTypeForExpression, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    RELEASE_ASSERT(vm.typeProfiler());
    vm.typeProfilerLog()->processLogEntries(vm, "jsc Testing API: functionFindTypeForExpression"_s);

    JSValue functionValue = callFrame->argument(0);
    RELEASE_ASSERT(functionValue.isCallable(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(callFrame->argument(1).isString());
    String substring = asString(callFrame->argument(1))->value(globalObject);
    String sourceCodeText = executable->source().view().toString();
    unsigned offset = static_cast<unsigned>(sourceCodeText.find(substring) + executable->source().startOffset());
    
    String jsonString = vm.typeProfiler()->typeInformationForExpressionAtOffset(TypeProfilerSearchDescriptorNormal, offset, executable->sourceID(), vm);
    return JSValue::encode(JSONParse(globalObject, jsonString));
}

JSC_DEFINE_HOST_FUNCTION(functionReturnTypeFor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    RELEASE_ASSERT(vm.typeProfiler());
    vm.typeProfilerLog()->processLogEntries(vm, "jsc Testing API: functionReturnTypeFor"_s);

    JSValue functionValue = callFrame->argument(0);
    RELEASE_ASSERT(functionValue.isCallable(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    unsigned offset = executable->typeProfilingStartOffset(vm);
    String jsonString = vm.typeProfiler()->typeInformationForExpressionAtOffset(TypeProfilerSearchDescriptorFunctionReturn, offset, executable->sourceID(), vm);
    return JSValue::encode(JSONParse(globalObject, jsonString));
}

JSC_DEFINE_HOST_FUNCTION(functionFlattenDictionaryObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSValue value = callFrame->argument(0);
    RELEASE_ASSERT(value.isObject() && value.getObject()->structure()->isDictionary());
    value.getObject()->flattenDictionaryObject(vm);
    return encodedJSUndefined();
}

JSC_DEFINE_HOST_FUNCTION(functionDumpBasicBlockExecutionRanges, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    RELEASE_ASSERT(vm.controlFlowProfiler());
    vm.controlFlowProfiler()->dumpData();
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionHasBasicBlockExecuted, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    RELEASE_ASSERT(vm.controlFlowProfiler());

    JSValue functionValue = callFrame->argument(0);
    RELEASE_ASSERT(functionValue.isCallable(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(callFrame->argument(1).isString());
    String substring = asString(callFrame->argument(1))->value(globalObject);
    String sourceCodeText = executable->source().view().toString();
    RELEASE_ASSERT(sourceCodeText.contains(substring));
    int offset = sourceCodeText.find(substring) + executable->source().startOffset();
    
    bool hasExecuted = vm.controlFlowProfiler()->hasBasicBlockAtTextOffsetBeenExecuted(offset, executable->sourceID(), vm);
    return JSValue::encode(jsBoolean(hasExecuted));
}

JSC_DEFINE_HOST_FUNCTION(functionBasicBlockExecutionCount, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    RELEASE_ASSERT(vm.controlFlowProfiler());

    JSValue functionValue = callFrame->argument(0);
    RELEASE_ASSERT(functionValue.isCallable(vm));
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(vm, functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(callFrame->argument(1).isString());
    String substring = asString(callFrame->argument(1))->value(globalObject);
    String sourceCodeText = executable->source().view().toString();
    RELEASE_ASSERT(sourceCodeText.contains(substring));
    int offset = sourceCodeText.find(substring) + executable->source().startOffset();
    
    size_t executionCount = vm.controlFlowProfiler()->basicBlockExecutionCountAtTextOffset(offset, executable->sourceID(), vm);
    return JSValue::encode(JSValue(executionCount));
}

class DoNothingDebugger final : public Debugger {
    WTF_MAKE_NONCOPYABLE(DoNothingDebugger);
    WTF_MAKE_FAST_ALLOCATED;
public:
    DoNothingDebugger(VM& vm)
        : Debugger(vm)
    {
        DollarVMAssertScope assertScope;
        setSuppressAllPauses(true);
    }

private:
    void sourceParsed(JSGlobalObject*, SourceProvider*, int, const WTF::String&) final
    {
        DollarVMAssertScope assertScope;
    }
};

static EncodedJSValue changeDebuggerModeWhenIdle(JSGlobalObject* globalObject, OptionSet<CodeGenerationMode> codeGenerationMode)
{
    DollarVMAssertScope assertScope;

    bool debuggerRequested = codeGenerationMode.contains(CodeGenerationMode::Debugger);
    if (debuggerRequested == globalObject->hasDebugger())
        return JSValue::encode(jsUndefined());

    VM* vm = &globalObject->vm();
    vm->whenIdle([=] () {
        DollarVMAssertScope assertScope;
        if (debuggerRequested) {
            Debugger* debugger = new DoNothingDebugger(globalObject->vm());
            globalObject->setDebugger(debugger);
            debugger->activateBreakpoints(); // Also deletes all code.
        } else {
            Debugger* debugger = globalObject->debugger();
            debugger->deactivateBreakpoints(); // Also deletes all code.
            globalObject->setDebugger(nullptr);
            delete debugger;
        }
    });
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionEnableDebuggerModeWhenIdle, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return changeDebuggerModeWhenIdle(globalObject, { CodeGenerationMode::Debugger });
}

JSC_DEFINE_HOST_FUNCTION(functionDisableDebuggerModeWhenIdle, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return changeDebuggerModeWhenIdle(globalObject, { });
}

JSC_DEFINE_HOST_FUNCTION(functionDeleteAllCodeWhenIdle, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM* vm = &globalObject->vm();
    vm->whenIdle([=] () {
        DollarVMAssertScope assertScope;
        vm->deleteAllCode(PreventCollectionAndDeleteAllCode);
    });
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionGlobalObjectCount, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsNumber(globalObject->vm().heap.globalObjectCount()));
}

JSC_DEFINE_HOST_FUNCTION(functionGlobalObjectForObject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    JSValue value = callFrame->argument(0);
    RELEASE_ASSERT(value.isObject());
    JSGlobalObject* result = jsCast<JSObject*>(value)->globalObject(globalObject->vm());
    RELEASE_ASSERT(result);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionGetGetterSetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue value = callFrame->argument(0);
    if (!value.isObject())
        return JSValue::encode(jsUndefined());

    JSValue property = callFrame->argument(1);
    if (!property.isString())
        return JSValue::encode(jsUndefined());

    auto propertyName = asString(property)->toIdentifier(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    PropertySlot slot(value, PropertySlot::InternalMethodType::VMInquiry, &vm);
    value.getPropertySlot(globalObject, propertyName, slot);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue result;
    if (slot.isCacheableGetter())
        result = slot.getterSetter();
    else
        result = jsNull();

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionLoadGetterFromGetterSetter, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    GetterSetter* getterSetter = jsDynamicCast<GetterSetter*>(vm, callFrame->argument(0));
    if (UNLIKELY(!getterSetter)) {
        throwTypeError(globalObject, scope, "Invalid use of loadGetterFromGetterSetter test function: argument is not a GetterSetter"_s);
        return encodedJSValue();
    }

    JSObject* getter = getterSetter->getter();
    RELEASE_ASSERT(getter);
    return JSValue::encode(getter);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateCustomTestGetterSetter, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    return JSValue::encode(JSTestCustomGetterSetter::create(vm, globalObject, JSTestCustomGetterSetter::createStructure(vm, globalObject)));
}

JSC_DEFINE_HOST_FUNCTION(functionDeltaBetweenButterflies, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSObject* a = jsDynamicCast<JSObject*>(vm, callFrame->argument(0));
    JSObject* b = jsDynamicCast<JSObject*>(vm, callFrame->argument(1));
    if (!a || !b)
        return JSValue::encode(jsNumber(PNaN));

    ptrdiff_t delta = bitwise_cast<char*>(a->butterfly()) - bitwise_cast<char*>(b->butterfly());
    if (delta < 0)
        return JSValue::encode(jsNumber(PNaN));
    if (delta > std::numeric_limits<int32_t>::max())
        return JSValue::encode(jsNumber(PNaN));
    return JSValue::encode(jsNumber(static_cast<int32_t>(delta)));
}

JSC_DEFINE_HOST_FUNCTION(functionCurrentCPUTime, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsNumber(CPUTime::forCurrentThread().value()));
}

JSC_DEFINE_HOST_FUNCTION(functionTotalGCTime, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    return JSValue::encode(jsNumber(vm.heap.totalGCTime().seconds()));
}

JSC_DEFINE_HOST_FUNCTION(functionParseCount, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsNumber(globalParseCount.load()));
}

JSC_DEFINE_HOST_FUNCTION(functionIsWasmSupported, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if ENABLE(WEBASSEMBLY)
    return JSValue::encode(jsBoolean(Wasm::isSupported()));
#else
    return JSValue::encode(jsBoolean(false));
#endif
}

JSC_DEFINE_HOST_FUNCTION(functionMake16BitStringIfPossible, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    String string = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!string.is8Bit())
        return JSValue::encode(jsString(vm, WTFMove(string)));
    Vector<UChar> buffer;
    buffer.resize(string.length());
    StringImpl::copyCharacters(buffer.data(), string.characters8(), string.length());
    return JSValue::encode(jsString(vm, String::adopt(WTFMove(buffer))));
}

JSC_DEFINE_HOST_FUNCTION(functionGetStructureTransitionList, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* obj = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!obj)
        return JSValue::encode(jsNull());
    Vector<Structure*, 8> structures;

    for (auto* structure = obj->structure(); structure; structure = structure->previousID())
        structures.append(structure);

    JSArray* result = JSArray::tryCreate(vm, globalObject->arrayStructureForIndexingTypeDuringAllocation(ArrayWithContiguous), 0);
    RETURN_IF_EXCEPTION(scope, { });

    for (size_t i = 0; i < structures.size(); ++i) {
        auto* structure = structures[structures.size() - i - 1];
        result->push(globalObject, JSValue(structure->id()));
        RETURN_IF_EXCEPTION(scope, { });
        result->push(globalObject, JSValue(structure->transitionOffset()));
        RETURN_IF_EXCEPTION(scope, { });
        result->push(globalObject, JSValue(structure->maxOffset()));
        RETURN_IF_EXCEPTION(scope, { });
        if (structure->transitionPropertyName())
            result->push(globalObject, jsString(vm, String(*structure->transitionPropertyName())));
        else
            result->push(globalObject, jsNull());
        RETURN_IF_EXCEPTION(scope, { });
        result->push(globalObject, jsNumber(static_cast<int32_t>(structure->transitionKind())));
        RETURN_IF_EXCEPTION(scope, { });
    }

    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionGetConcurrently, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* obj = callFrame->argument(0).toObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    if (!obj)
        return JSValue::encode(jsNull());
    String property = callFrame->argument(1).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    auto name = PropertyName(Identifier::fromString(vm, property));
    auto offset = obj->structure()->getConcurrently(name.uid());
    if (offset != invalidOffset)
        ASSERT(JSValue::encode(obj->getDirect(offset)));
    JSValue result = JSValue(offset != invalidOffset);
    RETURN_IF_EXCEPTION(scope, { });
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionHasOwnLengthProperty, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    JSObject* target = asObject(callFrame->uncheckedArgument(0));
    JSFunction* function = jsDynamicCast<JSFunction*>(vm, target);
    return JSValue::encode(jsBoolean(function->canAssumeNameAndLengthAreOriginal(vm)));
}

JSC_DEFINE_HOST_FUNCTION(functionRejectPromiseAsHandled, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    JSPromise* promise = jsCast<JSPromise*>(callFrame->uncheckedArgument(0));
    JSValue reason = callFrame->uncheckedArgument(1);
    promise->rejectAsHandled(globalObject, reason);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionSetUserPreferredLanguages, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSArray* array = jsDynamicCast<JSArray*>(vm, callFrame->argument(0));
    if (!array)
        return throwVMTypeError(globalObject, scope, "Expected first argument to be an array"_s);

    Vector<String> languages;
    unsigned length = array->length();
    for (unsigned i = 0; i < length; i++) {
        String language = array->get(globalObject, i).toWTFString(globalObject);
        RETURN_IF_EXCEPTION(scope, encodedJSValue());
        languages.append(language);
    }

    overrideUserPreferredLanguages(languages);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionICUVersion, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsNumber(WTF::ICU::majorVersion()));
}

JSC_DEFINE_HOST_FUNCTION(functionICUHeaderVersion, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsNumber(U_ICU_VERSION_MAJOR_NUM));
}

JSC_DEFINE_HOST_FUNCTION(functionAssertEnabled, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsBoolean(ASSERT_ENABLED));
}

JSC_DEFINE_HOST_FUNCTION(functionSecurityAssertEnabled, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if ENABLE(SECURITY_ASSERTIONS)
    return JSValue::encode(jsBoolean(true));
#else
    return JSValue::encode(jsBoolean(false));
#endif
}

JSC_DEFINE_HOST_FUNCTION(functionAsanEnabled, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsBoolean(ASAN_ENABLED));
}

JSC_DEFINE_HOST_FUNCTION(functionIsMemoryLimited, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if PLATFORM(IOS) || PLATFORM(APPLETV) || PLATFORM(WATCHOS)
    return JSValue::encode(jsBoolean(true));
#else
    return JSValue::encode(jsBoolean(false));
#endif
}

JSC_DEFINE_HOST_FUNCTION(functionUseJIT, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsBoolean(Options::useJIT()));
}

JSC_DEFINE_HOST_FUNCTION(functionIsGigacageEnabled, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsBoolean(Gigacage::isEnabled()));
}

JSC_DEFINE_HOST_FUNCTION(functionToUncacheableDictionary, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = jsDynamicCast<JSObject*>(vm, callFrame->argument(0));
    if (!object)
        return throwVMTypeError(globalObject, scope, "Expected first argument to be an object"_s);
    object->convertToUncacheableDictionary(vm);
    return JSValue::encode(object);
}

JSC_DEFINE_HOST_FUNCTION(functionIsPrivateSymbol, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;

    if (!(callFrame->argument(0).isSymbol()))
        return JSValue::encode(jsBoolean(false));

    return JSValue::encode(jsBoolean(asSymbol(callFrame->argument(0))->uid().isPrivate()));
}

JSC_DEFINE_HOST_FUNCTION(functionDumpAndResetPasDebugSpectrum, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
#if !USE(SYSTEM_MALLOC)
#if BUSE(LIBPAS)
    pas_heap_lock_lock();
    pas_debug_spectrum_dump(&pas_log_stream.base);
    pas_debug_spectrum_reset();
    pas_heap_lock_unlock();
#endif
#endif
    return JSValue::encode(jsUndefined());
}

#if ENABLE(JIT)
JSC_DEFINE_HOST_FUNCTION(functionJITSizeStatistics, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;

    VM& vm = globalObject->vm();

    if (!vm.jitSizeStatistics)
        return JSValue::encode(jsUndefined());

    WTF::StringPrintStream stream;
    stream.print(*vm.jitSizeStatistics);
    return JSValue::encode(jsString(vm, stream.toString()));
}

JSC_DEFINE_HOST_FUNCTION(functionDumpJITSizeStatistics, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;

    VM& vm = globalObject->vm();

    if (!vm.jitSizeStatistics)
        return JSValue::encode(jsUndefined());

    dataLogLn(*vm.jitSizeStatistics);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(functionResetJITSizeStatistics, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;

    VM& vm = globalObject->vm();

    if (!vm.jitSizeStatistics)
        return JSValue::encode(jsUndefined());

    vm.jitSizeStatistics->reset();
    return JSValue::encode(jsUndefined());
}
#endif

constexpr unsigned jsDollarVMPropertyAttributes = PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete;

void JSDollarVM::finishCreation(VM& vm)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);

    JSGlobalObject* globalObject = this->globalObject(vm);

    auto addFunction = [&] (VM& vm, const char* name, NativeFunction function, unsigned arguments) {
        DollarVMAssertScope assertScope;
        JSDollarVM::addFunction(vm, globalObject, name, function, arguments);
    };
    auto addConstructibleFunction = [&] (VM& vm, const char* name, NativeFunction function, unsigned arguments) {
        DollarVMAssertScope assertScope;
        JSDollarVM::addConstructibleFunction(vm, globalObject, name, function, arguments);
    };

    addFunction(vm, "abort", functionCrash, 0);
    addFunction(vm, "crash", functionCrash, 0);
    addFunction(vm, "breakpoint", functionBreakpoint, 0);

    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "dfgTrue"), 0, functionDFGTrue, DFGTrueIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "ftlTrue"), 0, functionFTLTrue, FTLTrueIntrinsic, jsDollarVMPropertyAttributes);

    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuMfence"), 0, functionCpuMfence, CPUMfenceIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuRdtsc"), 0, functionCpuRdtsc, CPURdtscIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuCpuid"), 0, functionCpuCpuid, CPUCpuidIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuPause"), 0, functionCpuPause, CPUPauseIntrinsic, jsDollarVMPropertyAttributes);
    addFunction(vm, "cpuClflush", functionCpuClflush, 2);

    addFunction(vm, "llintTrue", functionLLintTrue, 0);
    addFunction(vm, "baselineJITTrue", functionBaselineJITTrue, 0);

    addFunction(vm, "noInline", functionNoInline, 1);

    addFunction(vm, "gc", functionGC, 0);
    addFunction(vm, "gcSweepAsynchronously", functionGCSweepAsynchronously, 0);
    addFunction(vm, "edenGC", functionEdenGC, 0);
    addFunction(vm, "dumpSubspaceHashes", functionDumpSubspaceHashes, 0);

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
    addFunction(vm, "clearLinkBufferStats", functionClearLinkBufferStats, 0);
    addFunction(vm, "linkBufferStats", functionLinkBufferStats, 0);
    addFunction(vm, "value", functionValue, 1);
    addFunction(vm, "getpid", functionGetPID, 0);

    addFunction(vm, "haveABadTime", functionHaveABadTime, 1);
    addFunction(vm, "isHavingABadTime", functionIsHavingABadTime, 1);

    addFunction(vm, "callWithStackSize", functionCallWithStackSize, 2);

    addFunction(vm, "createGlobalObject", functionCreateGlobalObject, 0);
    addFunction(vm, "createProxy", functionCreateProxy, 1);
    addFunction(vm, "createRuntimeArray", functionCreateRuntimeArray, 0);
    addFunction(vm, "createNullRopeString", functionCreateNullRopeString, 0);

    addFunction(vm, "createImpureGetter", functionCreateImpureGetter, 1);
    addFunction(vm, "createCustomGetterObject", functionCreateCustomGetterObject, 0);
    addFunction(vm, "createDOMJITNodeObject", functionCreateDOMJITNodeObject, 0);
    addFunction(vm, "createDOMJITGetterObject", functionCreateDOMJITGetterObject, 0);
    addFunction(vm, "createDOMJITGetterNoEffectsObject", functionCreateDOMJITGetterNoEffectsObject, 0);
    addFunction(vm, "createDOMJITGetterComplexObject", functionCreateDOMJITGetterComplexObject, 0);
    addFunction(vm, "createDOMJITFunctionObject", functionCreateDOMJITFunctionObject, 0);
    addFunction(vm, "createDOMJITCheckJSCastObject", functionCreateDOMJITCheckJSCastObject, 0);
    addFunction(vm, "createDOMJITGetterBaseJSObject", functionCreateDOMJITGetterBaseJSObject, 0);
    addFunction(vm, "createBuiltin", functionCreateBuiltin, 2);
#if ENABLE(WEBASSEMBLY)
    addFunction(vm, "createWasmStreamingParser", functionCreateWasmStreamingParser, 0);
    addFunction(vm, "createWasmStreamingCompilerForCompile", functionCreateWasmStreamingCompilerForCompile, 0);
    addFunction(vm, "createWasmStreamingCompilerForInstantiate", functionCreateWasmStreamingCompilerForInstantiate, 0);
#endif
    addFunction(vm, "createStaticCustomAccessor", functionCreateStaticCustomAccessor, 0);
    addFunction(vm, "createStaticCustomValue", functionCreateStaticCustomValue, 0);
    addFunction(vm, "createObjectDoingSideEffectPutWithoutCorrectSlotStatus", functionCreateObjectDoingSideEffectPutWithoutCorrectSlotStatus, 0);
    addFunction(vm, "createEmptyFunctionWithName", functionCreateEmptyFunctionWithName, 1);
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

    addFunction(vm, "enableDebuggerModeWhenIdle", functionEnableDebuggerModeWhenIdle, 0);
    addFunction(vm, "disableDebuggerModeWhenIdle", functionDisableDebuggerModeWhenIdle, 0);

    addFunction(vm, "deleteAllCodeWhenIdle", functionDeleteAllCodeWhenIdle, 0);

    addFunction(vm, "globalObjectCount", functionGlobalObjectCount, 0);
    addFunction(vm, "globalObjectForObject", functionGlobalObjectForObject, 1);

    addFunction(vm, "getGetterSetter", functionGetGetterSetter, 2);
    addFunction(vm, "loadGetterFromGetterSetter", functionLoadGetterFromGetterSetter, 1);
    addFunction(vm, "createCustomTestGetterSetter", functionCreateCustomTestGetterSetter, 1);

    addFunction(vm, "deltaBetweenButterflies", functionDeltaBetweenButterflies, 2);
    
    addFunction(vm, "currentCPUTime", functionCurrentCPUTime, 0);
    addFunction(vm, "totalGCTime", functionTotalGCTime, 0);

    addFunction(vm, "parseCount", functionParseCount, 0);

    addFunction(vm, "isWasmSupported", functionIsWasmSupported, 0);
    addFunction(vm, "make16BitStringIfPossible", functionMake16BitStringIfPossible, 1);

    addFunction(vm, "getStructureTransitionList", functionGetStructureTransitionList, 1);
    addFunction(vm, "getConcurrently", functionGetConcurrently, 2);

    addFunction(vm, "hasOwnLengthProperty", functionHasOwnLengthProperty, 1);
    addFunction(vm, "rejectPromiseAsHandled", functionRejectPromiseAsHandled, 1);

    addFunction(vm, "setUserPreferredLanguages", functionSetUserPreferredLanguages, 1);
    addFunction(vm, "icuVersion", functionICUVersion, 0);
    addFunction(vm, "icuHeaderVersion", functionICUHeaderVersion, 0);

    addFunction(vm, "assertEnabled", functionAssertEnabled, 0);
    addFunction(vm, "securityAssertEnabled", functionSecurityAssertEnabled, 0);
    addFunction(vm, "asanEnabled", functionAsanEnabled, 0);

    addFunction(vm, "isMemoryLimited", functionIsMemoryLimited, 0);
    addFunction(vm, "useJIT", functionUseJIT, 0);
    addFunction(vm, "isGigacageEnabled", functionIsGigacageEnabled, 0);

    addFunction(vm, "toUncacheableDictionary", functionToUncacheableDictionary, 1);

    addFunction(vm, "isPrivateSymbol", functionIsPrivateSymbol, 1);
    addFunction(vm, "dumpAndResetPasDebugSpectrum", functionDumpAndResetPasDebugSpectrum, 0);

#if ENABLE(JIT)
    addFunction(vm, "jitSizeStatistics", functionJITSizeStatistics, 0);
    addFunction(vm, "dumpJITSizeStatistics", functionDumpJITSizeStatistics, 0);
    addFunction(vm, "resetJITSizeStatistics", functionResetJITSizeStatistics, 0);
#endif

    m_objectDoingSideEffectPutWithoutCorrectSlotStatusStructure.set(vm, this, ObjectDoingSideEffectPutWithoutCorrectSlotStatus::createStructure(vm, globalObject, jsNull()));
}

void JSDollarVM::addFunction(VM& vm, JSGlobalObject* globalObject, const char* name, NativeFunction function, unsigned arguments)
{
    DollarVMAssertScope assertScope;
    Identifier identifier = Identifier::fromString(vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function), jsDollarVMPropertyAttributes);
}

void JSDollarVM::addConstructibleFunction(VM& vm, JSGlobalObject* globalObject, const char* name, NativeFunction function, unsigned arguments)
{
    DollarVMAssertScope assertScope;
    Identifier identifier = Identifier::fromString(vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function, NoIntrinsic, function), jsDollarVMPropertyAttributes);
}

template<typename Visitor>
void JSDollarVM::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSDollarVM* thisObject = jsCast<JSDollarVM*>(cell);
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_objectDoingSideEffectPutWithoutCorrectSlotStatusStructure);
}

DEFINE_VISIT_CHILDREN(JSDollarVM);

} // namespace JSC

IGNORE_WARNINGS_END
