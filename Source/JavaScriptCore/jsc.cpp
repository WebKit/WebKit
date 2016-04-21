/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2004-2008, 2012-2013, 2015-2016 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Bjoern Graf (bjoern.graf@gmail.com)
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

#include "config.h"

#include "ArrayPrototype.h"
#include "BuiltinExecutableCreator.h"
#include "ButterflyInlines.h"
#include "BytecodeGenerator.h"
#include "CodeBlock.h"
#include "Completion.h"
#include "CopiedSpaceInlines.h"
#include "DFGPlan.h"
#include "Disassembler.h"
#include "Exception.h"
#include "ExceptionHelpers.h"
#include "GetterSetter.h"
#include "HeapProfiler.h"
#include "HeapSnapshotBuilder.h"
#include "HeapStatistics.h"
#include "InitializeThreading.h"
#include "Interpreter.h"
#include "JSArray.h"
#include "JSArrayBuffer.h"
#include "JSCInlines.h"
#include "JSFunction.h"
#include "JSInternalPromise.h"
#include "JSInternalPromiseDeferred.h"
#include "JSLock.h"
#include "JSNativeStdFunction.h"
#include "JSONObject.h"
#include "JSProxy.h"
#include "JSString.h"
#include "JSWASMModule.h"
#include "ProfilerDatabase.h"
#include "SamplingProfiler.h"
#include "ShadowChicken.h"
#include "StackVisitor.h"
#include "StructureInlines.h"
#include "StructureRareDataInlines.h"
#include "SuperSampler.h"
#include "TestRunnerUtils.h"
#include "TypeProfilerLog.h"
#include "WASMModuleParser.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/StringPrintStream.h>
#include <wtf/text/StringBuilder.h>

#if OS(WINDOWS)
#include <direct.h>
#else
#include <unistd.h>
#endif

#if HAVE(READLINE)
// readline/history.h has a Function typedef which conflicts with the WTF::Function template from WTF/Forward.h
// We #define it to something else to avoid this conflict.
#define Function ReadlineFunction
#include <readline/history.h>
#include <readline/readline.h>
#undef Function
#endif

#if HAVE(SYS_TIME_H)
#include <sys/time.h>
#endif

#if HAVE(SIGNAL_H)
#include <signal.h>
#endif

#if COMPILER(MSVC)
#include <crtdbg.h>
#include <mmsystem.h>
#include <windows.h>
#endif

#if PLATFORM(IOS) && CPU(ARM_THUMB2)
#include <fenv.h>
#include <arm/arch.h>
#endif

#if PLATFORM(EFL)
#include <Ecore.h>
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

using namespace JSC;
using namespace WTF;

namespace {

NO_RETURN_WITH_VALUE static void jscExit(int status)
{
    waitForAsynchronousDisassembly();
    
#if ENABLE(DFG_JIT)
    if (DFG::isCrashing()) {
        for (;;) {
#if OS(WINDOWS)
            Sleep(1000);
#else
            pause();
#endif
        }
    }
#endif // ENABLE(DFG_JIT)
    exit(status);
}

class Element;
class ElementHandleOwner;
class Masuqerader;
class Root;
class RuntimeArray;

class Element : public JSNonFinalObject {
public:
    Element(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    typedef JSNonFinalObject Base;
    static const bool needsDestruction = false;

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
        visitor.append(&thisObject->m_root);
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
    virtual bool isReachableFromOpaqueRoots(Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
    {
        Element* element = jsCast<Element*>(handle.slot()->asCell());
        return visitor.containsOpaqueRoot(element->root());
    }
};

class Masquerader : public JSNonFinalObject {
public:
    Masquerader(VM& vm, Structure* structure)
        : Base(vm, structure)
    {
    }

    typedef JSNonFinalObject Base;
    static const unsigned StructureFlags = Base::StructureFlags | JSC::MasqueradesAsUndefined;

    static Masquerader* create(VM& vm, JSGlobalObject* globalObject)
    {
        globalObject->masqueradesAsUndefinedWatchpoint()->fireAll("Masquerading object allocated");
        Structure* structure = createStructure(vm, globalObject, jsNull());
        Masquerader* result = new (NotNull, allocateCell<Masquerader>(vm.heap, sizeof(Masquerader))) Masquerader(vm, structure);
        result->finishCreation(vm);
        return result;
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
    {
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
    }

    DECLARE_INFO;
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
    static const bool needsDestruction = true;

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
        ImpureGetter* thisObject = jsCast<ImpureGetter*>(object);
        
        if (thisObject->m_delegate && thisObject->m_delegate->getPropertySlot(exec, name, slot))
            return true;

        return Base::getOwnPropertySlot(object, exec, name, slot);
    }

    static void visitChildren(JSCell* cell, SlotVisitor& visitor)
    {
        Base::visitChildren(cell, visitor);
        ImpureGetter* thisObject = jsCast<ImpureGetter*>(cell);
        visitor.append(&thisObject->m_delegate);
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
            slot.setCacheableCustom(thisObject, DontDelete | ReadOnly | DontEnum, thisObject->customGetter);
            return true;
        }
        return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
    }

private:
    static EncodedJSValue customGetter(ExecState* exec, EncodedJSValue thisValue, PropertyName)
    {
        CustomGetter* thisObject = jsDynamicCast<CustomGetter*>(JSValue::decode(thisValue));
        if (!thisObject)
            return throwVMTypeError(exec);
        bool shouldThrow = thisObject->get(exec, PropertyName(Identifier::fromString(exec, "shouldThrow"))).toBoolean(exec);
        if (shouldThrow)
            return throwVMTypeError(exec);
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
        RuntimeArray* runtimeArray = new (NotNull, allocateCell<RuntimeArray>(*exec->heap())) RuntimeArray(exec, structure);
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
        RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
        if (propertyName == exec->propertyNames().length) {
            slot.setCacheableCustom(thisObject, DontDelete | ReadOnly | DontEnum, thisObject->lengthGetter);
            return true;
        }

        Optional<uint32_t> index = parseIndex(propertyName);
        if (index && index.value() < thisObject->getLength()) {
            slot.setValue(thisObject, DontDelete | DontEnum, jsNumber(thisObject->m_vector[index.value()]));
            return true;
        }

        return JSObject::getOwnPropertySlot(thisObject, exec, propertyName, slot);
    }

    static bool getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned index, PropertySlot& slot)
    {
        RuntimeArray* thisObject = jsCast<RuntimeArray*>(object);
        if (index < thisObject->getLength()) {
            slot.setValue(thisObject, DontDelete | DontEnum, jsNumber(thisObject->m_vector[index]));
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
        return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info(), ArrayClass);
    }

protected:
    void finishCreation(ExecState* exec)
    {
        Base::finishCreation(exec->vm());
        ASSERT(inherits(info()));

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
        RuntimeArray* thisObject = jsDynamicCast<RuntimeArray*>(JSValue::decode(thisValue));
        if (!thisObject)
            return throwVMTypeError(exec);
        return JSValue::encode(jsNumber(thisObject->getLength()));
    }

    Vector<int> m_vector;
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
        visitor.append(&thisObject->m_hiddenValue);
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


const ClassInfo Element::s_info = { "Element", &Base::s_info, 0, CREATE_METHOD_TABLE(Element) };
const ClassInfo Masquerader::s_info = { "Masquerader", &Base::s_info, 0, CREATE_METHOD_TABLE(Masquerader) };
const ClassInfo Root::s_info = { "Root", &Base::s_info, 0, CREATE_METHOD_TABLE(Root) };
const ClassInfo ImpureGetter::s_info = { "ImpureGetter", &Base::s_info, 0, CREATE_METHOD_TABLE(ImpureGetter) };
const ClassInfo CustomGetter::s_info = { "CustomGetter", &Base::s_info, 0, CREATE_METHOD_TABLE(CustomGetter) };
const ClassInfo RuntimeArray::s_info = { "RuntimeArray", &Base::s_info, 0, CREATE_METHOD_TABLE(RuntimeArray) };
const ClassInfo SimpleObject::s_info = { "SimpleObject", &Base::s_info, 0, CREATE_METHOD_TABLE(SimpleObject) };

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

}

static bool fillBufferWithContentsOfFile(const String& fileName, Vector<char>& buffer);

static EncodedJSValue JSC_HOST_CALL functionCreateProxy(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateRuntimeArray(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateImpureGetter(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateCustomGetterObject(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateBuiltin(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateGlobalObject(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionSetImpureGetterDelegate(ExecState*);

static EncodedJSValue JSC_HOST_CALL functionSetElementRoot(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateRoot(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateElement(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGetElement(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCreateSimpleObject(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGetHiddenValue(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionSetHiddenValue(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionPrint(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDebug(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDataLogValue(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDescribe(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDescribeArray(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionJSCStack(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGCAndSweep(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFullGC(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionEdenGC(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionForceGCSlowPaths(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionHeapSize(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionAddressOf(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGetGetterSetter(ExecState*);
#ifndef NDEBUG
static EncodedJSValue JSC_HOST_CALL functionDumpCallFrame(ExecState*);
#endif
static EncodedJSValue JSC_HOST_CALL functionVersion(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionRun(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionLoad(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionReadFile(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCheckSyntax(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionReadline(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionPreciseTime(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNeverInlineFunction(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNoDFG(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionOptimizeNextInvocation(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionNumberOfDFGCompiles(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionReoptimizationRetryCount(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionTransferArrayBuffer(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFailNextNewCodeBlock(ExecState*);
static NO_RETURN_WITH_VALUE EncodedJSValue JSC_HOST_CALL functionQuit(ExecState*);
static NO_RETURN_DUE_TO_CRASH EncodedJSValue JSC_HOST_CALL functionAbort(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFalse1(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFalse2(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionUndefined1(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionUndefined2(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIsInt32(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionEffectful42(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIdentity(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionMakeMasquerader(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionHasCustomProperties(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDumpTypesForAllVariables(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionFindTypeForExpression(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionReturnTypeFor(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDumpBasicBlockExecutionRanges(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionHasBasicBlockExecuted(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionBasicBlockExecutionCount(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionEnableExceptionFuzz(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionDrainMicrotasks(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionIs32BitPlatform(ExecState*);
#if ENABLE(WEBASSEMBLY)
static EncodedJSValue JSC_HOST_CALL functionLoadWebAssembly(ExecState*);
#endif
static EncodedJSValue JSC_HOST_CALL functionLoadModule(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionCheckModuleSyntax(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionPlatformSupportsSamplingProfiler(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshot(ExecState*);
#if ENABLE(SAMPLING_PROFILER)
static EncodedJSValue JSC_HOST_CALL functionStartSamplingProfiler(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionSamplingProfilerStackTraces(ExecState*);
#endif

#if ENABLE(SAMPLING_FLAGS)
static EncodedJSValue JSC_HOST_CALL functionSetSamplingFlags(ExecState*);
static EncodedJSValue JSC_HOST_CALL functionClearSamplingFlags(ExecState*);
#endif

static EncodedJSValue JSC_HOST_CALL functionShadowChickenFunctionsOnStack(ExecState*);

struct Script {
    bool isFile;
    char* argument;

    Script(bool isFile, char *argument)
        : isFile(isFile)
        , argument(argument)
    {
    }
};

class CommandLine {
public:
    CommandLine(int argc, char** argv)
    {
        parseArguments(argc, argv);
    }

    bool m_interactive { false };
    bool m_dump { false };
    bool m_module { false };
    bool m_exitCode { false };
    Vector<Script> m_scripts;
    Vector<String> m_arguments;
    bool m_profile { false };
    String m_profilerOutput;
    bool m_dumpSamplingProfilerData { false };

    void parseArguments(int, char**);
};

static const char interactivePrompt[] = ">>> ";

class StopWatch {
public:
    void start();
    void stop();
    long getElapsedMS(); // call stop() first

private:
    double m_startTime;
    double m_stopTime;
};

void StopWatch::start()
{
    m_startTime = monotonicallyIncreasingTime();
}

void StopWatch::stop()
{
    m_stopTime = monotonicallyIncreasingTime();
}

long StopWatch::getElapsedMS()
{
    return static_cast<long>((m_stopTime - m_startTime) * 1000);
}

template<typename Vector>
static inline String stringFromUTF(const Vector& utf8)
{
    return String::fromUTF8WithLatin1Fallback(utf8.data(), utf8.size());
}

template<typename Vector>
static inline SourceCode jscSource(const Vector& utf8, const String& filename)
{
    String str = stringFromUTF(utf8);
    return makeSource(str, filename);
}

class GlobalObject : public JSGlobalObject {
private:
    GlobalObject(VM&, Structure*);

public:
    typedef JSGlobalObject Base;

    static GlobalObject* create(VM& vm, Structure* structure, const Vector<String>& arguments)
    {
        GlobalObject* object = new (NotNull, allocateCell<GlobalObject>(vm.heap)) GlobalObject(vm, structure);
        object->finishCreation(vm, arguments);
        vm.heap.addFinalizer(object, destroy);
        return object;
    }

    static const bool needsDestruction = false;

    DECLARE_INFO;
    static const GlobalObjectMethodTable s_globalObjectMethodTable;

    static Structure* createStructure(VM& vm, JSValue prototype)
    {
        return Structure::create(vm, 0, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    }

    static RuntimeFlags javaScriptRuntimeFlags(const JSGlobalObject*) { return RuntimeFlags::createAllEnabled(); }

protected:
    void finishCreation(VM& vm, const Vector<String>& arguments)
    {
        Base::finishCreation(vm);

        addFunction(vm, "dataLogValue", functionDataLogValue, 1);
        addFunction(vm, "debug", functionDebug, 1);
        addFunction(vm, "describe", functionDescribe, 1);
        addFunction(vm, "describeArray", functionDescribeArray, 1);
        addFunction(vm, "print", functionPrint, 1);
        addFunction(vm, "quit", functionQuit, 0);
        addFunction(vm, "abort", functionAbort, 0);
        addFunction(vm, "gc", functionGCAndSweep, 0);
        addFunction(vm, "fullGC", functionFullGC, 0);
        addFunction(vm, "edenGC", functionEdenGC, 0);
        addFunction(vm, "forceGCSlowPaths", functionForceGCSlowPaths, 0);
        addFunction(vm, "gcHeapSize", functionHeapSize, 0);
        addFunction(vm, "addressOf", functionAddressOf, 1);
        addFunction(vm, "getGetterSetter", functionGetGetterSetter, 2);
#ifndef NDEBUG
        addFunction(vm, "dumpCallFrame", functionDumpCallFrame, 0);
#endif
        addFunction(vm, "version", functionVersion, 1);
        addFunction(vm, "run", functionRun, 1);
        addFunction(vm, "load", functionLoad, 1);
        addFunction(vm, "readFile", functionReadFile, 1);
        addFunction(vm, "checkSyntax", functionCheckSyntax, 1);
        addFunction(vm, "jscStack", functionJSCStack, 1);
        addFunction(vm, "readline", functionReadline, 0);
        addFunction(vm, "preciseTime", functionPreciseTime, 0);
        addFunction(vm, "neverInlineFunction", functionNeverInlineFunction, 1);
        addFunction(vm, "noInline", functionNeverInlineFunction, 1);
        addFunction(vm, "noDFG", functionNoDFG, 1);
        addFunction(vm, "numberOfDFGCompiles", functionNumberOfDFGCompiles, 1);
        addFunction(vm, "optimizeNextInvocation", functionOptimizeNextInvocation, 1);
        addFunction(vm, "reoptimizationRetryCount", functionReoptimizationRetryCount, 1);
        addFunction(vm, "transferArrayBuffer", functionTransferArrayBuffer, 1);
        addFunction(vm, "failNextNewCodeBlock", functionFailNextNewCodeBlock, 1);
#if ENABLE(SAMPLING_FLAGS)
        addFunction(vm, "setSamplingFlags", functionSetSamplingFlags, 1);
        addFunction(vm, "clearSamplingFlags", functionClearSamplingFlags, 1);
#endif
        addFunction(vm, "shadowChickenFunctionsOnStack", functionShadowChickenFunctionsOnStack, 0);
        addConstructableFunction(vm, "Root", functionCreateRoot, 0);
        addConstructableFunction(vm, "Element", functionCreateElement, 1);
        addFunction(vm, "getElement", functionGetElement, 1);
        addFunction(vm, "setElementRoot", functionSetElementRoot, 2);
        
        addConstructableFunction(vm, "SimpleObject", functionCreateSimpleObject, 0);
        addFunction(vm, "getHiddenValue", functionGetHiddenValue, 1);
        addFunction(vm, "setHiddenValue", functionSetHiddenValue, 2);
        
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "DFGTrue"), 0, functionFalse1, DFGTrueIntrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "OSRExit"), 0, functionUndefined1, OSRExitIntrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "isFinalTier"), 0, functionFalse2, IsFinalTierIntrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "predictInt32"), 0, functionUndefined2, SetInt32HeapPredictionIntrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "isInt32"), 0, functionIsInt32, CheckInt32Intrinsic, DontEnum);
        putDirectNativeFunction(vm, this, Identifier::fromString(&vm, "fiatInt52"), 0, functionIdentity, FiatInt52Intrinsic, DontEnum);
        
        addFunction(vm, "effectful42", functionEffectful42, 0);
        addFunction(vm, "makeMasquerader", functionMakeMasquerader, 0);
        addFunction(vm, "hasCustomProperties", functionHasCustomProperties, 0);

        addFunction(vm, "createProxy", functionCreateProxy, 1);
        addFunction(vm, "createRuntimeArray", functionCreateRuntimeArray, 0);

        addFunction(vm, "createImpureGetter", functionCreateImpureGetter, 1);
        addFunction(vm, "createCustomGetterObject", functionCreateCustomGetterObject, 0);
        addFunction(vm, "createBuiltin", functionCreateBuiltin, 2);
        addFunction(vm, "createGlobalObject", functionCreateGlobalObject, 0);
        addFunction(vm, "setImpureGetterDelegate", functionSetImpureGetterDelegate, 2);

        addFunction(vm, "dumpTypesForAllVariables", functionDumpTypesForAllVariables , 0);
        addFunction(vm, "findTypeForExpression", functionFindTypeForExpression, 2);
        addFunction(vm, "returnTypeFor", functionReturnTypeFor, 1);

        addFunction(vm, "dumpBasicBlockExecutionRanges", functionDumpBasicBlockExecutionRanges , 0);
        addFunction(vm, "hasBasicBlockExecuted", functionHasBasicBlockExecuted, 2);
        addFunction(vm, "basicBlockExecutionCount", functionBasicBlockExecutionCount, 2);

        addFunction(vm, "enableExceptionFuzz", functionEnableExceptionFuzz, 0);

        addFunction(vm, "drainMicrotasks", functionDrainMicrotasks, 0);

        addFunction(vm, "is32BitPlatform", functionIs32BitPlatform, 0);

#if ENABLE(WEBASSEMBLY)
        addFunction(vm, "loadWebAssembly", functionLoadWebAssembly, 3);
#endif
        addFunction(vm, "loadModule", functionLoadModule, 1);
        addFunction(vm, "checkModuleSyntax", functionCheckModuleSyntax, 1);

        addFunction(vm, "platformSupportsSamplingProfiler", functionPlatformSupportsSamplingProfiler, 0);
        addFunction(vm, "generateHeapSnapshot", functionGenerateHeapSnapshot, 0);
#if ENABLE(SAMPLING_PROFILER)
        addFunction(vm, "startSamplingProfiler", functionStartSamplingProfiler, 0);
        addFunction(vm, "samplingProfilerStackTraces", functionSamplingProfilerStackTraces, 0);
#endif

        if (!arguments.isEmpty()) {
            JSArray* array = constructEmptyArray(globalExec(), 0);
            for (size_t i = 0; i < arguments.size(); ++i)
                array->putDirectIndex(globalExec(), i, jsString(globalExec(), arguments[i]));
            putDirect(vm, Identifier::fromString(globalExec(), "arguments"), array);
        }

        putDirect(vm, Identifier::fromString(globalExec(), "console"), jsUndefined());
        putDirect(vm, vm.propertyNames->printPrivateName, JSFunction::create(vm, this, 1, vm.propertyNames->printPrivateName.string(), functionPrint));
    }

    void addFunction(VM& vm, const char* name, NativeFunction function, unsigned arguments)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        putDirect(vm, identifier, JSFunction::create(vm, this, arguments, identifier.string(), function));
    }
    
    void addConstructableFunction(VM& vm, const char* name, NativeFunction function, unsigned arguments)
    {
        Identifier identifier = Identifier::fromString(&vm, name);
        putDirect(vm, identifier, JSFunction::create(vm, this, arguments, identifier.string(), function, NoIntrinsic, function));
    }

    static JSInternalPromise* moduleLoaderResolve(JSGlobalObject*, ExecState*, JSValue, JSValue);
    static JSInternalPromise* moduleLoaderFetch(JSGlobalObject*, ExecState*, JSValue);
};

const ClassInfo GlobalObject::s_info = { "global", &JSGlobalObject::s_info, nullptr, CREATE_METHOD_TABLE(GlobalObject) };
const GlobalObjectMethodTable GlobalObject::s_globalObjectMethodTable = { &allowsAccessFrom, &supportsLegacyProfiling, &supportsRichSourceInfo, &shouldInterruptScript, &javaScriptRuntimeFlags, 0, &shouldInterruptScriptBeforeTimeout, &moduleLoaderResolve, &moduleLoaderFetch, nullptr, nullptr, nullptr, nullptr };


GlobalObject::GlobalObject(VM& vm, Structure* structure)
    : JSGlobalObject(vm, structure, &s_globalObjectMethodTable)
{
}

static UChar pathSeparator()
{
#if OS(WINDOWS)
    return '\\';
#else
    return '/';
#endif
}

struct DirectoryName {
    // In unix, it is "/". In Windows, it becomes a drive letter like "C:\"
    String rootName;

    // If the directory name is "/home/WebKit", this becomes "home/WebKit". If the directory name is "/", this becomes "".
    String queryName;
};

struct ModuleName {
    ModuleName(const String& moduleName);

    bool startsWithRoot() const
    {
        return !queries.isEmpty() && queries[0].isEmpty();
    }

    Vector<String> queries;
};

ModuleName::ModuleName(const String& moduleName)
{
    // A module name given from code is represented as the UNIX style path. Like, `./A/B.js`.
    moduleName.split('/', true, queries);
}

static bool extractDirectoryName(const String& absolutePathToFile, DirectoryName& directoryName)
{
    size_t firstSeparatorPosition = absolutePathToFile.find(pathSeparator());
    if (firstSeparatorPosition == notFound)
        return false;
    directoryName.rootName = absolutePathToFile.substring(0, firstSeparatorPosition + 1); // Include the separator.
    size_t lastSeparatorPosition = absolutePathToFile.reverseFind(pathSeparator());
    ASSERT_WITH_MESSAGE(lastSeparatorPosition != notFound, "If the separator is not found, this function already returns when performing the forward search.");
    if (firstSeparatorPosition == lastSeparatorPosition)
        directoryName.queryName = StringImpl::empty();
    else {
        size_t queryStartPosition = firstSeparatorPosition + 1;
        size_t queryLength = lastSeparatorPosition - queryStartPosition; // Not include the last separator.
        directoryName.queryName = absolutePathToFile.substring(queryStartPosition, queryLength);
    }
    return true;
}

static bool currentWorkingDirectory(DirectoryName& directoryName)
{
#if OS(WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364934.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#maxpath
    // The _MAX_PATH in Windows is 260. If the path of the current working directory is longer than that, _getcwd truncates the result.
    // And other I/O functions taking a path name also truncate it. To avoid this situation,
    //
    // (1). When opening the file in Windows for modules, we always use the abosolute path and add "\\?\" prefix to the path name.
    // (2). When retrieving the current working directory, use GetCurrentDirectory instead of _getcwd.
    //
    // In the path utility functions inside the JSC shell, we does not handle the UNC and UNCW including the network host name.
    DWORD bufferLength = ::GetCurrentDirectoryW(0, nullptr);
    if (!bufferLength)
        return false;
    // In Windows, wchar_t is the UTF-16LE.
    // https://msdn.microsoft.com/en-us/library/dd374081.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff381407.aspx
    auto buffer = std::make_unique<wchar_t[]>(bufferLength);
    DWORD lengthNotIncludingNull = ::GetCurrentDirectoryW(bufferLength, buffer.get());
    static_assert(sizeof(wchar_t) == sizeof(UChar), "In Windows, both are UTF-16LE");
    String directoryString = String(reinterpret_cast<UChar*>(buffer.get()));
    // We don't support network path like \\host\share\<path name>.
    if (directoryString.startsWith("\\\\"))
        return false;
#else
    auto buffer = std::make_unique<char[]>(PATH_MAX);
    if (!getcwd(buffer.get(), PATH_MAX))
        return false;
    String directoryString = String::fromUTF8(buffer.get());
#endif
    if (directoryString.isEmpty())
        return false;

    if (directoryString[directoryString.length() - 1] == pathSeparator())
        return extractDirectoryName(directoryString, directoryName);
    // Append the seperator to represents the file name. extractDirectoryName only accepts the absolute file name.
    return extractDirectoryName(makeString(directoryString, pathSeparator()), directoryName);
}

static String resolvePath(const DirectoryName& directoryName, const ModuleName& moduleName)
{
    Vector<String> directoryPieces;
    directoryName.queryName.split(pathSeparator(), false, directoryPieces);

    // Only first '/' is recognized as the path from the root.
    if (moduleName.startsWithRoot())
        directoryPieces.clear();

    for (const auto& query : moduleName.queries) {
        if (query == String(ASCIILiteral(".."))) {
            if (!directoryPieces.isEmpty())
                directoryPieces.removeLast();
        } else if (!query.isEmpty() && query != String(ASCIILiteral(".")))
            directoryPieces.append(query);
    }

    StringBuilder builder;
    builder.append(directoryName.rootName);
    for (size_t i = 0; i < directoryPieces.size(); ++i) {
        builder.append(directoryPieces[i]);
        if (i + 1 != directoryPieces.size())
            builder.append(pathSeparator());
    }
    return builder.toString();
}

JSInternalPromise* GlobalObject::moduleLoaderResolve(JSGlobalObject* globalObject, ExecState* exec, JSValue keyValue, JSValue referrerValue)
{
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    const Identifier key = keyValue.toPropertyKey(exec);
    if (exec->hadException()) {
        JSValue exception = exec->exception();
        exec->clearException();
        return deferred->reject(exec, exception);
    }

    if (key.isSymbol())
        return deferred->resolve(exec, keyValue);

    DirectoryName directoryName;
    if (referrerValue.isUndefined()) {
        if (!currentWorkingDirectory(directoryName))
            return deferred->reject(exec, createError(exec, ASCIILiteral("Could not resolve the current working directory.")));
    } else {
        const Identifier referrer = referrerValue.toPropertyKey(exec);
        if (exec->hadException()) {
            JSValue exception = exec->exception();
            exec->clearException();
            return deferred->reject(exec, exception);
        }
        if (referrer.isSymbol()) {
            if (!currentWorkingDirectory(directoryName))
                return deferred->reject(exec, createError(exec, ASCIILiteral("Could not resolve the current working directory.")));
        } else {
            // If the referrer exists, we assume that the referrer is the correct absolute path.
            if (!extractDirectoryName(referrer.impl(), directoryName))
                return deferred->reject(exec, createError(exec, makeString("Could not resolve the referrer name '", String(referrer.impl()), "'.")));
        }
    }

    return deferred->resolve(exec, jsString(exec, resolvePath(directoryName, ModuleName(key.impl()))));
}

static void convertShebangToJSComment(Vector<char>& buffer)
{
    if (buffer.size() >= 2) {
        if (buffer[0] == '#' && buffer[1] == '!')
            buffer[0] = buffer[1] = '/';
    }
}

static bool fillBufferWithContentsOfFile(FILE* file, Vector<char>& buffer)
{
    fseek(file, 0, SEEK_END);
    size_t bufferCapacity = ftell(file);
    fseek(file, 0, SEEK_SET);
    buffer.resize(bufferCapacity);
    size_t readSize = fread(buffer.data(), 1, buffer.size(), file);
    return readSize == buffer.size();
}

static bool fillBufferWithContentsOfFile(const String& fileName, Vector<char>& buffer)
{
    FILE* f = fopen(fileName.utf8().data(), "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.utf8().data());
        return false;
    }

    bool result = fillBufferWithContentsOfFile(f, buffer);
    fclose(f);

    return result;
}

static bool fetchScriptFromLocalFileSystem(const String& fileName, Vector<char>& buffer)
{
    if (!fillBufferWithContentsOfFile(fileName, buffer))
        return false;
    convertShebangToJSComment(buffer);
    return true;
}

static bool fetchModuleFromLocalFileSystem(const String& fileName, Vector<char>& buffer)
{
    // We assume that fileName is always an absolute path.
#if OS(WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#maxpath
    // Use long UNC to pass the long path name to the Windows APIs.
    String longUNCPathName = WTF::makeString("\\\\?\\", fileName);
    static_assert(sizeof(wchar_t) == sizeof(UChar), "In Windows, both are UTF-16LE");
    auto utf16Vector = longUNCPathName.charactersWithNullTermination();
    FILE* f = _wfopen(reinterpret_cast<wchar_t*>(utf16Vector.data()), L"rb");
#else
    FILE* f = fopen(fileName.utf8().data(), "r");
#endif
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.utf8().data());
        return false;
    }

    bool result = fillBufferWithContentsOfFile(f, buffer);
    if (result)
        convertShebangToJSComment(buffer);
    fclose(f);

    return result;
}

JSInternalPromise* GlobalObject::moduleLoaderFetch(JSGlobalObject* globalObject, ExecState* exec, JSValue key)
{
    JSInternalPromiseDeferred* deferred = JSInternalPromiseDeferred::create(exec, globalObject);
    String moduleKey = key.toString(exec)->value(exec);
    if (exec->hadException()) {
        JSValue exception = exec->exception();
        exec->clearException();
        return deferred->reject(exec, exception);
    }

    // Here, now we consider moduleKey as the fileName.
    Vector<char> utf8;
    if (!fetchModuleFromLocalFileSystem(moduleKey, utf8))
        return deferred->reject(exec, createError(exec, makeString("Could not open file '", moduleKey, "'.")));

    return deferred->resolve(exec, jsString(exec, stringFromUTF(utf8)));
}


EncodedJSValue JSC_HOST_CALL functionPrint(ExecState* exec)
{
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        if (i)
            putchar(' ');

        printf("%s", exec->uncheckedArgument(i).toString(exec)->view(exec).get().utf8().data());
    }

    putchar('\n');
    fflush(stdout);
    return JSValue::encode(jsUndefined());
}

#ifndef NDEBUG
EncodedJSValue JSC_HOST_CALL functionDumpCallFrame(ExecState* exec)
{
    VMEntryFrame* topVMEntryFrame = exec->vm().topVMEntryFrame;
    ExecState* callerFrame = exec->callerFrame(topVMEntryFrame);
    if (callerFrame)
        exec->vm().interpreter->dumpCallFrame(callerFrame);
    return JSValue::encode(jsUndefined());
}
#endif

EncodedJSValue JSC_HOST_CALL functionDebug(ExecState* exec)
{
    fprintf(stderr, "--> %s\n", exec->argument(0).toString(exec)->view(exec).get().utf8().data());
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDataLogValue(ExecState* exec)
{
    dataLog("value is: ", exec->argument(0), "\n");
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDescribe(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(jsString(exec, toString(exec->argument(0))));
}

EncodedJSValue JSC_HOST_CALL functionDescribeArray(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    JSObject* object = jsDynamicCast<JSObject*>(exec->argument(0));
    if (!object)
        return JSValue::encode(jsNontrivialString(exec, ASCIILiteral("<not object>")));
    return JSValue::encode(jsNontrivialString(exec, toString("<Public length: ", object->getArrayLength(), "; vector length: ", object->getVectorLength(), ">")));
}

class FunctionJSCStackFunctor {
public:
    FunctionJSCStackFunctor(StringBuilder& trace)
        : m_trace(trace)
    {
    }

    StackVisitor::Status operator()(StackVisitor& visitor) const
    {
        m_trace.append(String::format("    %zu   %s\n", visitor->index(), visitor->toString().utf8().data()));
        return StackVisitor::Continue;
    }

private:
    StringBuilder& m_trace;
};

EncodedJSValue JSC_HOST_CALL functionJSCStack(ExecState* exec)
{
    StringBuilder trace;
    trace.appendLiteral("--> Stack trace:\n");

    FunctionJSCStackFunctor functor(trace);
    exec->iterate(functor);
    fprintf(stderr, "%s", trace.toString().utf8().data());
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionCreateRoot(ExecState* exec)
{
    JSLockHolder lock(exec);
    return JSValue::encode(Root::create(exec->vm(), exec->lexicalGlobalObject()));
}

EncodedJSValue JSC_HOST_CALL functionCreateElement(ExecState* exec)
{
    JSLockHolder lock(exec);
    Root* root = jsDynamicCast<Root*>(exec->argument(0));
    if (!root)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(Element::create(exec->vm(), exec->lexicalGlobalObject(), root));
}

EncodedJSValue JSC_HOST_CALL functionGetElement(ExecState* exec)
{
    JSLockHolder lock(exec);
    Root* root = jsDynamicCast<Root*>(exec->argument(0));
    if (!root)
        return JSValue::encode(jsUndefined());
    Element* result = root->element();
    return JSValue::encode(result ? result : jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionSetElementRoot(ExecState* exec)
{
    JSLockHolder lock(exec);
    Element* element = jsDynamicCast<Element*>(exec->argument(0));
    Root* root = jsDynamicCast<Root*>(exec->argument(1));
    if (element && root)
        element->setRoot(exec->vm(), root);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionCreateSimpleObject(ExecState* exec)
{
    JSLockHolder lock(exec);
    return JSValue::encode(SimpleObject::create(exec->vm(), exec->lexicalGlobalObject()));
}

EncodedJSValue JSC_HOST_CALL functionGetHiddenValue(ExecState* exec)
{
    JSLockHolder lock(exec);
    SimpleObject* simpleObject = jsCast<SimpleObject*>(exec->argument(0).asCell());
    return JSValue::encode(simpleObject->hiddenValue());
}

EncodedJSValue JSC_HOST_CALL functionSetHiddenValue(ExecState* exec)
{
    JSLockHolder lock(exec);
    SimpleObject* simpleObject = jsCast<SimpleObject*>(exec->argument(0).asCell());
    JSValue value = exec->argument(1);
    simpleObject->setHiddenValue(exec->vm(), value);
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionCreateProxy(ExecState* exec)
{
    JSLockHolder lock(exec);
    JSValue target = exec->argument(0);
    if (!target.isObject())
        return JSValue::encode(jsUndefined());
    JSObject* jsTarget = asObject(target.asCell());
    Structure* structure = JSProxy::createStructure(exec->vm(), exec->lexicalGlobalObject(), jsTarget->getPrototypeDirect(), ImpureProxyType);
    JSProxy* proxy = JSProxy::create(exec->vm(), structure, jsTarget);
    return JSValue::encode(proxy);
}

EncodedJSValue JSC_HOST_CALL functionCreateRuntimeArray(ExecState* exec)
{
    JSLockHolder lock(exec);
    RuntimeArray* array = RuntimeArray::create(exec);
    return JSValue::encode(array);
}

EncodedJSValue JSC_HOST_CALL functionCreateImpureGetter(ExecState* exec)
{
    JSLockHolder lock(exec);
    JSValue target = exec->argument(0);
    JSObject* delegate = nullptr;
    if (target.isObject())
        delegate = asObject(target.asCell());
    Structure* structure = ImpureGetter::createStructure(exec->vm(), exec->lexicalGlobalObject(), jsNull());
    ImpureGetter* result = ImpureGetter::create(exec->vm(), structure, delegate);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionCreateCustomGetterObject(ExecState* exec)
{
    JSLockHolder lock(exec);
    Structure* structure = CustomGetter::createStructure(exec->vm(), exec->lexicalGlobalObject(), jsNull());
    CustomGetter* result = CustomGetter::create(exec->vm(), structure);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionSetImpureGetterDelegate(ExecState* exec)
{
    JSLockHolder lock(exec);
    JSValue base = exec->argument(0);
    if (!base.isObject())
        return JSValue::encode(jsUndefined());
    JSValue delegate = exec->argument(1);
    if (!delegate.isObject())
        return JSValue::encode(jsUndefined());
    ImpureGetter* impureGetter = jsCast<ImpureGetter*>(asObject(base.asCell()));
    impureGetter->setDelegate(exec->vm(), asObject(delegate.asCell()));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionGCAndSweep(ExecState* exec)
{
    JSLockHolder lock(exec);
    exec->heap()->collectAllGarbage();
    return JSValue::encode(jsNumber(exec->heap()->sizeAfterLastFullCollection()));
}

EncodedJSValue JSC_HOST_CALL functionFullGC(ExecState* exec)
{
    JSLockHolder lock(exec);
    exec->heap()->collect(FullCollection);
    return JSValue::encode(jsNumber(exec->heap()->sizeAfterLastFullCollection()));
}

EncodedJSValue JSC_HOST_CALL functionEdenGC(ExecState* exec)
{
    JSLockHolder lock(exec);
    exec->heap()->collect(EdenCollection);
    return JSValue::encode(jsNumber(exec->heap()->sizeAfterLastEdenCollection()));
}

EncodedJSValue JSC_HOST_CALL functionForceGCSlowPaths(ExecState*)
{
    // It's best for this to be the first thing called in the 
    // JS program so the option is set to true before we JIT.
    Options::forceGCSlowPaths() = true;
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionHeapSize(ExecState* exec)
{
    JSLockHolder lock(exec);
    return JSValue::encode(jsNumber(exec->heap()->size()));
}

// This function is not generally very helpful in 64-bit code as the tag and payload
// share a register. But in 32-bit JITed code the tag may not be checked if an
// optimization removes type checking requirements, such as in ===.
EncodedJSValue JSC_HOST_CALL functionAddressOf(ExecState* exec)
{
    JSValue value = exec->argument(0);
    if (!value.isCell())
        return JSValue::encode(jsUndefined());
    // Need to cast to uint64_t so bitwise_cast will play along.
    uint64_t asNumber = reinterpret_cast<uint64_t>(value.asCell());
    EncodedJSValue returnValue = JSValue::encode(jsNumber(bitwise_cast<double>(asNumber)));
    return returnValue;
}

static EncodedJSValue JSC_HOST_CALL functionGetGetterSetter(ExecState* exec)
{
    JSValue value = exec->argument(0);
    if (!value.isObject())
        return JSValue::encode(jsUndefined());

    JSValue property = exec->argument(1);
    if (!property.isString())
        return JSValue::encode(jsUndefined());

    Identifier ident = Identifier::fromString(&exec->vm(), property.toString(exec)->value(exec));

    PropertySlot slot(value, PropertySlot::InternalMethodType::VMInquiry);
    value.getPropertySlot(exec, ident, slot);

    JSValue result;
    if (slot.isCacheableGetter())
        result = slot.getterSetter();
    else
        result = jsNull();

    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionVersion(ExecState*)
{
    // We need this function for compatibility with the Mozilla JS tests but for now
    // we don't actually do any version-specific handling
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionRun(ExecState* exec)
{
    String fileName = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Could not open file."))));

    GlobalObject* globalObject = GlobalObject::create(exec->vm(), GlobalObject::createStructure(exec->vm(), jsNull()), Vector<String>());

    JSArray* array = constructEmptyArray(globalObject->globalExec(), 0);
    for (unsigned i = 1; i < exec->argumentCount(); ++i)
        array->putDirectIndex(globalObject->globalExec(), i - 1, exec->uncheckedArgument(i));
    globalObject->putDirect(
        exec->vm(), Identifier::fromString(globalObject->globalExec(), "arguments"), array);

    NakedPtr<Exception> exception;
    StopWatch stopWatch;
    stopWatch.start();
    evaluate(globalObject->globalExec(), jscSource(script, fileName), JSValue(), exception);
    stopWatch.stop();

    if (exception) {
        exec->vm().throwException(globalObject->globalExec(), exception);
        return JSValue::encode(jsUndefined());
    }
    
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

EncodedJSValue JSC_HOST_CALL functionLoad(ExecState* exec)
{
    String fileName = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Could not open file."))));

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    
    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(globalObject->globalExec(), jscSource(script, fileName), JSValue(), evaluationException);
    if (evaluationException)
        exec->vm().throwException(exec, evaluationException);
    return JSValue::encode(result);
}

EncodedJSValue JSC_HOST_CALL functionReadFile(ExecState* exec)
{
    String fileName = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    Vector<char> script;
    if (!fillBufferWithContentsOfFile(fileName, script))
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Could not open file."))));

    return JSValue::encode(jsString(exec, stringFromUTF(script)));
}

EncodedJSValue JSC_HOST_CALL functionCheckSyntax(ExecState* exec)
{
    String fileName = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Could not open file."))));

    JSGlobalObject* globalObject = exec->lexicalGlobalObject();

    StopWatch stopWatch;
    stopWatch.start();

    JSValue syntaxException;
    bool validSyntax = checkSyntax(globalObject->globalExec(), jscSource(script, fileName), &syntaxException);
    stopWatch.stop();

    if (!validSyntax)
        exec->vm().throwException(exec, syntaxException);
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

#if ENABLE(SAMPLING_FLAGS)
EncodedJSValue JSC_HOST_CALL functionSetSamplingFlags(ExecState* exec)
{
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        unsigned flag = static_cast<unsigned>(exec->uncheckedArgument(i).toNumber(exec));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::setFlag(flag);
    }
    return JSValue::encode(jsNull());
}

EncodedJSValue JSC_HOST_CALL functionClearSamplingFlags(ExecState* exec)
{
    for (unsigned i = 0; i < exec->argumentCount(); ++i) {
        unsigned flag = static_cast<unsigned>(exec->uncheckedArgument(i).toNumber(exec));
        if ((flag >= 1) && (flag <= 32))
            SamplingFlags::clearFlag(flag);
    }
    return JSValue::encode(jsNull());
}
#endif

EncodedJSValue JSC_HOST_CALL functionShadowChickenFunctionsOnStack(ExecState* exec)
{
    return JSValue::encode(exec->vm().shadowChicken().functionsOnStack(exec));
}

EncodedJSValue JSC_HOST_CALL functionReadline(ExecState* exec)
{
    Vector<char, 256> line;
    int c;
    while ((c = getchar()) != EOF) {
        // FIXME: Should we also break on \r? 
        if (c == '\n')
            break;
        line.append(c);
    }
    line.append('\0');
    return JSValue::encode(jsString(exec, line.data()));
}

EncodedJSValue JSC_HOST_CALL functionPreciseTime(ExecState*)
{
    return JSValue::encode(jsNumber(currentTime()));
}

EncodedJSValue JSC_HOST_CALL functionNeverInlineFunction(ExecState* exec)
{
    return JSValue::encode(setNeverInline(exec));
}

EncodedJSValue JSC_HOST_CALL functionNoDFG(ExecState* exec)
{
    return JSValue::encode(setNeverOptimize(exec));
}

EncodedJSValue JSC_HOST_CALL functionOptimizeNextInvocation(ExecState* exec)
{
    return JSValue::encode(optimizeNextInvocation(exec));
}

EncodedJSValue JSC_HOST_CALL functionNumberOfDFGCompiles(ExecState* exec)
{
    return JSValue::encode(numberOfDFGCompiles(exec));
}

EncodedJSValue JSC_HOST_CALL functionReoptimizationRetryCount(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(jsUndefined());
    
    CodeBlock* block = getSomeBaselineCodeBlockForFunction(exec->argument(0));
    if (!block)
        return JSValue::encode(jsNumber(0));
    
    return JSValue::encode(jsNumber(block->reoptimizationRetryCounter()));
}

EncodedJSValue JSC_HOST_CALL functionTransferArrayBuffer(ExecState* exec)
{
    if (exec->argumentCount() < 1)
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Not enough arguments"))));
    
    JSArrayBuffer* buffer = jsDynamicCast<JSArrayBuffer*>(exec->argument(0));
    if (!buffer)
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Expected an array buffer"))));
    
    ArrayBufferContents dummyContents;
    buffer->impl()->transfer(dummyContents);
    
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionFailNextNewCodeBlock(ExecState* exec)
{
    exec->vm().setFailNextNewCodeBlock();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionQuit(ExecState*)
{
    jscExit(EXIT_SUCCESS);

#if COMPILER(MSVC)
    // Without this, Visual Studio will complain that this method does not return a value.
    return JSValue::encode(jsUndefined());
#endif
}

EncodedJSValue JSC_HOST_CALL functionAbort(ExecState*)
{
    CRASH();
}

EncodedJSValue JSC_HOST_CALL functionFalse1(ExecState*) { return JSValue::encode(jsBoolean(false)); }
EncodedJSValue JSC_HOST_CALL functionFalse2(ExecState*) { return JSValue::encode(jsBoolean(false)); }

EncodedJSValue JSC_HOST_CALL functionUndefined1(ExecState*) { return JSValue::encode(jsUndefined()); }
EncodedJSValue JSC_HOST_CALL functionUndefined2(ExecState*) { return JSValue::encode(jsUndefined()); }
EncodedJSValue JSC_HOST_CALL functionIsInt32(ExecState* exec)
{
    for (size_t i = 0; i < exec->argumentCount(); ++i) {
        if (!exec->argument(i).isInt32())
            return JSValue::encode(jsBoolean(false));
    }
    return JSValue::encode(jsBoolean(true));
}

EncodedJSValue JSC_HOST_CALL functionIdentity(ExecState* exec) { return JSValue::encode(exec->argument(0)); }

EncodedJSValue JSC_HOST_CALL functionEffectful42(ExecState*)
{
    return JSValue::encode(jsNumber(42));
}

EncodedJSValue JSC_HOST_CALL functionMakeMasquerader(ExecState* exec)
{
    return JSValue::encode(Masquerader::create(exec->vm(), exec->lexicalGlobalObject()));
}

EncodedJSValue JSC_HOST_CALL functionHasCustomProperties(ExecState* exec)
{
    JSValue value = exec->argument(0);
    if (value.isObject())
        return JSValue::encode(jsBoolean(asObject(value)->hasCustomProperties()));
    return JSValue::encode(jsBoolean(false));
}

EncodedJSValue JSC_HOST_CALL functionDumpTypesForAllVariables(ExecState* exec)
{
    exec->vm().dumpTypeProfilerData();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionFindTypeForExpression(ExecState* exec)
{
    RELEASE_ASSERT(exec->vm().typeProfiler());
    exec->vm().typeProfilerLog()->processLogEntries(ASCIILiteral("jsc Testing API: functionFindTypeForExpression"));

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(exec->argument(1).isString());
    String substring = exec->argument(1).getString(exec);
    String sourceCodeText = executable->source().view().toString();
    unsigned offset = static_cast<unsigned>(sourceCodeText.find(substring) + executable->source().startOffset());
    
    String jsonString = exec->vm().typeProfiler()->typeInformationForExpressionAtOffset(TypeProfilerSearchDescriptorNormal, offset, executable->sourceID(), exec->vm());
    return JSValue::encode(JSONParse(exec, jsonString));
}

EncodedJSValue JSC_HOST_CALL functionReturnTypeFor(ExecState* exec)
{
    RELEASE_ASSERT(exec->vm().typeProfiler());
    exec->vm().typeProfilerLog()->processLogEntries(ASCIILiteral("jsc Testing API: functionReturnTypeFor"));

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

    unsigned offset = executable->typeProfilingStartOffset();
    String jsonString = exec->vm().typeProfiler()->typeInformationForExpressionAtOffset(TypeProfilerSearchDescriptorFunctionReturn, offset, executable->sourceID(), exec->vm());
    return JSValue::encode(JSONParse(exec, jsonString));
}

EncodedJSValue JSC_HOST_CALL functionDumpBasicBlockExecutionRanges(ExecState* exec)
{
    RELEASE_ASSERT(exec->vm().controlFlowProfiler());
    exec->vm().controlFlowProfiler()->dumpData();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionHasBasicBlockExecuted(ExecState* exec)
{
    RELEASE_ASSERT(exec->vm().controlFlowProfiler());

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(exec->argument(1).isString());
    String substring = exec->argument(1).getString(exec);
    String sourceCodeText = executable->source().view().toString();
    RELEASE_ASSERT(sourceCodeText.contains(substring));
    int offset = sourceCodeText.find(substring) + executable->source().startOffset();
    
    bool hasExecuted = exec->vm().controlFlowProfiler()->hasBasicBlockAtTextOffsetBeenExecuted(offset, executable->sourceID(), exec->vm());
    return JSValue::encode(jsBoolean(hasExecuted));
}

EncodedJSValue JSC_HOST_CALL functionBasicBlockExecutionCount(ExecState* exec)
{
    RELEASE_ASSERT(exec->vm().controlFlowProfiler());

    JSValue functionValue = exec->argument(0);
    RELEASE_ASSERT(functionValue.isFunction());
    FunctionExecutable* executable = (jsDynamicCast<JSFunction*>(functionValue.asCell()->getObject()))->jsExecutable();

    RELEASE_ASSERT(exec->argument(1).isString());
    String substring = exec->argument(1).getString(exec);
    String sourceCodeText = executable->source().view().toString();
    RELEASE_ASSERT(sourceCodeText.contains(substring));
    int offset = sourceCodeText.find(substring) + executable->source().startOffset();
    
    size_t executionCount = exec->vm().controlFlowProfiler()->basicBlockExecutionCountAtTextOffset(offset, executable->sourceID(), exec->vm());
    return JSValue::encode(JSValue(executionCount));
}

EncodedJSValue JSC_HOST_CALL functionEnableExceptionFuzz(ExecState*)
{
    Options::useExceptionFuzz() = true;
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionDrainMicrotasks(ExecState* exec)
{
    exec->vm().drainMicrotasks();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionIs32BitPlatform(ExecState*)
{
#if USE(JSVALUE64)
    return JSValue::encode(JSValue(JSC::JSValue::JSFalse));
#else
    return JSValue::encode(JSValue(JSC::JSValue::JSTrue));
#endif
}

#if ENABLE(WEBASSEMBLY)
EncodedJSValue JSC_HOST_CALL functionLoadWebAssembly(ExecState* exec)
{
    String fileName = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    Vector<char> buffer;
    if (!fillBufferWithContentsOfFile(fileName, buffer))
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Could not open file."))));
    RefPtr<WebAssemblySourceProvider> sourceProvider = WebAssemblySourceProvider::create(reinterpret_cast<Vector<uint8_t>&>(buffer), fileName);
    SourceCode source(sourceProvider);
    JSObject* imports = exec->argument(1).getObject();
    JSArrayBuffer* arrayBuffer = jsDynamicCast<JSArrayBuffer*>(exec->argument(2));

    String errorMessage;
    JSWASMModule* module = parseWebAssembly(exec, source, imports, arrayBuffer, errorMessage);
    if (!module)
        return JSValue::encode(exec->vm().throwException(exec, createSyntaxError(exec, errorMessage)));
    return JSValue::encode(module);
}
#endif

EncodedJSValue JSC_HOST_CALL functionLoadModule(ExecState* exec)
{
    String fileName = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(fileName, script))
        return JSValue::encode(exec->vm().throwException(exec, createError(exec, ASCIILiteral("Could not open file."))));

    JSInternalPromise* promise = loadAndEvaluateModule(exec, fileName);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSValue error;
    JSFunction* errorHandler = JSNativeStdFunction::create(exec->vm(), exec->lexicalGlobalObject(), 1, String(), [&](ExecState* exec) {
        error = exec->argument(0);
        return JSValue::encode(jsUndefined());
    });

    promise->then(exec, nullptr, errorHandler);
    exec->vm().drainMicrotasks();
    if (error)
        return JSValue::encode(exec->vm().throwException(exec, error));
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionCreateBuiltin(ExecState* exec)
{
    if (exec->argumentCount() < 1 || !exec->argument(0).isString())
        return JSValue::encode(jsUndefined());

    String functionText = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(JSValue());

    VM& vm = exec->vm();
    const SourceCode& source = makeSource(functionText);
    JSFunction* func = JSFunction::createBuiltinFunction(vm, createBuiltinExecutable(vm, source, Identifier::fromString(&vm, "foo"), ConstructorKind::None, ConstructAbility::CannotConstruct)->link(vm, source), exec->lexicalGlobalObject());

    return JSValue::encode(func);
}

EncodedJSValue JSC_HOST_CALL functionCreateGlobalObject(ExecState* exec)
{
    VM& vm = exec->vm();
    return JSValue::encode(GlobalObject::create(vm, GlobalObject::createStructure(vm, jsNull()), Vector<String>()));
}

EncodedJSValue JSC_HOST_CALL functionCheckModuleSyntax(ExecState* exec)
{
    String source = exec->argument(0).toString(exec)->value(exec);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    StopWatch stopWatch;
    stopWatch.start();

    ParserError error;
    bool validSyntax = checkModuleSyntax(exec, makeSource(source), error);
    stopWatch.stop();

    if (!validSyntax)
        exec->vm().throwException(exec, jsNontrivialString(exec, toString("SyntaxError: ", error.message(), ":", error.line())));
    return JSValue::encode(jsNumber(stopWatch.getElapsedMS()));
}

EncodedJSValue JSC_HOST_CALL functionPlatformSupportsSamplingProfiler(ExecState*)
{
#if ENABLE(SAMPLING_PROFILER)
    return JSValue::encode(JSValue(JSC::JSValue::JSTrue));
#else
    return JSValue::encode(JSValue(JSC::JSValue::JSFalse));
#endif
}

EncodedJSValue JSC_HOST_CALL functionGenerateHeapSnapshot(ExecState* exec)
{
    JSLockHolder lock(exec);

    HeapSnapshotBuilder snapshotBuilder(exec->vm().ensureHeapProfiler());
    snapshotBuilder.buildSnapshot();

    String jsonString = snapshotBuilder.json();
    EncodedJSValue result = JSValue::encode(JSONParse(exec, jsonString));
    RELEASE_ASSERT(!exec->hadException());
    return result;
}

#if ENABLE(SAMPLING_PROFILER)
EncodedJSValue JSC_HOST_CALL functionStartSamplingProfiler(ExecState* exec)
{
    exec->vm().ensureSamplingProfiler(WTF::Stopwatch::create());
    exec->vm().samplingProfiler()->noticeCurrentThreadAsJSCExecutionThread();
    exec->vm().samplingProfiler()->start();
    return JSValue::encode(jsUndefined());
}

EncodedJSValue JSC_HOST_CALL functionSamplingProfilerStackTraces(ExecState* exec)
{
    RELEASE_ASSERT(exec->vm().samplingProfiler());
    String jsonString = exec->vm().samplingProfiler()->stackTracesAsJSON();
    EncodedJSValue result = JSValue::encode(JSONParse(exec, jsonString));
    RELEASE_ASSERT(!exec->hadException());
    return result;
}
#endif // ENABLE(SAMPLING_PROFILER)

// Use SEH for Release builds only to get rid of the crash report dialog
// (luckily the same tests fail in Release and Debug builds so far). Need to
// be in a separate main function because the jscmain function requires object
// unwinding.

#if COMPILER(MSVC) && !defined(_DEBUG)
#define TRY       __try {
#define EXCEPT(x) } __except (EXCEPTION_EXECUTE_HANDLER) { x; }
#else
#define TRY
#define EXCEPT(x)
#endif

int jscmain(int argc, char** argv);

static double s_desiredTimeout;

static NO_RETURN_DUE_TO_CRASH void timeoutThreadMain(void*)
{
    auto timeout = std::chrono::microseconds(static_cast<std::chrono::microseconds::rep>(s_desiredTimeout * 1000000));
    std::this_thread::sleep_for(timeout);
    
    dataLog("Timed out after ", s_desiredTimeout, " seconds!\n");
    CRASH();
}

int main(int argc, char** argv)
{
#if PLATFORM(IOS) && CPU(ARM_THUMB2)
    // Enabled IEEE754 denormal support.
    fenv_t env;
    fegetenv( &env );
    env.__fpscr &= ~0x01000000u;
    fesetenv( &env );
#endif

#if OS(WINDOWS)
    // Cygwin calls ::SetErrorMode(SEM_FAILCRITICALERRORS), which we will inherit. This is bad for
    // testing/debugging, as it causes the post-mortem debugger not to be invoked. We reset the
    // error mode here to work around Cygwin's behavior. See <http://webkit.org/b/55222>.
    ::SetErrorMode(0);

#if defined(_DEBUG)
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
#endif

    timeBeginPeriod(1);
#endif

#if PLATFORM(EFL)
    ecore_init();
#endif

    // Need to initialize WTF threading before we start any threads. Cannot initialize JSC
    // threading yet, since that would do somethings that we'd like to defer until after we
    // have a chance to parse options.
    WTF::initializeThreading();

    if (char* timeoutString = getenv("JSCTEST_timeout")) {
        if (sscanf(timeoutString, "%lf", &s_desiredTimeout) != 1) {
            dataLog(
                "WARNING: timeout string is malformed, got ", timeoutString,
                " but expected a number. Not using a timeout.\n");
        } else
            createThread(timeoutThreadMain, 0, "jsc Timeout Thread");
    }

#if PLATFORM(IOS)
    Options::crashIfCantAllocateJITMemory() = true;
#endif

    // We can't use destructors in the following code because it uses Windows
    // Structured Exception Handling
    int res = 0;
    TRY
        res = jscmain(argc, argv);
    EXCEPT(res = 3)
    if (Options::logHeapStatisticsAtExit())
        HeapStatistics::reportSuccess();

#if PLATFORM(EFL)
    ecore_shutdown();
#endif

    jscExit(res);
}

static void dumpException(GlobalObject* globalObject, JSValue exception)
{
    printf("Exception: %s\n", exception.toString(globalObject->globalExec())->value(globalObject->globalExec()).utf8().data());
    Identifier stackID = Identifier::fromString(globalObject->globalExec(), "stack");
    JSValue stackValue = exception.get(globalObject->globalExec(), stackID);
    if (!stackValue.isUndefinedOrNull())
        printf("%s\n", stackValue.toString(globalObject->globalExec())->value(globalObject->globalExec()).utf8().data());
}

static void dumpException(GlobalObject* globalObject, NakedPtr<Exception> evaluationException)
{
    if (evaluationException)
        dumpException(globalObject, evaluationException->value());
}

static bool runWithScripts(GlobalObject* globalObject, const Vector<Script>& scripts, bool dump, bool module)
{
    String fileName;
    Vector<char> scriptBuffer;

    if (dump)
        JSC::Options::dumpGeneratedBytecodes() = true;

    VM& vm = globalObject->vm();
    bool success = true;

    JSFunction* errorHandler = JSNativeStdFunction::create(vm, globalObject, 1, String(), [&](ExecState* exec) {
        success = false;
        dumpException(globalObject, exec->argument(0));
        return JSValue::encode(jsUndefined());
    });

#if ENABLE(SAMPLING_FLAGS)
    SamplingFlags::start();
#endif

    for (size_t i = 0; i < scripts.size(); i++) {
        JSInternalPromise* promise = nullptr;
        if (scripts[i].isFile) {
            fileName = scripts[i].argument;
            if (module)
                promise = loadAndEvaluateModule(globalObject->globalExec(), fileName);
            else {
                if (!fetchScriptFromLocalFileSystem(fileName, scriptBuffer))
                    return false; // fail early so we can catch missing files
            }
        } else {
            size_t commandLineLength = strlen(scripts[i].argument);
            scriptBuffer.resize(commandLineLength);
            std::copy(scripts[i].argument, scripts[i].argument + commandLineLength, scriptBuffer.begin());
            fileName = ASCIILiteral("[Command Line]");
        }

        if (module) {
            if (!promise)
                promise = loadAndEvaluateModule(globalObject->globalExec(), jscSource(scriptBuffer, fileName));
            globalObject->globalExec()->clearException();
            promise->then(globalObject->globalExec(), nullptr, errorHandler);
            globalObject->vm().drainMicrotasks();
        } else {
            NakedPtr<Exception> evaluationException;
            JSValue returnValue = evaluate(globalObject->globalExec(), jscSource(scriptBuffer, fileName), JSValue(), evaluationException);
            success = success && !evaluationException;
            if (dump && !evaluationException)
                printf("End: %s\n", returnValue.toString(globalObject->globalExec())->value(globalObject->globalExec()).utf8().data());
            dumpException(globalObject, evaluationException);
        }

        globalObject->globalExec()->clearException();
    }

#if ENABLE(REGEXP_TRACING)
    vm.dumpRegExpTrace();
#endif
    return success;
}

#define RUNNING_FROM_XCODE 0

static void runInteractive(GlobalObject* globalObject)
{
    String interpreterName(ASCIILiteral("Interpreter"));
    
    bool shouldQuit = false;
    while (!shouldQuit) {
#if HAVE(READLINE) && !RUNNING_FROM_XCODE
        ParserError error;
        String source;
        do {
            error = ParserError();
            char* line = readline(source.isEmpty() ? interactivePrompt : "... ");
            shouldQuit = !line;
            if (!line)
                break;
            source = source + line;
            source = source + '\n';
            checkSyntax(globalObject->vm(), makeSource(source, interpreterName), error);
            if (!line[0])
                break;
            add_history(line);
        } while (error.syntaxErrorType() == ParserError::SyntaxErrorRecoverable);
        
        if (error.isValid()) {
            printf("%s:%d\n", error.message().utf8().data(), error.line());
            continue;
        }
        
        
        NakedPtr<Exception> evaluationException;
        JSValue returnValue = evaluate(globalObject->globalExec(), makeSource(source, interpreterName), JSValue(), evaluationException);
#else
        printf("%s", interactivePrompt);
        Vector<char, 256> line;
        int c;
        while ((c = getchar()) != EOF) {
            // FIXME: Should we also break on \r? 
            if (c == '\n')
                break;
            line.append(c);
        }
        if (line.isEmpty())
            break;

        NakedPtr<Exception> evaluationException;
        JSValue returnValue = evaluate(globalObject->globalExec(), jscSource(line, interpreterName), JSValue(), evaluationException);
#endif
        if (evaluationException)
            printf("Exception: %s\n", evaluationException->value().toString(globalObject->globalExec())->value(globalObject->globalExec()).utf8().data());
        else
            printf("%s\n", returnValue.toString(globalObject->globalExec())->value(globalObject->globalExec()).utf8().data());

        globalObject->globalExec()->clearException();
        globalObject->vm().drainMicrotasks();
    }
    printf("\n");
}

static NO_RETURN void printUsageStatement(bool help = false)
{
    fprintf(stderr, "Usage: jsc [options] [files] [-- arguments]\n");
    fprintf(stderr, "  -d         Dumps bytecode (debug builds only)\n");
    fprintf(stderr, "  -e         Evaluate argument as script code\n");
    fprintf(stderr, "  -f         Specifies a source file (deprecated)\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  -i         Enables interactive mode (default if no files are specified)\n");
    fprintf(stderr, "  -m         Execute as a module\n");
#if HAVE(SIGNAL_H)
    fprintf(stderr, "  -s         Installs signal handlers that exit on a crash (Unix platforms only)\n");
#endif
    fprintf(stderr, "  -p <file>  Outputs profiling data to a file\n");
    fprintf(stderr, "  -x         Output exit code before terminating\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  --options                  Dumps all JSC VM options and exits\n");
    fprintf(stderr, "  --dumpOptions              Dumps all JSC VM options before continuing\n");
    fprintf(stderr, "  --<jsc VM option>=<value>  Sets the specified JSC VM option\n");
    fprintf(stderr, "\n");

    jscExit(help ? EXIT_SUCCESS : EXIT_FAILURE);
}

void CommandLine::parseArguments(int argc, char** argv)
{
    Options::initialize();
    
    int i = 1;
    bool needToDumpOptions = false;
    bool needToExit = false;

    bool hasBadJSCOptions = false;
    for (; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-f")) {
            if (++i == argc)
                printUsageStatement();
            m_scripts.append(Script(true, argv[i]));
            continue;
        }
        if (!strcmp(arg, "-e")) {
            if (++i == argc)
                printUsageStatement();
            m_scripts.append(Script(false, argv[i]));
            continue;
        }
        if (!strcmp(arg, "-i")) {
            m_interactive = true;
            continue;
        }
        if (!strcmp(arg, "-d")) {
            m_dump = true;
            continue;
        }
        if (!strcmp(arg, "-p")) {
            if (++i == argc)
                printUsageStatement();
            m_profile = true;
            m_profilerOutput = argv[i];
            continue;
        }
        if (!strcmp(arg, "-m")) {
            m_module = true;
            continue;
        }
        if (!strcmp(arg, "-s")) {
#if HAVE(SIGNAL_H)
            signal(SIGILL, _exit);
            signal(SIGFPE, _exit);
            signal(SIGBUS, _exit);
            signal(SIGSEGV, _exit);
#endif
            continue;
        }
        if (!strcmp(arg, "-x")) {
            m_exitCode = true;
            continue;
        }
        if (!strcmp(arg, "--")) {
            ++i;
            break;
        }
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            printUsageStatement(true);

        if (!strcmp(arg, "--options")) {
            needToDumpOptions = true;
            needToExit = true;
            continue;
        }
        if (!strcmp(arg, "--dumpOptions")) {
            needToDumpOptions = true;
            continue;
        }
        if (!strcmp(arg, "--reportSamplingProfilerData")) {
            JSC::Options::useSamplingProfiler() = true;
            JSC::Options::collectSamplingProfilerDataForJSCShell() = true;
            m_dumpSamplingProfilerData = true;
            continue;
        }

        // See if the -- option is a JSC VM option.
        if (strstr(arg, "--") == arg) {
            if (!JSC::Options::setOption(&arg[2])) {
                hasBadJSCOptions = true;
                dataLog("ERROR: invalid option: ", arg, "\n");
            }
            continue;
        }

        // This arg is not recognized by the VM nor by jsc. Pass it on to the
        // script.
        m_scripts.append(Script(true, argv[i]));
    }

    if (hasBadJSCOptions && JSC::Options::validateOptions())
        CRASH();

    if (m_scripts.isEmpty())
        m_interactive = true;

    for (; i < argc; ++i)
        m_arguments.append(argv[i]);

    if (needToDumpOptions)
        JSC::Options::dumpAllOptions(stderr, JSC::Options::DumpLevel::Overridden, "All JSC runtime options:");
    JSC::Options::ensureOptionsAreCoherent();
    if (needToExit)
        jscExit(EXIT_SUCCESS);
}

// We make this function no inline so that globalObject won't be on the stack if we do a GC in jscmain.
static int NEVER_INLINE runJSC(VM* vm, CommandLine options)
{
    JSLockHolder locker(vm);

    int result;
    if (options.m_profile && !vm->m_perBytecodeProfiler)
        vm->m_perBytecodeProfiler = std::make_unique<Profiler::Database>(*vm);

    GlobalObject* globalObject = GlobalObject::create(*vm, GlobalObject::createStructure(*vm, jsNull()), options.m_arguments);
    bool success = runWithScripts(globalObject, options.m_scripts, options.m_dump, options.m_module);
    if (options.m_interactive && success)
        runInteractive(globalObject);

    result = success ? 0 : 3;

    if (options.m_exitCode)
        printf("jsc exiting %d\n", result);

    if (options.m_profile) {
        if (!vm->m_perBytecodeProfiler->save(options.m_profilerOutput.utf8().data()))
            fprintf(stderr, "could not save profiler output.\n");
    }

#if ENABLE(JIT)
    if (Options::useExceptionFuzz())
        printf("JSC EXCEPTION FUZZ: encountered %u checks.\n", numberOfExceptionFuzzChecks());
    bool fireAtEnabled =
    Options::fireExecutableAllocationFuzzAt() || Options::fireExecutableAllocationFuzzAtOrAfter();
    if (Options::useExecutableAllocationFuzz() && (!fireAtEnabled || Options::verboseExecutableAllocationFuzz()))
        printf("JSC EXECUTABLE ALLOCATION FUZZ: encountered %u checks.\n", numberOfExecutableAllocationFuzzChecks());
    if (Options::useOSRExitFuzz()) {
        printf("JSC OSR EXIT FUZZ: encountered %u static checks.\n", numberOfStaticOSRExitFuzzChecks());
        printf("JSC OSR EXIT FUZZ: encountered %u dynamic checks.\n", numberOfOSRExitFuzzChecks());
    }
#endif
    auto compileTimeStats = DFG::Plan::compileTimeStats();
    Vector<CString> compileTimeKeys;
    for (auto& entry : compileTimeStats)
        compileTimeKeys.append(entry.key);
    std::sort(compileTimeKeys.begin(), compileTimeKeys.end());
    for (CString key : compileTimeKeys)
        printf("%40s: %.3lf ms\n", key.data(), compileTimeStats.get(key));

    return result;
}

int jscmain(int argc, char** argv)
{
    // Note that the options parsing can affect VM creation, and thus
    // comes first.
    CommandLine options(argc, argv);

    // Initialize JSC before getting VM.
#if ENABLE(SAMPLING_REGIONS)
    WTF::initializeMainThread();
#endif
    JSC::initializeThreading();

    VM* vm = &VM::create(LargeHeap).leakRef();
    int result;
    result = runJSC(vm, options);

    if (Options::gcAtEnd()) {
        // We need to hold the API lock to do a GC.
        JSLockHolder locker(vm);
        vm->heap.collectAllGarbage();
    }

    if (options.m_dumpSamplingProfilerData) {
#if ENABLE(SAMPLING_PROFILER)
        JSLockHolder locker(vm);
        vm->samplingProfiler()->reportTopFunctions();
        vm->samplingProfiler()->reportTopBytecodes();
#else
        dataLog("Sampling profiler is not enabled on this platform\n");
#endif
    }

    printSuperSamplerState();

    return result;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif
