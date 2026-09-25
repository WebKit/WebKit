/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/Completion.h>
#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObjectInlines.h>
#include <JavaScriptCore/JSLock.h>
#include <JavaScriptCore/SourceCode.h>
#include <JavaScriptCore/VM.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <wtf/text/MakeString.h>

namespace JSCToolsTest {

namespace {

using JSC::Corpse::Address;
using JSC::Corpse::Snapshot;

struct VMFixture {
    int32_t sum { 0 };
};

// Just prove we can launch and attach to JSC.
Address createFixture()
{
    static VMFixture fixture;

    JSC::initialize();
    JSC::VM& vm = JSC::VM::create(JSC::HeapType::Large).leakRef();
    JSC::JSLockHolder locker(vm);
    JSC::JSGlobalObject* globalObject = JSC::JSGlobalObject::create(vm, JSC::JSGlobalObject::createStructure(vm, JSC::jsNull()));

    NakedPtr<JSC::Exception> exception;
    JSC::JSValue result = JSC::evaluate(globalObject, JSC::makeSource("40 + 2"_s, JSC::SourceOrigin { }, JSC::SourceTaintedOrigin::Untainted), JSC::JSValue(), exception);
    RELEASE_ASSERT(!exception && result.isInt32());

    fixture.sum = result.asInt32();
    return Address { &fixture };
}

void analyze(Snapshot& snapshot, Address fixtureAddress)
{
    auto fixture = snapshot.memory().ptr<VMFixture>(fixtureAddress);
    TEST_ASSERT(fixture, "the target's fixture reads");
    if (!fixture)
        return;
    TEST_ASSERT_EQ(fixture->sum, 40 + 2, "the target's VM computed the sum");
}

void analyzeExited(Snapshot& snapshot, Address fixtureAddress)
{
    TEST_ASSERT(kill(snapshot.process()->pid(), 0) && errno == ESRCH, "the target has exited");
    analyze(snapshot, fixtureAddress);
}

} // anonymous namespace

void testVM()
{
    SuiteTracer tracer("VM");
    if (!tracer.shouldRun())
        return;

    analyzeInAndOutOfProcess(createFixture, analyze);
    analyzeAfterTargetExits(createFixture, analyzeExited);
}

} // namespace JSCToolsTest

#endif // ENABLE(MYA)
