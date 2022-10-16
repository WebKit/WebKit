/*
 * Copyright (C) 2015-2022 Apple Inc. All rights reserved.
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
#include <wtf/ApproximateTime.h>
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

#if PLATFORM(COCOA)
#include <wtf/cocoa/CrashReporter.h>
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
        return &vm.cellSpace();
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
        JSDollarVMCallFrame* frame = new (NotNull, allocateCell<JSDollarVMCallFrame>(vm)) JSDollarVMCallFrame(vm, structure);
        frame->finishCreation(vm, callFrame, requestedFrameIndex);
        return frame;
    }

    void finishCreation(VM& vm, CallFrame* callFrame, unsigned requestedFrameIndex)
    {
        DollarVMAssertScope assertScope;
        Base::finishCreation(vm);

        auto addProperty = [&] (VM& vm, ASCIILiteral name, JSValue value) {
            DollarVMAssertScope assertScope;
            JSDollarVMCallFrame::addProperty(vm, name, value);
        };

        unsigned frameIndex = 0;
        bool isValid = false;
        callFrame->iterate(vm, [&] (StackVisitor& visitor) {
            DollarVMAssertScope assertScope;

            if (frameIndex++ != requestedFrameIndex)
                return IterationStatus::Continue;

            addProperty(vm, "name"_s, jsString(vm, visitor->functionName()));

            if (visitor->callee().isCell())
                addProperty(vm, "callee"_s, visitor->callee().asCell());

            CodeBlock* codeBlock = visitor->codeBlock();
            if (codeBlock) {
                addProperty(vm, "codeBlock"_s, codeBlock);
                addProperty(vm, "unlinkedCodeBlock"_s, codeBlock->unlinkedCodeBlock());
                addProperty(vm, "executable"_s, codeBlock->ownerExecutable());
            }
            isValid = true;

            return IterationStatus::Done;
        });

        addProperty(vm, "valid"_s, jsBoolean(isValid));
    }

    DECLARE_INFO;

private:
    void addProperty(VM& vm, ASCIILiteral name, JSValue value)
    {
        DollarVMAssertScope assertScope;
        Identifier identifier = Identifier::fromString(vm, name);
        putDirect(vm, identifier, value);
    }
};

const ClassInfo JSDollarVMCallFrame::s_info = { "CallFrame"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDollarVMCallFrame) };

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
        return &vm.cellSpace();
    }

    Root* root() const { return m_root.get(); }
    void setRoot(VM& vm, Root* root) { m_root.set(vm, this, root); }

    static Element* create(VM& vm, JSGlobalObject* globalObject, Root* root)
    {
        DollarVMAssertScope assertScope;
        Structure* structure = createStructure(vm, globalObject, jsNull());
        Element* element = new (NotNull, allocateCell<Element>(vm)) Element(vm, structure);
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
        return &vm.destructibleObjectSpace();
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
        Root* root = new (NotNull, allocateCell<Root>(vm)) Root(vm, structure);
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
        return &vm.cellSpace();
    }

    static SimpleObject* create(VM& vm, JSGlobalObject* globalObject)
    {
        DollarVMAssertScope assertScope;
        Structure* structure = createStructure(vm, globalObject, jsNull());
        SimpleObject* simpleObject = new (NotNull, allocateCell<SimpleObject>(vm)) SimpleObject(vm, structure);
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
        return &vm.cellSpace();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static ImpureGetter* create(VM& vm, Structure* structure, JSObject* delegate)
    {
        DollarVMAssertScope assertScope;
        ImpureGetter* getter = new (NotNull, allocateCell<ImpureGetter>(vm)) ImpureGetter(vm, structure);
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
        return &vm.cellSpace();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static CustomGetter* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        CustomGetter* getter = new (NotNull, allocateCell<CustomGetter>(vm)) CustomGetter(vm, structure);
        getter->finishCreation(vm);
        return getter;
    }

    static bool getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        CustomGetter* thisObject = jsCast<CustomGetter*>(object);
        if (propertyName == PropertyName(Identifier::fromString(vm, "customGetter"_s))) {
            slot.setCacheableCustom(thisObject, PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, customGetterValueGetter);
            return true;
        }
        
        if (propertyName == PropertyName(Identifier::fromString(vm, "customGetterAccessor"_s))) {
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

    CustomGetter* thisObject = jsDynamicCast<CustomGetter*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    bool shouldThrow = thisObject->get(globalObject, PropertyName(Identifier::fromString(vm, "shouldThrow"_s))).toBoolean(globalObject);
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

    JSObject* thisObject = jsDynamicCast<JSObject*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    bool shouldThrow = thisObject->get(globalObject, PropertyName(Identifier::fromString(vm, "shouldThrow"_s))).toBoolean(globalObject);
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
        return &vm.cellSpace();
    }

    static RuntimeArray* create(JSGlobalObject* globalObject, CallFrame* callFrame)
    {
        DollarVMAssertScope assertScope;
        VM& vm = globalObject->vm();
        Structure* structure = createStructure(vm, globalObject, createPrototype(vm, globalObject));
        RuntimeArray* runtimeArray = new (NotNull, allocateCell<RuntimeArray>(vm)) RuntimeArray(globalObject, structure);
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
        ASSERT(inherits(info()));

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

    RuntimeArray* thisObject = jsDynamicCast<RuntimeArray*>(JSValue::decode(thisValue));
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
    
    JSObject* thisObject = jsDynamicCast<JSObject*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);

    if (JSValue result = thisObject->getDirect(vm, PropertyName(Identifier::fromString(vm, "testField"_s))))
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

    return thisObject->putDirect(vm, PropertyName(Identifier::fromString(vm, "testField"_s)), JSValue::decode(value));
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
    { "testStaticAccessor"_s, static_cast<unsigned>(PropertyAttribute::CustomAccessor), NoIntrinsic, { HashTableValue::GetterSetterType, testStaticAccessorGetter, testStaticAccessorPutter } },
    { "testStaticAccessorDontEnum"_s, PropertyAttribute::CustomAccessor | PropertyAttribute::DontEnum, NoIntrinsic, { HashTableValue::GetterSetterType, testStaticAccessorGetter, testStaticAccessorPutter } },
    { "testStaticAccessorReadOnly"_s, PropertyAttribute::CustomAccessor | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, NoIntrinsic, { HashTableValue::GetterSetterType, testStaticAccessorGetter, nullptr } },
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
        return &vm.cellSpace();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static StaticCustomAccessor* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        StaticCustomAccessor* accessor = new (NotNull, allocateCell<StaticCustomAccessor>(vm)) StaticCustomAccessor(vm, structure);
        accessor->finishCreation(vm);
        return accessor;
    }

    static bool getOwnPropertySlot(JSObject* thisObject, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
    {
        if (String(propertyName.uid()) == "thinAirCustomGetter"_s) {
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
    
    JSObject* thisObject = jsDynamicCast<JSObject*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);

    return thisObject->putDirect(vm, PropertyName(Identifier::fromString(vm, "testStaticValue"_s)), JSValue::decode(value));
}

JSC_DEFINE_CUSTOM_SETTER(testStaticValuePutterSetFlag, (JSGlobalObject* globalObject, EncodedJSValue thisValue, EncodedJSValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* thisObject = jsDynamicCast<JSObject*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);

    return thisObject->putDirect(vm, PropertyName(Identifier::fromString(vm, "testStaticValueSetterCalled"_s)), jsBoolean(true));
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
    { "testStaticValue"_s, static_cast<unsigned>(PropertyAttribute::CustomValue), NoIntrinsic, { HashTableValue::GetterSetterType, testStaticValueGetter, testStaticValuePutter } },
    { "testStaticValueNoSetter"_s, PropertyAttribute::CustomValue | PropertyAttribute::DontEnum, NoIntrinsic, { HashTableValue::GetterSetterType, testStaticValueGetter, nullptr } },
    { "testStaticValueReadOnly"_s, PropertyAttribute::CustomValue | PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly, NoIntrinsic, { HashTableValue::GetterSetterType, testStaticValueGetter, nullptr } },
    { "testStaticValueSetFlag"_s, static_cast<unsigned>(PropertyAttribute::CustomValue), NoIntrinsic, { HashTableValue::GetterSetterType, testStaticValueGetter, testStaticValuePutterSetFlag } },
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
        return &vm.cellSpace();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static StaticCustomValue* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        StaticCustomValue* accessor = new (NotNull, allocateCell<StaticCustomValue>(vm)) StaticCustomValue(vm, structure);
        accessor->finishCreation(vm);
        return accessor;
    }
};

static JSC_DECLARE_HOST_FUNCTION(staticDontDeleteDontEnumMethod);

class StaticDontDeleteDontEnum : public JSNonFinalObject {
public:
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | HasStaticPropertyTable;

    StaticDontDeleteDontEnum(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    DECLARE_INFO;

    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        DollarVMAssertScope assertScope;
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    static StaticDontDeleteDontEnum* create(VM& vm, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        StaticDontDeleteDontEnum* result = new (NotNull, allocateCell<StaticDontDeleteDontEnum>(vm)) StaticDontDeleteDontEnum(vm, structure);
        result->finishCreation(vm);
        return result;
    }
};

JSC_DEFINE_HOST_FUNCTION(staticDontDeleteDontEnumMethod, (JSGlobalObject*, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return encodedJSUndefined();
}

static const struct CompactHashIndex staticDontDeleteDontEnumTableIndex[8] = {
    { 0, -1 },
    { 1, -1 },
    { -1, -1 },
    { -1, -1 },
    { -1, -1 },
    { 2, -1 },
    { -1, -1 },
    { -1, -1 },
};

static const struct HashTableValue staticDontDeleteDontEnumTableValues[3] = {
    { "dontEnum"_s, static_cast<unsigned>(PropertyAttribute::Function | PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::NativeFunctionType, staticDontDeleteDontEnumMethod, 0 } },
    { "dontDelete"_s, static_cast<unsigned>(PropertyAttribute::Function | PropertyAttribute::DontDelete), NoIntrinsic, { HashTableValue::NativeFunctionType, staticDontDeleteDontEnumMethod, 0 } },
    { "dontDeleteDontEnum"_s, static_cast<unsigned>(PropertyAttribute::Function | PropertyAttribute::DontDelete | PropertyAttribute::DontEnum), NoIntrinsic, { HashTableValue::NativeFunctionType, staticDontDeleteDontEnumMethod, 0 } },
};

static const struct HashTable staticDontDeleteDontEnumTable =
    { 3, 7, false, nullptr, staticDontDeleteDontEnumTableValues, staticDontDeleteDontEnumTableIndex };

class ObjectDoingSideEffectPutWithoutCorrectSlotStatus : public JSNonFinalObject {
    using Base = JSNonFinalObject;
    static constexpr unsigned StructureFlags = Base::StructureFlags | OverridesPut;
public:
    template<typename CellType, SubspaceAccess>
    static CompleteSubspace* subspaceFor(VM& vm)
    {
        return &vm.cellSpace();
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
        ObjectDoingSideEffectPutWithoutCorrectSlotStatus* accessor = new (NotNull, allocateCell<ObjectDoingSideEffectPutWithoutCorrectSlotStatus>(vm)) ObjectDoingSideEffectPutWithoutCorrectSlotStatus(vm, structure);
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
        return &vm.cellSpace();
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
        DOMJITNode* getter = new (NotNull, allocateCell<DOMJITNode>(vm)) DOMJITNode(vm, structure);
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
extern "C" { static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterSlowCall, EncodedJSValue, (JSGlobalObject*, void*)); }

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
        DOMJITGetter* getter = new (NotNull, allocateCell<DOMJITGetter>(vm)) DOMJITGetter(vm, structure);
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
        putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"_s), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
    }
    {
        auto* customGetterSetter = DOMAttributeGetterSetter::create(vm, domJITGetterCustomGetter, nullptr, DOMAttributeAnnotation { DOMJITNode::info(), nullptr });
        putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter2"_s), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
    }
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(JSValue::decode(thisValue));
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
extern "C" { static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterNoEffectSlowCall, EncodedJSValue, (JSGlobalObject*, void*)); }

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
        DOMJITGetterNoEffects* getter = new (NotNull, allocateCell<DOMJITGetterNoEffects>(vm)) DOMJITGetterNoEffects(vm, structure);
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
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"_s), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterNoEffectCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(JSValue::decode(thisValue));
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
extern "C" { static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterComplexSlowCall, EncodedJSValue, (JSGlobalObject*, void*)); }

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
        DOMJITGetterComplex* getter = new (NotNull, allocateCell<DOMJITGetterComplex>(vm)) DOMJITGetterComplex(vm, structure);
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

JSC_DEFINE_HOST_FUNCTION(functionDOMJITGetterComplexEnableException, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    auto* object = jsDynamicCast<DOMJITGetterComplex*>(callFrame->thisValue());
    if (object)
        object->m_enableException = true;
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterComplexCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsDynamicCast<DOMJITGetterComplex*>(JSValue::decode(thisValue));
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
    auto* domjitGetterComplex = jsDynamicCast<DOMJITGetterComplex*>(object);
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
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"_s), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "enableException"_s), 0, functionDOMJITGetterComplexEnableException, ImplementationVisibility::Public, NoIntrinsic, 0);
}

extern "C" { static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionDOMJITFunctionObjectWithoutTypeCheck, EncodedJSValue, (JSGlobalObject* globalObject, DOMJITNode*)); }

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
        DOMJITFunctionObject* object = new (NotNull, allocateCell<DOMJITFunctionObject>(vm)) DOMJITFunctionObject(vm, structure);
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

    DOMJITNode* thisObject = jsDynamicCast<DOMJITNode*>(callFrame->thisValue());
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
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "func"_s), 0, functionDOMJITFunctionObjectWithTypeCheck, ImplementationVisibility::Public, NoIntrinsic, &DOMJITFunctionObjectSignature, static_cast<unsigned>(PropertyAttribute::ReadOnly));
}

extern "C" { static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(functionDOMJITCheckJSCastObjectWithoutTypeCheck, EncodedJSValue, (JSGlobalObject* globalObject, DOMJITNode* node)); }

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
        DOMJITCheckJSCastObject* object = new (NotNull, allocateCell<DOMJITCheckJSCastObject>(vm)) DOMJITCheckJSCastObject(vm, structure);
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

    auto* thisObject = jsDynamicCast<DOMJITCheckJSCastObject*>(callFrame->thisValue());
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
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "func"_s), 0, functionDOMJITCheckJSCastObjectWithTypeCheck, ImplementationVisibility::Public, NoIntrinsic, &DOMJITCheckJSCastObjectSignature, static_cast<unsigned>(PropertyAttribute::ReadOnly));
}

static JSC_DECLARE_CUSTOM_GETTER(domJITGetterBaseJSObjectCustomGetter);
extern "C" { static JSC_DECLARE_JIT_OPERATION_WITHOUT_WTF_INTERNAL(domJITGetterBaseJSObjectSlowCall, EncodedJSValue, (JSGlobalObject*, void*)); }

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
        DOMJITGetterBaseJSObject* getter = new (NotNull, allocateCell<DOMJITGetterBaseJSObject>(vm)) DOMJITGetterBaseJSObject(vm, structure);
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
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customGetter"_s), customGetterSetter, PropertyAttribute::ReadOnly | PropertyAttribute::CustomAccessor);
}

JSC_DEFINE_CUSTOM_GETTER(domJITGetterBaseJSObjectCustomGetter, (JSGlobalObject* globalObject, EncodedJSValue thisValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* thisObject = jsDynamicCast<JSObject*>(JSValue::decode(thisValue));
    if (!thisObject)
        return throwVMTypeError(globalObject, scope);
    return JSValue::encode(thisObject->getPrototypeDirect());
}

JSC_DEFINE_JIT_OPERATION(domJITGetterBaseJSObjectSlowCall, EncodedJSValue, (JSGlobalObject* globalObject, void* pointer))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    CallFrame* callFrame = DECLARE_CALL_FRAME(vm);
    JITOperationPrologueCallFrameTracer tracer(vm, callFrame);
    JSObject* object = static_cast<JSObject*>(pointer);
    return JSValue::encode(object->getPrototypeDirect());
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
        return &vm.cellSpace();
    }

    JSTestCustomGetterSetter(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
        DollarVMAssertScope assertScope;
    }

    static JSTestCustomGetterSetter* create(VM& vm, JSGlobalObject*, Structure* structure)
    {
        DollarVMAssertScope assertScope;
        JSTestCustomGetterSetter* result = new (NotNull, allocateCell<JSTestCustomGetterSetter>(vm)) JSTestCustomGetterSetter(vm, structure);
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

JSC_DEFINE_CUSTOM_GETTER(customGetValue, (JSGlobalObject*, EncodedJSValue slotValue, PropertyName))
{
    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>());
    // Passed property holder.
    return slotValue;
}

JSC_DEFINE_CUSTOM_GETTER(customGetValue2, (JSGlobalObject* globalObject, EncodedJSValue slotValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>());

    auto* target = jsCast<JSTestCustomGetterSetter*>(JSValue::decode(slotValue));
    JSValue value = target->getDirect(vm, Identifier::fromString(vm, "value2"_s));
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
    object->put(object, globalObject, Identifier::fromString(vm, "result"_s), JSValue::decode(thisObject), slot);

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
    object->put(object, globalObject, Identifier::fromString(vm, "result"_s), globalObject, slot);

    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customSetValue, (JSGlobalObject* globalObject, EncodedJSValue slotValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>());

    JSValue value = JSValue::decode(encodedValue);
    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, globalObject, Identifier::fromString(vm, "result"_s), JSValue::decode(slotValue), slot);

    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customSetValueGlobalObject, (JSGlobalObject* globalObject, EncodedJSValue slotValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>());

    JSValue value = JSValue::decode(encodedValue);
    if (!value.isObject())
        return false;

    JSObject* object = asObject(value);
    PutPropertySlot slot(object);
    object->put(object, globalObject, Identifier::fromString(vm, "result"_s), globalObject, slot);

    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customSetValue2, (JSGlobalObject* globalObject, EncodedJSValue slotValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();

    RELEASE_ASSERT(JSValue::decode(slotValue).inherits<JSTestCustomGetterSetter>());
    auto* target = jsCast<JSTestCustomGetterSetter*>(JSValue::decode(slotValue));
    PutPropertySlot slot(target);
    target->putDirect(vm, Identifier::fromString(vm, "value2"_s), JSValue::decode(encodedValue));
    return true;
}

JSC_DEFINE_CUSTOM_SETTER(customFunctionSetter, (JSGlobalObject* globalObject, EncodedJSValue, EncodedJSValue encodedValue, PropertyName))
{
    DollarVMAssertScope assertScope;

    JSValue value = JSValue::decode(encodedValue);
    JSFunction* function = jsDynamicCast<JSFunction*>(value);
    if (!function)
        return false;

    auto callData = JSC::getCallData(function);
    MarkedArgumentBuffer args;
    call(globalObject, function, callData, jsUndefined(), args);

    return true;
}

void JSTestCustomGetterSetter::finishCreation(VM& vm)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);

    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValue"_s),
        CustomGetterSetter::create(vm, customGetValue, customSetValue), 0);
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValue2"_s),
        CustomGetterSetter::create(vm, customGetValue2, customSetValue2), static_cast<unsigned>(PropertyAttribute::CustomValue));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customAccessor"_s),
        CustomGetterSetter::create(vm, customGetAccessor, customSetAccessor), static_cast<unsigned>(PropertyAttribute::CustomAccessor));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValueGlobalObject"_s),
        CustomGetterSetter::create(vm, customGetValueGlobalObject, customSetValueGlobalObject), static_cast<unsigned>(PropertyAttribute::CustomValue));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customAccessorGlobalObject"_s),
        CustomGetterSetter::create(vm, customGetAccessorGlobalObject, customSetAccessorGlobalObject), static_cast<unsigned>(PropertyAttribute::CustomAccessor));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customValueNoSetter"_s),
        CustomGetterSetter::create(vm, customGetValue, nullptr), static_cast<unsigned>(PropertyAttribute::CustomValue));
    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customAccessorReadOnly"_s),
        CustomGetterSetter::create(vm, customGetAccessor, nullptr), PropertyAttribute::CustomAccessor | PropertyAttribute::ReadOnly);

    putDirectCustomAccessor(vm, Identifier::fromString(vm, "customFunction"_s),
        CustomGetterSetter::create(vm, customGetAccessor, customFunctionSetter), static_cast<unsigned>(PropertyAttribute::CustomAccessor));

}

const ClassInfo Element::s_info = { "Element"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(Element) };
const ClassInfo Root::s_info = { "Root"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(Root) };
const ClassInfo SimpleObject::s_info = { "SimpleObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(SimpleObject) };
const ClassInfo ImpureGetter::s_info = { "ImpureGetter"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(ImpureGetter) };
const ClassInfo CustomGetter::s_info = { "CustomGetter"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(CustomGetter) };
const ClassInfo RuntimeArray::s_info = { "RuntimeArray"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(RuntimeArray) };
#if ENABLE(JIT)
const ClassInfo DOMJITNode::s_info = { "DOMJITNode"_s, &Base::s_info, nullptr, &DOMJITNode::checkSubClassSnippet, CREATE_METHOD_TABLE(DOMJITNode) };
#else
const ClassInfo DOMJITNode::s_info = { "DOMJITNode"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITNode) };
#endif
const ClassInfo DOMJITGetter::s_info = { "DOMJITGetter"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetter) };
const ClassInfo DOMJITGetterNoEffects::s_info = { "DOMJITGetterNoEffects"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterNoEffects) };
const ClassInfo DOMJITGetterComplex::s_info = { "DOMJITGetterComplex"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterComplex) };
const ClassInfo DOMJITGetterBaseJSObject::s_info = { "DOMJITGetterBaseJSObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITGetterBaseJSObject) };
#if ENABLE(JIT)
const ClassInfo DOMJITFunctionObject::s_info = { "DOMJITFunctionObject"_s, &Base::s_info, nullptr, &DOMJITFunctionObject::checkSubClassSnippet, CREATE_METHOD_TABLE(DOMJITFunctionObject) };
#else
const ClassInfo DOMJITFunctionObject::s_info = { "DOMJITFunctionObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITFunctionObject) };
#endif
const ClassInfo DOMJITCheckJSCastObject::s_info = { "DOMJITCheckJSCastObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(DOMJITCheckJSCastObject) };
const ClassInfo JSTestCustomGetterSetter::s_info = { "JSTestCustomGetterSetter"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSTestCustomGetterSetter) };

const ClassInfo StaticCustomAccessor::s_info = { "StaticCustomAccessor"_s, &Base::s_info, &staticCustomAccessorTable, nullptr, CREATE_METHOD_TABLE(StaticCustomAccessor) };
const ClassInfo StaticCustomValue::s_info = { "StaticCustomValue"_s, &Base::s_info, &staticCustomValueTable, nullptr, CREATE_METHOD_TABLE(StaticCustomValue) };
const ClassInfo StaticDontDeleteDontEnum::s_info = { "StaticDontDeleteDontEnum"_s, &Base::s_info, &staticDontDeleteDontEnumTable, nullptr, CREATE_METHOD_TABLE(StaticDontDeleteDontEnum) };
const ClassInfo ObjectDoingSideEffectPutWithoutCorrectSlotStatus::s_info = { "ObjectDoingSideEffectPutWithoutCorrectSlotStatus"_s, &Base::s_info, &staticCustomAccessorTable, nullptr, CREATE_METHOD_TABLE(ObjectDoingSideEffectPutWithoutCorrectSlotStatus) };

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
        return &vm.destructibleObjectSpace();
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
        WasmStreamingParser* result = new (NotNull, allocateCell<WasmStreamingParser>(vm)) WasmStreamingParser(vm, structure);
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

        JSGlobalObject* globalObject = this->globalObject();
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "addBytes"_s), 0, functionWasmStreamingParserAddBytes, ImplementationVisibility::Public, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "finalize"_s), 0, functionWasmStreamingParserFinalize, ImplementationVisibility::Public, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
    }

    DECLARE_INFO;

    Ref<Wasm::ModuleInformation> m_info;
    Client m_client;
    Wasm::StreamingParser m_streamingParser;
};

const ClassInfo WasmStreamingParser::s_info = { "WasmStreamingParser"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WasmStreamingParser) };

JSC_DEFINE_HOST_FUNCTION(functionWasmStreamingParserAddBytes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsDynamicCast<WasmStreamingParser*>(callFrame->thisValue());
    if (!thisObject)
        RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(false)));

    JSValue value = callFrame->argument(0);
    BaseWebAssemblySourceProvider* provider = nullptr;
    if (auto* source = jsDynamicCast<JSSourceCode*>(value))
        provider = static_cast<BaseWebAssemblySourceProvider*>(source->sourceCode().provider());
    WebAssemblySourceProviderBufferGuard guard(provider);

    auto data = getWasmBufferFromValue(globalObject, value, guard);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());
    RELEASE_AND_RETURN(scope, JSValue::encode(jsNumber(static_cast<int32_t>(thisObject->streamingParser().addBytes(bitwise_cast<const uint8_t*>(data.first), data.second)))));
}

JSC_DEFINE_HOST_FUNCTION(functionWasmStreamingParserFinalize, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    auto* thisObject = jsDynamicCast<WasmStreamingParser*>(callFrame->thisValue());
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
        return &vm.destructibleObjectSpace();
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
        WasmStreamingCompiler* result = new (NotNull, allocateCell<WasmStreamingCompiler>(vm)) WasmStreamingCompiler(vm, structure, compilerMode, globalObject, promise, importObject);
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

        JSGlobalObject* globalObject = this->globalObject();
        putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "addBytes"_s), 0, functionWasmStreamingCompilerAddBytes, ImplementationVisibility::Public, NoIntrinsic, static_cast<unsigned>(PropertyAttribute::DontEnum));
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

const ClassInfo WasmStreamingCompiler::s_info = { "WasmStreamingCompiler"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(WasmStreamingCompiler) };

JSC_DEFINE_HOST_FUNCTION(functionWasmStreamingCompilerAddBytes, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsDynamicCast<WasmStreamingCompiler*>(callFrame->thisValue());
    if (!thisObject)
        RELEASE_AND_RETURN(scope, JSValue::encode(jsBoolean(false)));

    JSValue value = callFrame->argument(0);
    BaseWebAssemblySourceProvider* provider = nullptr;
    if (auto* source = jsDynamicCast<JSSourceCode*>(value))
        provider = static_cast<BaseWebAssemblySourceProvider*>(source->sourceCode().provider());
    WebAssemblySourceProviderBufferGuard guard(provider);

    auto data = getWasmBufferFromValue(globalObject, value, guard);
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
static JSC_DECLARE_HOST_FUNCTION(functionCreateStaticDontDeleteDontEnum);
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
static JSC_DECLARE_HOST_FUNCTION(functionToCacheableDictionary);
static JSC_DECLARE_HOST_FUNCTION(functionToUncacheableDictionary);
static JSC_DECLARE_HOST_FUNCTION(functionIsPrivateSymbol);
static JSC_DECLARE_HOST_FUNCTION(functionDumpAndResetPasDebugSpectrum);
static JSC_DECLARE_HOST_FUNCTION(functionMonotonicTimeNow);
static JSC_DECLARE_HOST_FUNCTION(functionWallTimeNow);
static JSC_DECLARE_HOST_FUNCTION(functionApproximateTimeNow);
static JSC_DECLARE_HOST_FUNCTION(functionHeapExtraMemorySize);
#if ENABLE(JIT)
static JSC_DECLARE_HOST_FUNCTION(functionJITSizeStatistics);
static JSC_DECLARE_HOST_FUNCTION(functionDumpJITSizeStatistics);
static JSC_DECLARE_HOST_FUNCTION(functionResetJITSizeStatistics);
#endif

static JSC_DECLARE_HOST_FUNCTION(functionEnsureArrayStorage);
#if PLATFORM(COCOA)
static JSC_DECLARE_HOST_FUNCTION(functionSetCrashLogMessage);;
#endif

const ClassInfo JSDollarVM::s_info = { "DollarVM"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSDollarVM) };

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
JSC_DEFINE_HOST_FUNCTION(functionCpuClflush, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
#if CPU(X86_64) && !OS(WINDOWS)
    if (!callFrame->argument(1).isUInt32())
        return JSValue::encode(jsBoolean(false));

    auto clflush = [] (void* ptr) {
        DollarVMAssertScope assertScope;
        char* ptrToFlush = static_cast<char*>(ptr);
        asm volatile ("clflush %0" :: "m"(*ptrToFlush) : "memory");
    };

    Vector<void*> toFlush;

    uint32_t offset = callFrame->argument(1).asUInt32();

    if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(callFrame->argument(0)))
        toFlush.append(bitwise_cast<char*>(view->vector()) + offset);
    else if (JSObject* object = jsDynamicCast<JSObject*>(callFrame->argument(0))) {
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

    IterationStatus operator()(StackVisitor& visitor) const
    {
        unsigned index = m_currentFrame++;
        // First frame (index 0) is `llintTrue` etc. function itself.
        if (index == 1) {
            if (visitor->codeBlock())
                m_jitType = visitor->codeBlock()->jitType();
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
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
    
    JSFunction* theFunction = jsDynamicCast<JSFunction*>(theFunctionValue);
    if (!theFunction)
        return nullptr;
    
    FunctionExecutable* executable = jsDynamicCast<FunctionExecutable*>(theFunction->executable());

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
        JSFunction* func = jsDynamicCast<JSFunction*>(value.asCell());
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
            return IterationStatus::Continue;
        VMInspector::dumpRegisters(visitor->callFrame());
        return IterationStatus::Done;
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

JSC_DEFINE_HOST_FUNCTION(functionInlineCapacity, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    if (auto* object = jsDynamicCast<JSObject*>(callFrame->argument(0)))
        return JSValue::encode(jsNumber(object->structure()->inlineCapacity()));

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
            return throwVMTypeError(globalObject, scope, "haveABadTime expects first argument to be an object if provided"_s);
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
            return throwVMTypeError(globalObject, scope, "isHavingABadTime expects first argument to be an object if provided"_s);
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

    auto callData = JSC::getCallData(function);
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
        return throwVMError(globalObject, throwScope, "Not supported for this platform"_s);

#if ENABLE(ASSEMBLER)
    if (g_jscConfig.isPermanentlyFrozen() || !g_jscConfig.disabledFreezingForTesting)
        return throwVMError(globalObject, throwScope, "Options are frozen"_s);

    if (callFrame->argumentCount() < 2)
        return throwVMError(globalObject, throwScope, "Invalid number of arguments"_s);
    JSValue arg0 = callFrame->argument(0);
    JSValue arg1 = callFrame->argument(1);
    if (!arg0.isCallable())
        return throwVMError(globalObject, throwScope, "arg0 should be a function"_s);
    if (!arg1.isNumber())
        return throwVMError(globalObject, throwScope, "arg1 should be a number"_s);

    JSFunction* function = jsCast<JSFunction*>(arg0);
    size_t desiredStackSize = arg1.asNumber();

    const StackBounds& bounds = Thread::current().stack();
    uint8_t* currentStackPosition = bitwise_cast<uint8_t*>(currentStackPointer());
    uint8_t* end = bitwise_cast<uint8_t*>(bounds.end());
    uint8_t* desiredStart = end + desiredStackSize;
    if (desiredStart >= currentStackPosition)
        return throwVMError(globalObject, throwScope, "Unable to setup desired stack size"_s);

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
    return throwVMError(globalObject, throwScope, "Not supported for this platform"_s);
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
    Structure* structure = JSProxy::createStructure(vm, globalObject, jsTarget->getPrototypeDirect());
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

    auto callback = jsDynamicCast<JSFunction*>(callFrame->argument(0));
    if (!callback)
        return throwVMTypeError(globalObject, scope, "First argument is not a JS function"_s);

    auto compiler = WasmStreamingCompiler::create(vm, globalObject, Wasm::CompilerMode::Validation, nullptr);
    MarkedArgumentBuffer args;
    args.append(compiler);
    ASSERT(!args.hasOverflowed());
    call(globalObject, callback, jsUndefined(), args, "You shouldn't see this..."_s);
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

    auto callback = jsDynamicCast<JSFunction*>(callFrame->argument(0));
    if (!callback)
        return throwVMTypeError(globalObject, scope, "First argument is not a JS function"_s);

    JSValue importArgument = callFrame->argument(1);
    JSObject* importObject = importArgument.getObject();
    if (UNLIKELY(!importArgument.isUndefined() && !importObject))
        return throwVMTypeError(globalObject, scope);

    auto compiler = WasmStreamingCompiler::create(vm, globalObject, Wasm::CompilerMode::FullCompile, importObject);
    MarkedArgumentBuffer args;
    args.append(compiler);
    ASSERT(!args.hasOverflowed());
    call(globalObject, callback, jsUndefined(), args, "You shouldn't see this..."_s);
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

JSC_DEFINE_HOST_FUNCTION(functionCreateStaticDontDeleteDontEnum, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Structure* structure = StaticDontDeleteDontEnum::createStructure(vm, globalObject, jsNull());
    auto* result = StaticDontDeleteDontEnum::create(vm, structure);
    return JSValue::encode(result);
}

JSC_DEFINE_HOST_FUNCTION(functionCreateObjectDoingSideEffectPutWithoutCorrectSlotStatus, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);

    auto* dollarVM = jsDynamicCast<JSDollarVM*>(callFrame->thisValue());
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

    RELEASE_AND_RETURN(scope, JSValue::encode(JSFunction::create(vm, globalObject, 1, name, functionCreateEmptyFunctionWithName, ImplementationVisibility::Public)));
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
    ImpureGetter* impureGetter = jsDynamicCast<ImpureGetter*>(asObject(base.asCell()));
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

    SourceCode source = makeSource(WTFMove(functionText), { });
    JSFunction* func = JSFunction::create(vm, createBuiltinExecutable(vm, source, Identifier::fromString(vm, "foo"_s), ImplementationVisibility::Public, ConstructorKind::None, ConstructAbility::CannotConstruct)->link(vm, nullptr, source), globalObject);

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
        return throwVMError(globalObject, scope, "Unknown private name."_s);

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

    Root* root = jsDynamicCast<Root*>(callFrame->argument(0));
    if (!root)
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Cannot create Element without a Root."_s)));
    return JSValue::encode(Element::create(vm, globalObject, root));
}

JSC_DEFINE_HOST_FUNCTION(functionGetElement, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    JSLockHolder lock(vm);
    Root* root = jsDynamicCast<Root*>(callFrame->argument(0));
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

    SimpleObject* simpleObject = jsDynamicCast<SimpleObject*>(callFrame->argument(0));
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

    SimpleObject* simpleObject = jsDynamicCast<SimpleObject*>(callFrame->argument(0));
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
    StackVisitor::visit(callFrame, vm, [&] (StackVisitor& visitor) -> IterationStatus {
        DollarVMAssertScope assertScope;
        if (visitor->isInlinedFrame())
            return IterationStatus::Continue;
        if (visitor->isWasmFrame())
            return IterationStatus::Continue;
        result->push(globalObject, jsCast<JSObject*>(visitor->callee().asCell()));
        scope.releaseAssertNoException(); // This function is only called from tests.
        return IterationStatus::Continue;
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
    RELEASE_ASSERT(functionValue.isCallable());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

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
    RELEASE_ASSERT(functionValue.isCallable());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

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
    RELEASE_ASSERT(functionValue.isCallable());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

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
    RELEASE_ASSERT(functionValue.isCallable());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

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

JSC_DEFINE_HOST_FUNCTION(functionGlobalObjectForObject, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    JSValue value = callFrame->argument(0);
    RELEASE_ASSERT(value.isObject());
    JSGlobalObject* result = jsCast<JSObject*>(value)->globalObject();
    RELEASE_ASSERT(result);
    return JSValue::encode(result->globalThis());
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

    GetterSetter* getterSetter = jsDynamicCast<GetterSetter*>(callFrame->argument(0));
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

JSC_DEFINE_HOST_FUNCTION(functionDeltaBetweenButterflies, (JSGlobalObject*, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    JSObject* a = jsDynamicCast<JSObject*>(callFrame->argument(0));
    JSObject* b = jsDynamicCast<JSObject*>(callFrame->argument(1));
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
    string.convertTo16Bit();
    return JSValue::encode(jsString(vm, WTFMove(string)));
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
        result->push(globalObject, JSValue(structure->id().bits()));
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
    JSFunction* function = jsDynamicCast<JSFunction*>(target);
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

    JSArray* array = jsDynamicCast<JSArray*>(callFrame->argument(0));
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

JSC_DEFINE_HOST_FUNCTION(functionToCacheableDictionary, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = jsDynamicCast<JSObject*>(callFrame->argument(0));
    if (!object)
        return throwVMTypeError(globalObject, scope, "Expected first argument to be an object"_s);
    if (!object->structure()->isUncacheableDictionary())
        object->convertToDictionary(vm);
    return JSValue::encode(object);
}

JSC_DEFINE_HOST_FUNCTION(functionToUncacheableDictionary, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* object = jsDynamicCast<JSObject*>(callFrame->argument(0));
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

JSC_DEFINE_HOST_FUNCTION(functionMonotonicTimeNow, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsNumber(MonotonicTime::now().secondsSinceEpoch().milliseconds()));
}

JSC_DEFINE_HOST_FUNCTION(functionWallTimeNow, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsNumber(WallTime::now().secondsSinceEpoch().milliseconds()));
}

JSC_DEFINE_HOST_FUNCTION(functionApproximateTimeNow, (JSGlobalObject*, CallFrame*))
{
    return JSValue::encode(jsNumber(ApproximateTime::now().secondsSinceEpoch().milliseconds()));
}

JSC_DEFINE_HOST_FUNCTION(functionHeapExtraMemorySize, (JSGlobalObject* globalObject, CallFrame*))
{
    DollarVMAssertScope assertScope;
    return JSValue::encode(jsNumber(globalObject->vm().heap.extraMemorySize()));
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

JSC_DEFINE_HOST_FUNCTION(functionEnsureArrayStorage, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;

    VM& vm = globalObject->vm();

    JSValue arg = callFrame->argument(0);
    if (arg.isObject())
        asObject(arg)->ensureArrayStorage(vm);
    return JSValue::encode(jsUndefined());
}


#if PLATFORM(COCOA)
JSC_DEFINE_HOST_FUNCTION(functionSetCrashLogMessage, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    DollarVMAssertScope assertScope;
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    String message = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    WTF::setCrashLogMessage(message.utf8().data());

    return JSValue::encode(jsUndefined());
}
#endif

constexpr unsigned jsDollarVMPropertyAttributes = PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum | PropertyAttribute::DontDelete;

void JSDollarVM::finishCreation(VM& vm)
{
    DollarVMAssertScope assertScope;
    Base::finishCreation(vm);

    JSGlobalObject* globalObject = this->globalObject();

    auto addFunction = [&] (VM& vm, ASCIILiteral name, NativeFunction function, unsigned arguments) {
        DollarVMAssertScope assertScope;
        JSDollarVM::addFunction(vm, globalObject, name, function, arguments);
    };
    auto addConstructibleFunction = [&] (VM& vm, ASCIILiteral name, NativeFunction function, unsigned arguments) {
        DollarVMAssertScope assertScope;
        JSDollarVM::addConstructibleFunction(vm, globalObject, name, function, arguments);
    };

    addFunction(vm, "abort"_s, functionCrash, 0);
    addFunction(vm, "crash"_s, functionCrash, 0);
    addFunction(vm, "breakpoint"_s, functionBreakpoint, 0);

    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "dfgTrue"_s), 0, functionDFGTrue, ImplementationVisibility::Public, DFGTrueIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "ftlTrue"_s), 0, functionFTLTrue, ImplementationVisibility::Public, FTLTrueIntrinsic, jsDollarVMPropertyAttributes);

    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuMfence"_s), 0, functionCpuMfence, ImplementationVisibility::Public, CPUMfenceIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuRdtsc"_s), 0, functionCpuRdtsc, ImplementationVisibility::Public, CPURdtscIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuCpuid"_s), 0, functionCpuCpuid, ImplementationVisibility::Public, CPUCpuidIntrinsic, jsDollarVMPropertyAttributes);
    putDirectNativeFunction(vm, globalObject, Identifier::fromString(vm, "cpuPause"_s), 0, functionCpuPause, ImplementationVisibility::Public, CPUPauseIntrinsic, jsDollarVMPropertyAttributes);
    addFunction(vm, "cpuClflush"_s, functionCpuClflush, 2);

    addFunction(vm, "llintTrue"_s, functionLLintTrue, 0);
    addFunction(vm, "baselineJITTrue"_s, functionBaselineJITTrue, 0);

    addFunction(vm, "noInline"_s, functionNoInline, 1);

    addFunction(vm, "gc"_s, functionGC, 0);
    addFunction(vm, "gcSweepAsynchronously"_s, functionGCSweepAsynchronously, 0);
    addFunction(vm, "edenGC"_s, functionEdenGC, 0);
    addFunction(vm, "dumpSubspaceHashes"_s, functionDumpSubspaceHashes, 0);

    addFunction(vm, "callFrame"_s, functionCallFrame, 1);
    addFunction(vm, "codeBlockFor"_s, functionCodeBlockFor, 1);
    addFunction(vm, "codeBlockForFrame"_s, functionCodeBlockForFrame, 1);
    addFunction(vm, "dumpSourceFor"_s, functionDumpSourceFor, 1);
    addFunction(vm, "dumpBytecodeFor"_s, functionDumpBytecodeFor, 1);

    addFunction(vm, "dataLog"_s, functionDataLog, 1);
    addFunction(vm, "print"_s, functionPrint, 1);
    addFunction(vm, "dumpCallFrame"_s, functionDumpCallFrame, 0);
    addFunction(vm, "dumpStack"_s, functionDumpStack, 0);
    addFunction(vm, "dumpRegisters"_s, functionDumpRegisters, 1);

    addFunction(vm, "dumpCell"_s, functionDumpCell, 1);

    addFunction(vm, "indexingMode"_s, functionIndexingMode, 1);
    addFunction(vm, "inlineCapacity"_s, functionInlineCapacity, 1);
    addFunction(vm, "clearLinkBufferStats"_s, functionClearLinkBufferStats, 0);
    addFunction(vm, "linkBufferStats"_s, functionLinkBufferStats, 0);
    addFunction(vm, "value"_s, functionValue, 1);
    addFunction(vm, "getpid"_s, functionGetPID, 0);

    addFunction(vm, "haveABadTime"_s, functionHaveABadTime, 1);
    addFunction(vm, "isHavingABadTime"_s, functionIsHavingABadTime, 1);

    addFunction(vm, "callWithStackSize"_s, functionCallWithStackSize, 2);

    addFunction(vm, "createGlobalObject"_s, functionCreateGlobalObject, 0);
    addFunction(vm, "createProxy"_s, functionCreateProxy, 1);
    addFunction(vm, "createRuntimeArray"_s, functionCreateRuntimeArray, 0);
    addFunction(vm, "createNullRopeString"_s, functionCreateNullRopeString, 0);

    addFunction(vm, "createImpureGetter"_s, functionCreateImpureGetter, 1);
    addFunction(vm, "createCustomGetterObject"_s, functionCreateCustomGetterObject, 0);
    addFunction(vm, "createDOMJITNodeObject"_s, functionCreateDOMJITNodeObject, 0);
    addFunction(vm, "createDOMJITGetterObject"_s, functionCreateDOMJITGetterObject, 0);
    addFunction(vm, "createDOMJITGetterNoEffectsObject"_s, functionCreateDOMJITGetterNoEffectsObject, 0);
    addFunction(vm, "createDOMJITGetterComplexObject"_s, functionCreateDOMJITGetterComplexObject, 0);
    addFunction(vm, "createDOMJITFunctionObject"_s, functionCreateDOMJITFunctionObject, 0);
    addFunction(vm, "createDOMJITCheckJSCastObject"_s, functionCreateDOMJITCheckJSCastObject, 0);
    addFunction(vm, "createDOMJITGetterBaseJSObject"_s, functionCreateDOMJITGetterBaseJSObject, 0);
    addFunction(vm, "createBuiltin"_s, functionCreateBuiltin, 2);
#if ENABLE(WEBASSEMBLY)
    addFunction(vm, "createWasmStreamingParser"_s, functionCreateWasmStreamingParser, 0);
    addFunction(vm, "createWasmStreamingCompilerForCompile"_s, functionCreateWasmStreamingCompilerForCompile, 0);
    addFunction(vm, "createWasmStreamingCompilerForInstantiate"_s, functionCreateWasmStreamingCompilerForInstantiate, 0);
#endif
    addFunction(vm, "createStaticCustomAccessor"_s, functionCreateStaticCustomAccessor, 0);
    addFunction(vm, "createStaticCustomValue"_s, functionCreateStaticCustomValue, 0);
    addFunction(vm, "createStaticDontDeleteDontEnum"_s, functionCreateStaticDontDeleteDontEnum, 0);
    addFunction(vm, "createObjectDoingSideEffectPutWithoutCorrectSlotStatus"_s, functionCreateObjectDoingSideEffectPutWithoutCorrectSlotStatus, 0);
    addFunction(vm, "createEmptyFunctionWithName"_s, functionCreateEmptyFunctionWithName, 1);
    addFunction(vm, "getPrivateProperty"_s, functionGetPrivateProperty, 2);
    addFunction(vm, "setImpureGetterDelegate"_s, functionSetImpureGetterDelegate, 2);

    addConstructibleFunction(vm, "Root"_s, functionCreateRoot, 0);
    addConstructibleFunction(vm, "Element"_s, functionCreateElement, 1);
    addFunction(vm, "getElement"_s, functionGetElement, 1);

    addConstructibleFunction(vm, "SimpleObject"_s, functionCreateSimpleObject, 0);
    addFunction(vm, "getHiddenValue"_s, functionGetHiddenValue, 1);
    addFunction(vm, "setHiddenValue"_s, functionSetHiddenValue, 2);

    addFunction(vm, "shadowChickenFunctionsOnStack"_s, functionShadowChickenFunctionsOnStack, 0);
    addFunction(vm, "setGlobalConstRedeclarationShouldNotThrow"_s, functionSetGlobalConstRedeclarationShouldNotThrow, 0);

    addFunction(vm, "findTypeForExpression"_s, functionFindTypeForExpression, 2);
    addFunction(vm, "returnTypeFor"_s, functionReturnTypeFor, 1);

    addFunction(vm, "flattenDictionaryObject"_s, functionFlattenDictionaryObject, 1);

    addFunction(vm, "dumpBasicBlockExecutionRanges"_s, functionDumpBasicBlockExecutionRanges , 0);
    addFunction(vm, "hasBasicBlockExecuted"_s, functionHasBasicBlockExecuted, 2);
    addFunction(vm, "basicBlockExecutionCount"_s, functionBasicBlockExecutionCount, 2);

    addFunction(vm, "enableDebuggerModeWhenIdle"_s, functionEnableDebuggerModeWhenIdle, 0);
    addFunction(vm, "disableDebuggerModeWhenIdle"_s, functionDisableDebuggerModeWhenIdle, 0);

    addFunction(vm, "deleteAllCodeWhenIdle"_s, functionDeleteAllCodeWhenIdle, 0);

    addFunction(vm, "globalObjectCount"_s, functionGlobalObjectCount, 0);
    addFunction(vm, "globalObjectForObject"_s, functionGlobalObjectForObject, 1);

    addFunction(vm, "getGetterSetter"_s, functionGetGetterSetter, 2);
    addFunction(vm, "loadGetterFromGetterSetter"_s, functionLoadGetterFromGetterSetter, 1);
    addFunction(vm, "createCustomTestGetterSetter"_s, functionCreateCustomTestGetterSetter, 1);

    addFunction(vm, "deltaBetweenButterflies"_s, functionDeltaBetweenButterflies, 2);
    
    addFunction(vm, "currentCPUTime"_s, functionCurrentCPUTime, 0);
    addFunction(vm, "totalGCTime"_s, functionTotalGCTime, 0);

    addFunction(vm, "parseCount"_s, functionParseCount, 0);

    addFunction(vm, "isWasmSupported"_s, functionIsWasmSupported, 0);
    addFunction(vm, "make16BitStringIfPossible"_s, functionMake16BitStringIfPossible, 1);

    addFunction(vm, "getStructureTransitionList"_s, functionGetStructureTransitionList, 1);
    addFunction(vm, "getConcurrently"_s, functionGetConcurrently, 2);

    addFunction(vm, "hasOwnLengthProperty"_s, functionHasOwnLengthProperty, 1);
    addFunction(vm, "rejectPromiseAsHandled"_s, functionRejectPromiseAsHandled, 1);

    addFunction(vm, "setUserPreferredLanguages"_s, functionSetUserPreferredLanguages, 1);
    addFunction(vm, "icuVersion"_s, functionICUVersion, 0);
    addFunction(vm, "icuHeaderVersion"_s, functionICUHeaderVersion, 0);

    addFunction(vm, "assertEnabled"_s, functionAssertEnabled, 0);
    addFunction(vm, "securityAssertEnabled"_s, functionSecurityAssertEnabled, 0);
    addFunction(vm, "asanEnabled"_s, functionAsanEnabled, 0);

    addFunction(vm, "isMemoryLimited"_s, functionIsMemoryLimited, 0);
    addFunction(vm, "useJIT"_s, functionUseJIT, 0);
    addFunction(vm, "isGigacageEnabled"_s, functionIsGigacageEnabled, 0);

    addFunction(vm, "toCacheableDictionary"_s, functionToCacheableDictionary, 1);
    addFunction(vm, "toUncacheableDictionary"_s, functionToUncacheableDictionary, 1);

    addFunction(vm, "isPrivateSymbol"_s, functionIsPrivateSymbol, 1);
    addFunction(vm, "dumpAndResetPasDebugSpectrum"_s, functionDumpAndResetPasDebugSpectrum, 0);

    addFunction(vm, "monotonicTimeNow"_s, functionMonotonicTimeNow, 0);
    addFunction(vm, "wallTimeNow"_s, functionWallTimeNow, 0);
    addFunction(vm, "approximateTimeNow"_s, functionApproximateTimeNow, 0);

    addFunction(vm, "heapExtraMemorySize"_s, functionHeapExtraMemorySize, 0);

#if ENABLE(JIT)
    addFunction(vm, "jitSizeStatistics"_s, functionJITSizeStatistics, 0);
    addFunction(vm, "dumpJITSizeStatistics"_s, functionDumpJITSizeStatistics, 0);
    addFunction(vm, "resetJITSizeStatistics"_s, functionResetJITSizeStatistics, 0);
#endif

    addFunction(vm, "ensureArrayStorage"_s, functionEnsureArrayStorage, 1);

#if PLATFORM(COCOA)
    addFunction(vm, "setCrashLogMessage"_s, functionSetCrashLogMessage, 1);
#endif

    m_objectDoingSideEffectPutWithoutCorrectSlotStatusStructureID.set(vm, this, ObjectDoingSideEffectPutWithoutCorrectSlotStatus::createStructure(vm, globalObject, jsNull()));
}

void JSDollarVM::addFunction(VM& vm, JSGlobalObject* globalObject, ASCIILiteral name, NativeFunction function, unsigned arguments)
{
    DollarVMAssertScope assertScope;
    Identifier identifier = Identifier::fromString(vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function, ImplementationVisibility::Public), jsDollarVMPropertyAttributes);
}

void JSDollarVM::addConstructibleFunction(VM& vm, JSGlobalObject* globalObject, ASCIILiteral name, NativeFunction function, unsigned arguments)
{
    DollarVMAssertScope assertScope;
    Identifier identifier = Identifier::fromString(vm, name);
    putDirect(vm, identifier, JSFunction::create(vm, globalObject, arguments, identifier.string(), function, ImplementationVisibility::Public, NoIntrinsic, function), jsDollarVMPropertyAttributes);
}

template<typename Visitor>
void JSDollarVM::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    JSDollarVM* thisObject = jsCast<JSDollarVM*>(cell);
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_objectDoingSideEffectPutWithoutCorrectSlotStatusStructureID);
}

DEFINE_VISIT_CHILDREN(JSDollarVM);

} // namespace JSC

IGNORE_WARNINGS_END
