/*
 * Copyright (C) 2008, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSGlobalData.h"

#include "ArgList.h"
#include "Heap.h"
#include "CommonIdentifiers.h"
#include "DebuggerActivation.h"
#include "FunctionConstructor.h"
#include "GetterSetter.h"
#include "Interpreter.h"
#include "JSActivation.h"
#include "JSAPIValueWrapper.h"
#include "JSArray.h"
#include "JSByteArray.h"
#include "JSClassRef.h"
#include "JSFunction.h"
#include "JSLock.h"
#include "JSNotAnObject.h"
#include "JSPropertyNameIterator.h"
#include "JSStaticScopeObject.h"
#include "Lexer.h"
#include "Lookup.h"
#include "Nodes.h"
#include "ParserArena.h"
#if ENABLE(REGEXP_TRACING)
#include "RegExp.h"
#endif
#include "RegExpCache.h"
#include "RegExpObject.h"
#include "StrictEvalActivation.h"
#include "StrongInlines.h"
#include <wtf/Threading.h>
#include <wtf/WTFThreadData.h>
#if PLATFORM(MAC)
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace WTF;

namespace {

using namespace JSC;

class Recompiler : public MarkedBlock::VoidFunctor {
public:
    void operator()(JSCell*);
};

inline void Recompiler::operator()(JSCell* cell)
{
    if (!cell->inherits(&JSFunction::s_info))
        return;
    JSFunction* function = asFunction(cell);
    if (!function->executable() || function->executable()->isHostFunction())
        return;
    function->jsExecutable()->discardCode();
}

} // namespace

namespace JSC {

#if ENABLE(ASSEMBLER) && (CPU(X86) || CPU(X86_64))
const uint64_t MacroAssemblerX86Common::s_maskSignBit = 0x7FFFFFFFFFFFFFFFull;
#endif

extern JSC_CONST_HASHTABLE HashTable arrayConstructorTable;
extern JSC_CONST_HASHTABLE HashTable arrayPrototypeTable;
extern JSC_CONST_HASHTABLE HashTable booleanPrototypeTable;
extern JSC_CONST_HASHTABLE HashTable jsonTable;
extern JSC_CONST_HASHTABLE HashTable dateTable;
extern JSC_CONST_HASHTABLE HashTable dateConstructorTable;
extern JSC_CONST_HASHTABLE HashTable errorPrototypeTable;
extern JSC_CONST_HASHTABLE HashTable globalObjectTable;
extern JSC_CONST_HASHTABLE HashTable mathTable;
extern JSC_CONST_HASHTABLE HashTable numberConstructorTable;
extern JSC_CONST_HASHTABLE HashTable numberPrototypeTable;
extern JSC_CONST_HASHTABLE HashTable objectConstructorTable;
extern JSC_CONST_HASHTABLE HashTable objectPrototypeTable;
extern JSC_CONST_HASHTABLE HashTable regExpTable;
extern JSC_CONST_HASHTABLE HashTable regExpConstructorTable;
extern JSC_CONST_HASHTABLE HashTable regExpPrototypeTable;
extern JSC_CONST_HASHTABLE HashTable stringTable;
extern JSC_CONST_HASHTABLE HashTable stringConstructorTable;

void* JSGlobalData::jsFinalObjectVPtr;
void* JSGlobalData::jsArrayVPtr;
void* JSGlobalData::jsByteArrayVPtr;
void* JSGlobalData::jsStringVPtr;
void* JSGlobalData::jsFunctionVPtr;

#if COMPILER(GCC)
// Work around for gcc trying to coalesce our reads of the various cell vptrs
#define CLOBBER_MEMORY() do { \
    asm volatile ("" : : : "memory"); \
} while (false)
#else
#define CLOBBER_MEMORY() do { } while (false)
#endif

void JSGlobalData::storeVPtrs()
{
    // Enough storage to fit a JSArray, JSByteArray, JSString, or JSFunction.
    // COMPILE_ASSERTS below check that this is true.
    char storage[64];

    COMPILE_ASSERT(sizeof(JSFinalObject) <= sizeof(storage), sizeof_JSFinalObject_must_be_less_than_storage);
    JSCell* jsFinalObject = new (storage) JSFinalObject(JSFinalObject::VPtrStealingHack);
    CLOBBER_MEMORY();
    JSGlobalData::jsFinalObjectVPtr = jsFinalObject->vptr();

    COMPILE_ASSERT(sizeof(JSArray) <= sizeof(storage), sizeof_JSArray_must_be_less_than_storage);
    JSCell* jsArray = new (storage) JSArray(JSArray::VPtrStealingHack);
    CLOBBER_MEMORY();
    JSGlobalData::jsArrayVPtr = jsArray->vptr();

    COMPILE_ASSERT(sizeof(JSByteArray) <= sizeof(storage), sizeof_JSByteArray_must_be_less_than_storage);
    JSCell* jsByteArray = new (storage) JSByteArray(JSByteArray::VPtrStealingHack);
    CLOBBER_MEMORY();
    JSGlobalData::jsByteArrayVPtr = jsByteArray->vptr();

    COMPILE_ASSERT(sizeof(JSString) <= sizeof(storage), sizeof_JSString_must_be_less_than_storage);
    JSCell* jsString = new (storage) JSString(JSString::VPtrStealingHack);
    CLOBBER_MEMORY();
    JSGlobalData::jsStringVPtr = jsString->vptr();

    COMPILE_ASSERT(sizeof(JSFunction) <= sizeof(storage), sizeof_JSFunction_must_be_less_than_storage);
    JSCell* jsFunction = new (storage) JSFunction(JSCell::VPtrStealingHack);
    CLOBBER_MEMORY();
    JSGlobalData::jsFunctionVPtr = jsFunction->vptr();
}

JSGlobalData::JSGlobalData(GlobalDataType globalDataType, ThreadStackType threadStackType, HeapSize heapSize)
    : globalDataType(globalDataType)
    , clientData(0)
    , topCallFrame(CallFrame::noCaller())
    , arrayConstructorTable(fastNew<HashTable>(JSC::arrayConstructorTable))
    , arrayPrototypeTable(fastNew<HashTable>(JSC::arrayPrototypeTable))
    , booleanPrototypeTable(fastNew<HashTable>(JSC::booleanPrototypeTable))
    , dateTable(fastNew<HashTable>(JSC::dateTable))
    , dateConstructorTable(fastNew<HashTable>(JSC::dateConstructorTable))
    , errorPrototypeTable(fastNew<HashTable>(JSC::errorPrototypeTable))
    , globalObjectTable(fastNew<HashTable>(JSC::globalObjectTable))
    , jsonTable(fastNew<HashTable>(JSC::jsonTable))
    , mathTable(fastNew<HashTable>(JSC::mathTable))
    , numberConstructorTable(fastNew<HashTable>(JSC::numberConstructorTable))
    , numberPrototypeTable(fastNew<HashTable>(JSC::numberPrototypeTable))
    , objectConstructorTable(fastNew<HashTable>(JSC::objectConstructorTable))
    , objectPrototypeTable(fastNew<HashTable>(JSC::objectPrototypeTable))
    , regExpTable(fastNew<HashTable>(JSC::regExpTable))
    , regExpConstructorTable(fastNew<HashTable>(JSC::regExpConstructorTable))
    , regExpPrototypeTable(fastNew<HashTable>(JSC::regExpPrototypeTable))
    , stringTable(fastNew<HashTable>(JSC::stringTable))
    , stringConstructorTable(fastNew<HashTable>(JSC::stringConstructorTable))
    , identifierTable(globalDataType == Default ? wtfThreadData().currentIdentifierTable() : createIdentifierTable())
    , propertyNames(new CommonIdentifiers(this))
    , emptyList(new MarkedArgumentBuffer)
#if ENABLE(ASSEMBLER)
    , executableAllocator(*this)
#endif
    , parserArena(new ParserArena)
    , keywords(new Keywords(this))
    , interpreter(0)
    , heap(this, heapSize)
#if ENABLE(DFG_JIT)
    , sizeOfLastScratchBuffer(0)
#endif
    , dynamicGlobalObject(0)
    , cachedUTCOffset(std::numeric_limits<double>::quiet_NaN())
    , maxReentryDepth(threadStackType == ThreadStackTypeSmall ? MaxSmallThreadReentryDepth : MaxLargeThreadReentryDepth)
    , m_regExpCache(new RegExpCache(this))
#if ENABLE(REGEXP_TRACING)
    , m_rtTraceList(new RTTraceList())
#endif
#ifndef NDEBUG
    , exclusiveThread(0)
#endif
#if CPU(X86) && ENABLE(JIT)
    , m_timeoutCount(512)
#endif
#if ENABLE(GC_VALIDATION)
    , m_isInitializingObject(false)
#endif
{
    interpreter = new Interpreter;
    if (globalDataType == Default)
        m_stack = wtfThreadData().stack();

    // Need to be careful to keep everything consistent here
    IdentifierTable* existingEntryIdentifierTable = wtfThreadData().setCurrentIdentifierTable(identifierTable);
    JSLock lock(SilenceAssertionsOnly);
    structureStructure.set(*this, Structure::createStructure(*this));
    debuggerActivationStructure.set(*this, DebuggerActivation::createStructure(*this, 0, jsNull()));
    activationStructure.set(*this, JSActivation::createStructure(*this, 0, jsNull()));
    interruptedExecutionErrorStructure.set(*this, InterruptedExecutionError::createStructure(*this, 0, jsNull()));
    terminatedExecutionErrorStructure.set(*this, TerminatedExecutionError::createStructure(*this, 0, jsNull()));
    staticScopeStructure.set(*this, JSStaticScopeObject::createStructure(*this, 0, jsNull()));
    strictEvalActivationStructure.set(*this, StrictEvalActivation::createStructure(*this, 0, jsNull()));
    stringStructure.set(*this, JSString::createStructure(*this, 0, jsNull()));
    notAnObjectStructure.set(*this, JSNotAnObject::createStructure(*this, 0, jsNull()));
    propertyNameIteratorStructure.set(*this, JSPropertyNameIterator::createStructure(*this, 0, jsNull()));
    getterSetterStructure.set(*this, GetterSetter::createStructure(*this, 0, jsNull()));
    apiWrapperStructure.set(*this, JSAPIValueWrapper::createStructure(*this, 0, jsNull()));
    scopeChainNodeStructure.set(*this, ScopeChainNode::createStructure(*this, 0, jsNull()));
    executableStructure.set(*this, ExecutableBase::createStructure(*this, 0, jsNull()));
    nativeExecutableStructure.set(*this, NativeExecutable::createStructure(*this, 0, jsNull()));
    evalExecutableStructure.set(*this, EvalExecutable::createStructure(*this, 0, jsNull()));
    programExecutableStructure.set(*this, ProgramExecutable::createStructure(*this, 0, jsNull()));
    functionExecutableStructure.set(*this, FunctionExecutable::createStructure(*this, 0, jsNull()));
    regExpStructure.set(*this, RegExp::createStructure(*this, 0, jsNull()));
    structureChainStructure.set(*this, StructureChain::createStructure(*this, 0, jsNull()));

    wtfThreadData().setCurrentIdentifierTable(existingEntryIdentifierTable);

#if ENABLE(JIT) && ENABLE(INTERPRETER)
#if USE(CF)
    CFStringRef canUseJITKey = CFStringCreateWithCString(0 , "JavaScriptCoreUseJIT", kCFStringEncodingMacRoman);
    CFBooleanRef canUseJIT = (CFBooleanRef)CFPreferencesCopyAppValue(canUseJITKey, kCFPreferencesCurrentApplication);
    if (canUseJIT) {
        m_canUseJIT = kCFBooleanTrue == canUseJIT;
        CFRelease(canUseJIT);
    } else {
      char* canUseJITString = getenv("JavaScriptCoreUseJIT");
      m_canUseJIT = !canUseJITString || atoi(canUseJITString);
    }
    CFRelease(canUseJITKey);
#elif OS(UNIX)
    char* canUseJITString = getenv("JavaScriptCoreUseJIT");
    m_canUseJIT = !canUseJITString || atoi(canUseJITString);
#else
    m_canUseJIT = true;
#endif
#endif
#if ENABLE(JIT)
#if ENABLE(INTERPRETER)
    if (m_canUseJIT)
        m_canUseJIT = executableAllocator.isValid();
#endif
    jitStubs = adoptPtr(new JITThunks(this));
#endif

    heap.notifyIsSafeToCollect();
}

void JSGlobalData::clearBuiltinStructures()
{
    structureStructure.clear();
    debuggerActivationStructure.clear();
    activationStructure.clear();
    interruptedExecutionErrorStructure.clear();
    terminatedExecutionErrorStructure.clear();
    staticScopeStructure.clear();
    strictEvalActivationStructure.clear();
    stringStructure.clear();
    notAnObjectStructure.clear();
    propertyNameIteratorStructure.clear();
    getterSetterStructure.clear();
    apiWrapperStructure.clear();
    scopeChainNodeStructure.clear();
    executableStructure.clear();
    nativeExecutableStructure.clear();
    evalExecutableStructure.clear();
    programExecutableStructure.clear();
    functionExecutableStructure.clear();
    regExpStructure.clear();
    structureChainStructure.clear();
}

JSGlobalData::~JSGlobalData()
{
    // By the time this is destroyed, heap.destroy() must already have been called.

    delete interpreter;
#ifndef NDEBUG
    // Zeroing out to make the behavior more predictable when someone attempts to use a deleted instance.
    interpreter = 0;
#endif

    arrayPrototypeTable->deleteTable();
    arrayConstructorTable->deleteTable();
    booleanPrototypeTable->deleteTable();
    dateTable->deleteTable();
    dateConstructorTable->deleteTable();
    errorPrototypeTable->deleteTable();
    globalObjectTable->deleteTable();
    jsonTable->deleteTable();
    mathTable->deleteTable();
    numberConstructorTable->deleteTable();
    numberPrototypeTable->deleteTable();
    objectConstructorTable->deleteTable();
    objectPrototypeTable->deleteTable();
    regExpTable->deleteTable();
    regExpConstructorTable->deleteTable();
    regExpPrototypeTable->deleteTable();
    stringTable->deleteTable();
    stringConstructorTable->deleteTable();

    fastDelete(const_cast<HashTable*>(arrayConstructorTable));
    fastDelete(const_cast<HashTable*>(arrayPrototypeTable));
    fastDelete(const_cast<HashTable*>(booleanPrototypeTable));
    fastDelete(const_cast<HashTable*>(dateTable));
    fastDelete(const_cast<HashTable*>(dateConstructorTable));
    fastDelete(const_cast<HashTable*>(errorPrototypeTable));
    fastDelete(const_cast<HashTable*>(globalObjectTable));
    fastDelete(const_cast<HashTable*>(jsonTable));
    fastDelete(const_cast<HashTable*>(mathTable));
    fastDelete(const_cast<HashTable*>(numberConstructorTable));
    fastDelete(const_cast<HashTable*>(numberPrototypeTable));
    fastDelete(const_cast<HashTable*>(objectConstructorTable));
    fastDelete(const_cast<HashTable*>(objectPrototypeTable));
    fastDelete(const_cast<HashTable*>(regExpTable));
    fastDelete(const_cast<HashTable*>(regExpConstructorTable));
    fastDelete(const_cast<HashTable*>(regExpPrototypeTable));
    fastDelete(const_cast<HashTable*>(stringTable));
    fastDelete(const_cast<HashTable*>(stringConstructorTable));

    deleteAllValues(opaqueJSClassData);

    delete emptyList;

    delete propertyNames;
    if (globalDataType != Default)
        deleteIdentifierTable(identifierTable);

    delete clientData;
    delete m_regExpCache;
#if ENABLE(REGEXP_TRACING)
    delete m_rtTraceList;
#endif

#if ENABLE(DFG_JIT)
    for (unsigned i = 0; i < scratchBuffers.size(); ++i)
        fastFree(scratchBuffers[i]);
#endif
}

PassRefPtr<JSGlobalData> JSGlobalData::createContextGroup(ThreadStackType type, HeapSize heapSize)
{
    return adoptRef(new JSGlobalData(APIContextGroup, type, heapSize));
}

PassRefPtr<JSGlobalData> JSGlobalData::create(ThreadStackType type, HeapSize heapSize)
{
    return adoptRef(new JSGlobalData(Default, type, heapSize));
}

PassRefPtr<JSGlobalData> JSGlobalData::createLeaked(ThreadStackType type, HeapSize heapSize)
{
    return create(type, heapSize);
}

bool JSGlobalData::sharedInstanceExists()
{
    return sharedInstanceInternal();
}

JSGlobalData& JSGlobalData::sharedInstance()
{
    JSGlobalData*& instance = sharedInstanceInternal();
    if (!instance) {
        instance = adoptRef(new JSGlobalData(APIShared, ThreadStackTypeSmall, SmallHeap)).leakRef();
        instance->makeUsableFromMultipleThreads();
    }
    return *instance;
}

JSGlobalData*& JSGlobalData::sharedInstanceInternal()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());
    static JSGlobalData* sharedInstance;
    return sharedInstance;
}

#if ENABLE(JIT)
NativeExecutable* JSGlobalData::getHostFunction(NativeFunction function, NativeFunction constructor)
{
    return jitStubs->hostFunctionStub(this, function, constructor);
}
NativeExecutable* JSGlobalData::getHostFunction(NativeFunction function, ThunkGenerator generator, DFG::Intrinsic intrinsic)
{
    return jitStubs->hostFunctionStub(this, function, generator, intrinsic);
}
#else
NativeExecutable* JSGlobalData::getHostFunction(NativeFunction function, NativeFunction constructor)
{
    return NativeExecutable::create(*this, function, constructor);
}
#endif

JSGlobalData::ClientData::~ClientData()
{
}

void JSGlobalData::resetDateCache()
{
    cachedUTCOffset = std::numeric_limits<double>::quiet_NaN();
    dstOffsetCache.reset();
    cachedDateString = UString();
    cachedDateStringValue = std::numeric_limits<double>::quiet_NaN();
    dateInstanceCache.reset();
}

void JSGlobalData::startSampling()
{
    interpreter->startSampling();
}

void JSGlobalData::stopSampling()
{
    interpreter->stopSampling();
}

void JSGlobalData::dumpSampleData(ExecState* exec)
{
    interpreter->dumpSampleData(exec);
#if ENABLE(ASSEMBLER)
    ExecutableAllocator::dumpProfile();
#endif
}

void JSGlobalData::recompileAllJSFunctions()
{
    // If JavaScript is running, it's not safe to recompile, since we'll end
    // up throwing away code that is live on the stack.
    ASSERT(!dynamicGlobalObject);
    
    heap.objectSpace().forEachCell<Recompiler>();
}

struct StackPreservingRecompiler : public MarkedBlock::VoidFunctor {
    HashSet<FunctionExecutable*> currentlyExecutingFunctions;
    void operator()(JSCell* cell)
    {
        if (!cell->inherits(&FunctionExecutable::s_info))
            return;
        FunctionExecutable* executable = static_cast<FunctionExecutable*>(cell);
        if (currentlyExecutingFunctions.contains(executable))
            return;
        executable->discardCode();
    }
};

void JSGlobalData::releaseExecutableMemory()
{
    if (dynamicGlobalObject) {
        StackPreservingRecompiler recompiler;
        HashSet<JSCell*> roots;
        heap.getConservativeRegisterRoots(roots);
        HashSet<JSCell*>::iterator end = roots.end();
        for (HashSet<JSCell*>::iterator ptr = roots.begin(); ptr != end; ++ptr) {
            ScriptExecutable* executable = 0;
            JSCell* cell = *ptr;
            if (cell->inherits(&ScriptExecutable::s_info))
                executable = static_cast<ScriptExecutable*>(*ptr);
            else if (cell->inherits(&JSFunction::s_info)) {
                JSFunction* function = asFunction(*ptr);
                if (function->isHostFunction())
                    continue;
                executable = function->jsExecutable();
            } else
                continue;
            ASSERT(executable->inherits(&ScriptExecutable::s_info));
            executable->unlinkCalls();
            if (executable->inherits(&FunctionExecutable::s_info))
                recompiler.currentlyExecutingFunctions.add(static_cast<FunctionExecutable*>(executable));
                
        }
        heap.objectSpace().forEachCell<StackPreservingRecompiler>(recompiler);
    }
    m_regExpCache->invalidateCode();
    heap.collectAllGarbage();
}
    
void releaseExecutableMemory(JSGlobalData& globalData)
{
    globalData.releaseExecutableMemory();
}

#if ENABLE(REGEXP_TRACING)
void JSGlobalData::addRegExpToTrace(RegExp* regExp)
{
    m_rtTraceList->add(regExp);
}

void JSGlobalData::dumpRegExpTrace()
{
    // The first RegExp object is ignored.  It is create by the RegExpPrototype ctor and not used.
    RTTraceList::iterator iter = ++m_rtTraceList->begin();
    
    if (iter != m_rtTraceList->end()) {
        printf("\nRegExp Tracing\n");
        printf("                                                            match()    matches\n");
        printf("Regular Expression                          JIT Address      calls      found\n");
        printf("----------------------------------------+----------------+----------+----------\n");
    
        unsigned reCount = 0;
    
        for (; iter != m_rtTraceList->end(); ++iter, ++reCount)
            (*iter)->printTraceData();

        printf("%d Regular Expressions\n", reCount);
    }
    
    m_rtTraceList->clear();
}
#else
void JSGlobalData::dumpRegExpTrace()
{
}
#endif

} // namespace JSC
