/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "HeapCellInlines.h"
#include "JSCJSValueInlines.h"
// The above are needed before DFGAbstractValue.h
#include "DFGAbstractValue.h"
#include "InitializeThreading.h"
#include <wtf/DataLog.h>
#include <wtf/text/StringCommon.h>

// We don't have a NO_RETURN_DUE_TO_EXIT, nor should we. That's ridiculous.
static bool hiddenTruthBecauseNoReturnIsStupid() { return true; }

static void usage()
{
    dataLog("Usage: testdfg [<filter>]\n");
    if (hiddenTruthBecauseNoReturnIsStupid())
        exit(1);
}

#if ENABLE(DFG_JIT)

using namespace JSC;
using namespace JSC::DFG;

namespace {

// Nothing fancy for now; we just use the existing WTF assertion machinery.
#define CHECK(x) do {                                                           \
        if (!!(x))                                                              \
            break;                                                              \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #x); \
        CRASH();                                                                \
    } while (false)


#define RUN_NOW(test) do {                      \
        if (!shouldRun(#test))                  \
            break;                              \
        dataLog(#test "...\n");          \
        test;                                   \
        dataLog(#test ": OK!\n");        \
    } while (false)

static void testEmptyValueDoesNotValidateWithHeapTop()
{
    AbstractValue value;

    value.makeHeapTop();
    CHECK(!value.validateOSREntryValue(JSValue(), FlushedJSValue));

    value.makeBytecodeTop();
    CHECK(value.validateOSREntryValue(JSValue(), FlushedJSValue));
}

void run(const char* filter)
{
    auto shouldRun = [&] (const char* testName) -> bool {
        return !filter || WTF::findIgnoringASCIICaseWithoutLength(testName, filter) != WTF::notFound;
    };

    RUN_NOW(testEmptyValueDoesNotValidateWithHeapTop());
}

} // anonymous namespace

#else // ENABLE(DFG_JIT)

static void run(const char*)
{
    dataLog("DFG JIT is not enabled.\n");
}

#endif // ENABLE(DFG_JIT)

int main(int argc, char** argv)
{
    const char* filter = nullptr;
    switch (argc) {
    case 1:
        break;
    case 2:
        filter = argv[1];
        break;
    default:
        usage();
        break;
    }

    JSC::initializeThreading();
    
    run(filter);

    return 0;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif
