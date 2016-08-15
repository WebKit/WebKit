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

#include "B3Compilation.h"
#include "InitializeThreading.h"
#include "JSString.h"
#include "VM.h"
#include "WASMPlan.h"
#include <wtf/DataLog.h>
#include <wtf/LEBDecoder.h>

class CommandLine {
public:
    CommandLine(int argc, char** argv)
    {
        parseArguments(argc, argv);
    }

    Vector<String> m_arguments;
    bool m_runLEBTests { false };
    bool m_runWASMTests { false };

    void parseArguments(int, char**);
};

static NO_RETURN void printUsageStatement(bool help = false)
{
    fprintf(stderr, "Usage: testWASM [options]\n");
    fprintf(stderr, "  -h|--help  Prints this help message\n");
    fprintf(stderr, "  -l|--leb   Runs the LEB decoder tests\n");
    fprintf(stderr, "  -w|--web   Run the WASM tests\n");
    fprintf(stderr, "\n");

    exit(help ? EXIT_SUCCESS : EXIT_FAILURE);
}

void CommandLine::parseArguments(int argc, char** argv)
{
    int i = 1;

    for (; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
            printUsageStatement(true);

        if (!strcmp(arg, "-l") || !strcmp(arg, "--leb"))
            m_runLEBTests = true;

        if (!strcmp(arg, "-w") || !strcmp(arg, "--web"))
            m_runWASMTests = true;
    }

    for (; i < argc; ++i)
        m_arguments.append(argv[i]);

}

#define FOR_EACH_UNSIGNED_LEB_TEST(macro) \
    /* Simple tests that use all the bits in the array */ \
    macro(({ 0x07 }), 0, true, 0x7, 1) \
    macro(({ 0x77 }), 0, true, 0x77, 1) \
    macro(({ 0x80, 0x07 }), 0, true, 0x380, 2) \
    macro(({ 0x89, 0x12 }), 0, true, 0x909, 2) \
    macro(({ 0xf3, 0x85, 0x02 }), 0, true, 0x82f3, 3) \
    macro(({ 0xf3, 0x85, 0xff, 0x74 }), 0, true, 0xe9fc2f3, 4) \
    macro(({ 0xf3, 0x85, 0xff, 0xf4, 0x7f }), 0, true, 0xfe9fc2f3, 5) \
    /* Test with extra trailing numbers */ \
    macro(({ 0x07, 0x80 }), 0, true, 0x7, 1) \
    macro(({ 0x07, 0x75 }), 0, true, 0x7, 1) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x43 }), 0, true, 0xe9fc2f3, 4) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x80 }), 0, true, 0xe9fc2f3, 4) \
    /* Test with preceeding numbers */ \
    macro(({ 0xf3, 0x07 }), 1, true, 0x7, 2) \
    macro(({ 0x03, 0x07 }), 1, true, 0x7, 2) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77 }), 5, true, 0x77, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77 }), 5, true, 0x77, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02 }), 3, true, 0x82f3, 6) \
    /* Test in the middle */ \
    macro(({ 0xf3, 0x07, 0x89 }), 1, true, 0x7, 2) \
    macro(({ 0x03, 0x07, 0x23 }), 1, true, 0x7, 2) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77, 0x43 }), 5, true, 0x77, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77, 0xf9 }), 5, true, 0x77, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02, 0xa4 }), 3, true, 0x82f3, 6) \
    /* Test decode too long */ \
    macro(({ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}), 0, false, 0x0, 0) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 1, false, 0x0, 0) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 0, false, 0x0, 0) \
    /* Test decode off end of array */ \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 2, false, 0x0, 0) \


#define TEST_UNSIGNED_LEB_DECODE(init, startOffset, expectedStatus, expectedResult, expectedOffset) \
    { \
        Vector<uint8_t> vector = Vector<uint8_t> init; \
        size_t offset = startOffset; \
        uint32_t result; \
        bool status = decodeUInt32(vector.data(), vector.size(), offset, result); \
        RELEASE_ASSERT(status == expectedStatus); \
        if (expectedStatus) { \
            RELEASE_ASSERT(result == expectedResult); \
            RELEASE_ASSERT(offset == expectedOffset); \
        } \
    };


#define FOR_EACH_SIGNED_LEB_TEST(macro) \
    /* Simple tests that use all the bits in the array */ \
    macro(({ 0x07 }), 0, true, 0x7, 1) \
    macro(({ 0x77 }), 0, true, -0x9, 1) \
    macro(({ 0x80, 0x07 }), 0, true, 0x380, 2) \
    macro(({ 0x89, 0x12 }), 0, true, 0x909, 2) \
    macro(({ 0xf3, 0x85, 0x02 }), 0, true, 0x82f3, 3) \
    macro(({ 0xf3, 0x85, 0xff, 0x74 }), 0, true, 0xfe9fc2f3, 4) \
    macro(({ 0xf3, 0x85, 0xff, 0xf4, 0x7f }), 0, true, 0xfe9fc2f3, 5) \
    /* Test with extra trailing numbers */ \
    macro(({ 0x07, 0x80 }), 0, true, 0x7, 1) \
    macro(({ 0x07, 0x75 }), 0, true, 0x7, 1) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x43 }), 0, true, 0xfe9fc2f3, 4) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x80 }), 0, true, 0xfe9fc2f3, 4) \
    /* Test with preceeding numbers */ \
    macro(({ 0xf3, 0x07 }), 1, true, 0x7, 2) \
    macro(({ 0x03, 0x07 }), 1, true, 0x7, 2) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77 }), 5, true, -0x9, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77 }), 5, true, -0x9, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02 }), 3, true, 0x82f3, 6) \
    /* Test in the middle */ \
    macro(({ 0xf3, 0x07, 0x89 }), 1, true, 0x7, 2) \
    macro(({ 0x03, 0x07, 0x23 }), 1, true, 0x7, 2) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77, 0x43 }), 5, true, -0x9, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77, 0xf9 }), 5, true, -0x9, 6) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02, 0xa4 }), 3, true, 0x82f3, 6) \
    /* Test decode too long */ \
    macro(({ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}), 0, false, 0x0, 0) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 1, false, 0x0, 0) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 0, false, 0x0, 0) \
    /* Test decode off end of array */ \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 2, false, 0x0, 0) \


#define TEST_SIGNED_LEB_DECODE(init, startOffset, expectedStatus, expectedResult, expectedOffset) \
    { \
        Vector<uint8_t> vector = Vector<uint8_t> init; \
        size_t offset = startOffset; \
        int32_t result; \
        bool status = decodeInt32(vector.data(), vector.size(), offset, result); \
        RELEASE_ASSERT(status == expectedStatus); \
        if (expectedStatus) { \
            int32_t expected = expectedResult; \
            RELEASE_ASSERT(result == expected); \
            RELEASE_ASSERT(offset == expectedOffset); \
        } \
    };


static void runLEBTests()
{
    FOR_EACH_UNSIGNED_LEB_TEST(TEST_UNSIGNED_LEB_DECODE)
    FOR_EACH_SIGNED_LEB_TEST(TEST_SIGNED_LEB_DECODE)
}

#if ENABLE(WEBASSEMBLY)

static JSC::VM* vm;

using namespace JSC;
using namespace WASM;
using namespace B3;

template<typename T, typename... Arguments>
T invoke(MacroAssemblerCodePtr ptr, Arguments... arguments)
{
    T (*function)(Arguments...) = bitwise_cast<T(*)(Arguments...)>(ptr.executableAddress());
    return function(arguments...);
}

template<typename T, typename... Arguments>
T invoke(const Compilation& code, Arguments... arguments)
{
    return invoke<T>(code.code(), arguments...);
}

// For now we inline the test files.
static void runWASMTests()
{
    {
        // Generated from: (module (func "return-i32" (result i32) (return (i32.const 5))) )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x85, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x00, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x82,
            0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x8d, 0x80, 0x80, 0x00,
            0x01, 0x00, 0x0a, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x2d, 0x69, 0x33, 0x32, 0x04, 0x63, 0x6f,
            0x64, 0x65, 0x8b, 0x80, 0x80, 0x00, 0x01, 0x86, 0x80, 0x80, 0x00, 0x00, 0x10, 0x05, 0x09, 0x01,
            0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        RELEASE_ASSERT(invoke<int>(*plan.result[0]) == 5);
    }


    {
        // Generated from: (module (func "return-i32" (result i32) (return (i32.add (i32.const 5) (i32.const 6)))) )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x85, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x00, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x82,
            0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x8d, 0x80, 0x80, 0x00,
            0x01, 0x00, 0x0a, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x2d, 0x69, 0x33, 0x32, 0x04, 0x63, 0x6f,
            0x64, 0x65, 0x8e, 0x80, 0x80, 0x00, 0x01, 0x89, 0x80, 0x80, 0x00, 0x00, 0x10, 0x05, 0x10, 0x06,
            0x40, 0x09, 0x01, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        RELEASE_ASSERT(invoke<int>(*plan.result[0]) == 11);
    }
    
    {
        // Generated from: (module (func "return-i32" (result i32) (return (i32.add (i32.add (i32.const 5) (i32.const 3)) (i32.const 3)))) )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x85, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x00, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x82,
            0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x8d, 0x80, 0x80, 0x00,
            0x01, 0x00, 0x0a, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x2d, 0x69, 0x33, 0x32, 0x04, 0x63, 0x6f,
            0x64, 0x65, 0x91, 0x80, 0x80, 0x00, 0x01, 0x8c, 0x80, 0x80, 0x00, 0x00, 0x10, 0x05, 0x10, 0x03,
            0x40, 0x10, 0x03, 0x40, 0x09, 0x01, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        RELEASE_ASSERT(invoke<int>(*plan.result[0]) == 11);
    }

    {
        // Generated from: (module (func "return-i32" (result i32) (block (return (i32.add (i32.add (i32.const 5) (i32.const 3)) (i32.const 3))))) )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x85, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x00, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x82,
            0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x8d, 0x80, 0x80, 0x00,
            0x01, 0x00, 0x0a, 0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x2d, 0x69, 0x33, 0x32, 0x04, 0x63, 0x6f,
            0x64, 0x65, 0x93, 0x80, 0x80, 0x00, 0x01, 0x8e, 0x80, 0x80, 0x00, 0x00, 0x01, 0x10, 0x05, 0x10,
            0x03, 0x40, 0x10, 0x03, 0x40, 0x09, 0x01, 0x0f, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        RELEASE_ASSERT(invoke<int>(*plan.result[0]) == 11);
    }
}

#endif // ENABLE(WEBASSEMBLY)

int main(int argc, char** argv)
{
    CommandLine options(argc, argv);

    if (options.m_runLEBTests)
        runLEBTests();


    if (options.m_runWASMTests) {
#if ENABLE(WEBASSEMBLY)
        JSC::initializeThreading();
        vm = &JSC::VM::create(JSC::LargeHeap).leakRef();
        runWASMTests();
#else
        dataLogLn("WASM is not enabled!");
        return EXIT_FAILURE;
#endif // ENABLE(WEBASSEMBLY)
    }

    return EXIT_SUCCESS;
}

