/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ExecutionHandlerTestSupport.h"

#if ENABLE(WEBASSEMBLY_DEBUGGER)

#include "Completion.h"
#include "Exception.h"
#include "ExecutionHandlerTest.h"
#include "Identifier.h"
#include "JSCJSValue.h"
#include "JSCJSValueInlines.h"
#include "JSFunction.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "JSObject.h"
#include "Options.h"
#include "SourceCode.h"
#include "SourceOrigin.h"
#include "Structure.h"
#include "StructureInlines.h"
#include "TestScripts.h"
#include "VM.h"
#include "WasmDebugServer.h"
#include "WasmExecutionHandler.h"
#include <wtf/Condition.h>
#include <wtf/DataLog.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NakedPtr.h>
#include <wtf/SentinelLinkedList.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Threading.h>
#include <wtf/URL.h>

namespace ExecutionHandlerTestSupport {

using JSC::Options;
using JSC::VM;
using JSC::Wasm::DebugServer;
using JSC::Wasm::ExecutionHandler;

std::atomic<unsigned> replyCount { 0 };

// ========== Worker/Workers classes (duplicated from jsc.cpp) ==========
// These are essential for proper VM tracking and $.agent.start() support

class Worker;

class Workers {
    WTF_MAKE_TZONE_ALLOCATED(Workers);
    WTF_MAKE_NONCOPYABLE(Workers);

public:
    Workers() = default;
    ~Workers() = default;

    static Workers& singleton()
    {
        static Workers* workers = new Workers();
        return *workers;
    }

    Lock m_lock;
    Condition m_condition;
    SentinelLinkedList<Worker, BasicRawSentinelNode<Worker>> m_workers;
};

class Worker : public BasicRawSentinelNode<Worker> {
public:
    Worker(Workers& workers, bool isMain)
        : m_workers(workers)
        , m_isMain(isMain)
    {
        Locker locker { m_workers.m_lock };
        m_workers.m_workers.append(this);

        *currentWorker() = this;
    }

    ~Worker()
    {
        Locker locker { m_workers.m_lock };
        RELEASE_ASSERT(isOnList());
        remove();
    }

    bool isMain() const { return m_isMain; }

    static Worker& current()
    {
        return **currentWorker();
    }

private:
    static ThreadSpecific<Worker*>& currentWorker()
    {
        static ThreadSpecific<Worker*>* worker;
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            worker = new ThreadSpecific<Worker*>();
        });
        return *worker;
    }

    Workers& m_workers;
    const bool m_isMain;
};

// ========== GlobalObject with $.agent.start() support ==========

class TestGlobalObject;
static JSC_DECLARE_HOST_FUNCTION(functionDollarAgentStart);
static JSC_DECLARE_HOST_FUNCTION(functionShouldExit);

class TestGlobalObject final : public JSC::JSGlobalObject {
public:
    using Base = JSC::JSGlobalObject;

    static TestGlobalObject* create(VM& vm, JSC::Structure* structure)
    {
        auto* object = new (NotNull, allocateCell<TestGlobalObject>(vm)) TestGlobalObject(vm, structure);
        object->finishCreation(vm);
        return object;
    }

    static JSC::Structure* createStructure(VM& vm, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, nullptr, prototype, JSC::TypeInfo(JSC::GlobalObjectType, StructureFlags), info());
    }

    DECLARE_INFO;

private:
    TestGlobalObject(VM& vm, JSC::Structure* structure)
        : Base(vm, structure, nullptr)
    {
    }

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        JSC::JSObject* dollar = JSC::JSFinalObject::create(vm, JSC::JSFinalObject::createStructure(vm, this, objectPrototype(), 0));
        JSC::JSObject* agent = JSC::JSFinalObject::create(vm, JSC::JSFinalObject::createStructure(vm, this, objectPrototype(), 0));
        JSC::Identifier startId = JSC::Identifier::fromString(vm, "start"_s);
        agent->putDirect(vm, startId, JSC::JSFunction::create(vm, this, 1, "start"_s, functionDollarAgentStart, JSC::ImplementationVisibility::Public));
        dollar->putDirect(vm, JSC::Identifier::fromString(vm, "agent"_s), agent);

        // Add shouldExit() function to $
        JSC::Identifier shouldExitId = JSC::Identifier::fromString(vm, "shouldExit"_s);
        dollar->putDirect(vm, shouldExitId, JSC::JSFunction::create(vm, this, 0, "shouldExit"_s, functionShouldExit, JSC::ImplementationVisibility::Public));

        putDirect(vm, JSC::Identifier::fromString(vm, "$"_s), dollar);
    }
};

using namespace JSC;
const JSC::ClassInfo TestGlobalObject::s_info = { "TestGlobalObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(TestGlobalObject) };

static constexpr ASCIILiteral RWI_REPLY_PREFIX = "[RWI] Reply: "_s;

// Spawns a worker thread that creates its own VM and runs the provided script.
// Follows jsc.cpp's Worker pattern for proper VM lifecycle management.
JSC_DEFINE_HOST_FUNCTION(functionDollarAgentStart, (JSC::JSGlobalObject * globalObject, JSC::CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (callFrame->argumentCount() < 1)
        return JSC::JSValue::encode(JSC::jsUndefined());

    JSC::JSValue scriptValue = callFrame->argument(0);
    String script = scriptValue.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, JSC::JSValue::encode(JSC::jsUndefined()));

    Lock didStartLock;
    Condition didStartCondition;
    bool didStart = false;

    Thread::create("JSC Agent"_s, [script = script.isolatedCopy(), &didStartLock, &didStartCondition, &didStart] {
        Worker worker(Workers::singleton(), false);
        VM& workerVM = VM::create(JSC::HeapType::Large).leakRef();

        {
            JSC::JSLockHolder locker(workerVM);
            auto* workerGlobal = TestGlobalObject::create(workerVM, TestGlobalObject::createStructure(workerVM, JSC::jsNull()));

            {
                Locker locker { didStartLock };
                didStart = true;
                didStartCondition.notifyOne();
            }

            JSC::SourceOrigin origin(URL({ }, "agent-worker"_s));
            JSC::SourceCode sourceCode = JSC::makeSource(script, origin, JSC::SourceTaintedOrigin::Untainted);

            NakedPtr<JSC::Exception> exception;
            JSC::evaluate(workerGlobal, sourceCode, JSC::JSValue(), exception);

            if (exception)
                dataLogLn("Worker exception: ", exception->value().toWTFString(workerGlobal));
        }

        {
            JSC::JSLockHolder locker(workerVM);
            workerVM.derefSuppressingSaferCPPChecking();
        }
    })->detach();

    {
        Locker locker { didStartLock };
        while (!didStart)
            didStartCondition.wait(didStartLock);
    }

    return JSC::JSValue::encode(JSC::jsUndefined());
}

// Returns true when tests are complete and WASM threads should exit their loops
JSC_DEFINE_HOST_FUNCTION(functionShouldExit, (JSC::JSGlobalObject*, JSC::CallFrame*))
{
    return JSC::JSValue::encode(JSC::jsBoolean(ExecutionHandlerTest::doneTesting));
}

// ========== Helper functions ==========

bool waitForCondition(std::function<bool()> predicate, Seconds timeout)
{
    auto deadline = MonotonicTime::now() + timeout;
    while (!predicate()) {
        if (MonotonicTime::now() >= deadline)
            return false;
    }
    return true;
}

void setupTestEnvironment(DebugServer*& debugServer, ExecutionHandler*& executionHandler)
{
    WTF::initializeMainThread();
    Options::setOptions("--enableWasmDebugger=true");

    debugServer = &DebugServer::singleton();
    bool started = debugServer->startRWI([](const String& packet) {
        replyCount++;
        dataLogLnIf(verboseLogging, RWI_REPLY_PREFIX, packet);
        return true;
    });

    RELEASE_ASSERT(started, "Failed to start DebugServer in RWI mode");
    RELEASE_ASSERT(debugServer->isConnected(), "DebugServer not connected");

    executionHandler = &debugServer->execution();
    executionHandler->setDebugServerThreadId(Thread::currentSingleton().uid());

    dataLogLnIf(verboseLogging, "DebugServer setup complete in RWI mode");
}

void workerThreadTask(const String& script)
{
    dataLogLnIf(verboseLogging, "Worker thread starting");

    Worker worker(Workers::singleton(), false);
    VM& vm = VM::create(JSC::HeapType::Large).leakRef();

    {
        JSC::JSLockHolder locker(vm);
        auto* globalObject = TestGlobalObject::create(vm, TestGlobalObject::createStructure(vm, JSC::jsNull()));

        dataLogLnIf(verboseLogging, "Worker thread created VM ", RawPointer(&vm), " with TestGlobalObject");

        NakedPtr<JSC::Exception> exception;
        JSC::SourceOrigin origin(URL({ }, "worker"_s));
        JSC::SourceCode sourceCode = JSC::makeSource(script, origin, JSC::SourceTaintedOrigin::Untainted);

        JSC::evaluate(globalObject, sourceCode, JSC::JSValue(), exception);

        if (exception)
            dataLogLn("ERROR: Worker thread got exception: ", exception->value().toWTFString(globalObject));
        else
            dataLogLnIf(verboseLogging, "Worker thread script completed normally (shouldExit() returned true)");
    }

    {
        JSC::JSLockHolder locker(vm);
        vm.derefSuppressingSaferCPPChecking();
    }

    dataLogLnIf(verboseLogging, "Worker thread ending");
}

} // namespace ExecutionHandlerTestSupport

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExecutionHandlerTestSupport::Workers);

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
