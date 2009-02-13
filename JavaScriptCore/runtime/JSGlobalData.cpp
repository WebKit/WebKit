/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "Collector.h"
#include "CommonIdentifiers.h"
#include "FunctionConstructor.h"
#include "Interpreter.h"
#include "JSActivation.h"
#include "JSClassRef.h"
#include "JSLock.h"
#include "JSNotAnObject.h"
#include "JSStaticScopeObject.h"
#include "Parser.h"
#include "Lexer.h"
#include "Lookup.h"
#include "Nodes.h"

#if ENABLE(JSC_MULTIPLE_THREADS)
#include <wtf/Threading.h>
#endif

#if PLATFORM(MAC)
#include "ProfilerServer.h"
#endif

using namespace WTF;

namespace JSC {

extern const HashTable arrayTable;
extern const HashTable dateTable;
extern const HashTable mathTable;
extern const HashTable numberTable;
extern const HashTable regExpTable;
extern const HashTable regExpConstructorTable;
extern const HashTable stringTable;

JSGlobalData::JSGlobalData(bool isShared)
    : initializingLazyNumericCompareFunction(false)
    , interpreter(new Interpreter)
    , exception(noValue())
    , arrayTable(new HashTable(JSC::arrayTable))
    , dateTable(new HashTable(JSC::dateTable))
    , mathTable(new HashTable(JSC::mathTable))
    , numberTable(new HashTable(JSC::numberTable))
    , regExpTable(new HashTable(JSC::regExpTable))
    , regExpConstructorTable(new HashTable(JSC::regExpConstructorTable))
    , stringTable(new HashTable(JSC::stringTable))
    , activationStructure(JSActivation::createStructure(jsNull()))
    , interruptedExecutionErrorStructure(JSObject::createStructure(jsNull()))
    , staticScopeStructure(JSStaticScopeObject::createStructure(jsNull()))
    , stringStructure(JSString::createStructure(jsNull()))
    , notAnObjectErrorStubStructure(JSNotAnObjectErrorStub::createStructure(jsNull()))
    , notAnObjectStructure(JSNotAnObject::createStructure(jsNull()))
#if !USE(ALTERNATE_JSIMMEDIATE)
    , numberStructure(JSNumberCell::createStructure(jsNull()))
#endif
    , identifierTable(createIdentifierTable())
    , propertyNames(new CommonIdentifiers(this))
    , emptyList(new ArgList)
    , newParserObjects(0)
    , parserObjectExtraRefCounts(0)
    , lexer(new Lexer(this))
    , parser(new Parser)
    , head(0)
    , dynamicGlobalObject(0)
    , isSharedInstance(isShared)
    , clientData(0)
    , scopeNodeBeingReparsed(0)
    , heap(this)
{
#if PLATFORM(MAC)
    startProfilerServerIfNeeded();
#endif
    interpreter->initialize(this);
}

JSGlobalData::~JSGlobalData()
{
    // By the time this is destroyed, heap.destroy() must already have been called.

    delete interpreter;
#ifndef NDEBUG
    // Zeroing out to make the behavior more predictable when someone attempts to use a deleted instance.
    interpreter = 0;
#endif

    arrayTable->deleteTable();
    dateTable->deleteTable();
    mathTable->deleteTable();
    numberTable->deleteTable();
    regExpTable->deleteTable();
    regExpConstructorTable->deleteTable();
    stringTable->deleteTable();
    delete arrayTable;
    delete dateTable;
    delete mathTable;
    delete numberTable;
    delete regExpTable;
    delete regExpConstructorTable;
    delete stringTable;

    delete parser;
    delete lexer;

    deleteAllValues(opaqueJSClassData);

    delete emptyList;

    delete propertyNames;
    deleteIdentifierTable(identifierTable);

    delete newParserObjects;
    delete parserObjectExtraRefCounts;
    
    delete clientData;
}

PassRefPtr<JSGlobalData> JSGlobalData::create()
{
    return adoptRef(new JSGlobalData);
}

PassRefPtr<JSGlobalData> JSGlobalData::createLeaked()
{
#ifndef NDEBUG
    Structure::startIgnoringLeaks();
    RefPtr<JSGlobalData> data = create();
    Structure::stopIgnoringLeaks();
    return data.release();
#else
    return create();
#endif
}

bool JSGlobalData::sharedInstanceExists()
{
    return sharedInstanceInternal();
}

JSGlobalData& JSGlobalData::sharedInstance()
{
    JSGlobalData*& instance = sharedInstanceInternal();
    if (!instance) {
        instance = new JSGlobalData(true);
#if ENABLE(JSC_MULTIPLE_THREADS)
        instance->makeUsableFromMultipleThreads();
#endif
    }
    return *instance;
}

JSGlobalData*& JSGlobalData::sharedInstanceInternal()
{
    ASSERT(JSLock::currentThreadIsHoldingLock());
    static JSGlobalData* sharedInstance;
    return sharedInstance;
}

// FIXME: We can also detect forms like v1 < v2 ? -1 : 0, reverse comparison, etc.
const Vector<Instruction>& JSGlobalData::numericCompareFunction(ExecState* exec)
{
    if (!lazyNumericCompareFunction.size() && !initializingLazyNumericCompareFunction) {
        initializingLazyNumericCompareFunction = true;
        RefPtr<ProgramNode> programNode = parser->parse<ProgramNode>(exec, 0, makeSource(UString("(function (v1, v2) { return v1 - v2; })")), 0, 0);
        RefPtr<FunctionBodyNode> functionBody = extractFunctionBody(programNode.get());
        lazyNumericCompareFunction = functionBody->bytecode(exec->scopeChain()).instructions();
        initializingLazyNumericCompareFunction = false;
    }

    return lazyNumericCompareFunction;
}

JSGlobalData::ClientData::~ClientData()
{
}

} // namespace JSC
