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
#include "JSCJSValueInlines.h"
#include "JSString.h"
#include "LLIntThunks.h"
#include "ProtoCallFrame.h"
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

StaticLock crashLock;

#define CHECK_EQ(x, y) do { \
        auto __x = (x); \
        auto __y = (y); \
        if (__x == __y) \
        break; \
        crashLock.lock(); \
        WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, toCString(#x " == " #y, " (" #x " == ", __x, ", " #y " == ", __y, ")").data()); \
        CRASH(); \
    } while (false)

#define FOR_EACH_UNSIGNED_LEB_TEST(macro) \
    /* Simple tests that use all the bits in the array */ \
    macro(({ 0x07 }), 0, true, 0x7lu, 1lu) \
    macro(({ 0x77 }), 0, true, 0x77lu, 1lu) \
    macro(({ 0x80, 0x07 }), 0, true, 0x380lu, 2lu) \
    macro(({ 0x89, 0x12 }), 0, true, 0x909lu, 2lu) \
    macro(({ 0xf3, 0x85, 0x02 }), 0, true, 0x82f3lu, 3lu) \
    macro(({ 0xf3, 0x85, 0xff, 0x74 }), 0, true, 0xe9fc2f3lu, 4lu) \
    macro(({ 0xf3, 0x85, 0xff, 0xf4, 0x7f }), 0, true, 0xfe9fc2f3lu, 5lu) \
    /* Test with extra trailing numbers */ \
    macro(({ 0x07, 0x80 }), 0, true, 0x7lu, 1lu) \
    macro(({ 0x07, 0x75 }), 0, true, 0x7lu, 1lu) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x43 }), 0, true, 0xe9fc2f3lu, 4lu) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x80 }), 0, true, 0xe9fc2f3lu, 4lu) \
    /* Test with preceeding numbers */ \
    macro(({ 0xf3, 0x07 }), 1, true, 0x7lu, 2lu) \
    macro(({ 0x03, 0x07 }), 1, true, 0x7lu, 2lu) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77 }), 5, true, 0x77lu, 6lu) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77 }), 5, true, 0x77lu, 6ul) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02 }), 3, true, 0x82f3lu, 6lu) \
    /* Test in the middle */ \
    macro(({ 0xf3, 0x07, 0x89 }), 1, true, 0x7lu, 2lu) \
    macro(({ 0x03, 0x07, 0x23 }), 1, true, 0x7lu, 2lu) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77, 0x43 }), 5, true, 0x77lu, 6lu) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77, 0xf9 }), 5, true, 0x77lu, 6lu) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02, 0xa4 }), 3, true, 0x82f3lu, 6lu) \
    /* Test decode too long */ \
    macro(({ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}), 0, false, 0x0lu, 0lu) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 1, false, 0x0lu, 0lu) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 0, false, 0x0lu, 0lu) \
    /* Test decode off end of array */ \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 2, false, 0x0lu, 0lu) \


#define TEST_UNSIGNED_LEB_DECODE(init, startOffset, expectedStatus, expectedResult, expectedOffset) \
    { \
        Vector<uint8_t> vector = Vector<uint8_t> init; \
        size_t offset = startOffset; \
        uint32_t result; \
        bool status = decodeUInt32(vector.data(), vector.size(), offset, result); \
        CHECK_EQ(status, expectedStatus); \
        if (expectedStatus) { \
            CHECK_EQ(result, expectedResult); \
            CHECK_EQ(offset, expectedOffset); \
        } \
    };


#define FOR_EACH_SIGNED_LEB_TEST(macro) \
    /* Simple tests that use all the bits in the array */ \
    macro(({ 0x07 }), 0, true, 0x7, 1lu) \
    macro(({ 0x77 }), 0, true, -0x9, 1lu) \
    macro(({ 0x80, 0x07 }), 0, true, 0x380, 2lu) \
    macro(({ 0x89, 0x12 }), 0, true, 0x909, 2lu) \
    macro(({ 0xf3, 0x85, 0x02 }), 0, true, 0x82f3, 3lu) \
    macro(({ 0xf3, 0x85, 0xff, 0x74 }), 0, true, 0xfe9fc2f3, 4lu) \
    macro(({ 0xf3, 0x85, 0xff, 0xf4, 0x7f }), 0, true, 0xfe9fc2f3, 5lu) \
    /* Test with extra trailing numbers */ \
    macro(({ 0x07, 0x80 }), 0, true, 0x7, 1lu) \
    macro(({ 0x07, 0x75 }), 0, true, 0x7, 1lu) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x43 }), 0, true, 0xfe9fc2f3, 4lu) \
    macro(({ 0xf3, 0x85, 0xff, 0x74, 0x80 }), 0, true, 0xfe9fc2f3, 4lu) \
    /* Test with preceeding numbers */ \
    macro(({ 0xf3, 0x07 }), 1, true, 0x7, 2lu) \
    macro(({ 0x03, 0x07 }), 1, true, 0x7, 2lu) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77 }), 5, true, -0x9, 6lu) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77 }), 5, true, -0x9, 6lu) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02 }), 3, true, 0x82f3, 6lu) \
    /* Test in the middle */ \
    macro(({ 0xf3, 0x07, 0x89 }), 1, true, 0x7, 2lu) \
    macro(({ 0x03, 0x07, 0x23 }), 1, true, 0x7, 2lu) \
    macro(({ 0xf2, 0x53, 0x43, 0x67, 0x79, 0x77, 0x43 }), 5, true, -0x9, 6lu) \
    macro(({ 0xf2, 0x53, 0x43, 0xf7, 0x84, 0x77, 0xf9 }), 5, true, -0x9, 6lu) \
    macro(({ 0xf2, 0x53, 0x43, 0xf3, 0x85, 0x02, 0xa4 }), 3, true, 0x82f3, 6lu) \
    /* Test decode too long */ \
    macro(({ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}), 0, false, 0x0, 0lu) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 1, false, 0x0, 0lu) \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 0, false, 0x0, 0lu) \
    /* Test decode off end of array */ \
    macro(({ 0x80, 0x80, 0xab, 0x8a, 0x9a, 0xa3, 0xff}), 2, false, 0x0, 0lu) \


#define TEST_SIGNED_LEB_DECODE(init, startOffset, expectedStatus, expectedResult, expectedOffset) \
    { \
        Vector<uint8_t> vector = Vector<uint8_t> init; \
        size_t offset = startOffset; \
        int32_t result; \
        bool status = decodeInt32(vector.data(), vector.size(), offset, result); \
        CHECK_EQ(status, expectedStatus); \
        if (expectedStatus) { \
            int32_t expected = expectedResult; \
            CHECK_EQ(result, expected); \
            CHECK_EQ(offset, expectedOffset); \
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

template<typename T>
T invoke(MacroAssemblerCodePtr ptr, std::initializer_list<JSValue> args)
{
    JSValue firstArgument;
    // Since vmEntryToJavaScript expects a this value we claim there is one... there isn't.
    int argCount = 1;
    JSValue* remainingArguments = nullptr;
    if (args.size()) {
        remainingArguments = const_cast<JSValue*>(args.begin());
        firstArgument = *remainingArguments;
        remainingArguments++;
        argCount = args.size();
    }

    ProtoCallFrame protoCallFrame;
    protoCallFrame.init(nullptr, nullptr, firstArgument, argCount, remainingArguments);

    // This won't work for floating point values but we don't have those yet.
    return static_cast<T>(vmEntryToWASM(ptr.executableAddress(), vm, &protoCallFrame));
}

template<typename T>
T invoke(const Compilation& code, std::initializer_list<JSValue> args)
{
    return invoke<T>(code.code(), args);
}

inline JSValue box(uint64_t value)
{
    return JSValue::decode(value);
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
        CHECK_EQ(invoke<int>(*plan.result[0], { }), 5);
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
        CHECK_EQ(invoke<int>(*plan.result[0], { }), 11);
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
        CHECK_EQ(invoke<int>(*plan.result[0], { }), 11);
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
        CHECK_EQ(invoke<int>(*plan.result[0], { }), 11);
    }

    {
        // Generated from: (module (func $add (param $x i32) (param $y i32) (result i32) (return (i32.add (get_local $x) (get_local $y)))) )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x87, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x02, 0x01, 0x01, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f,
            0x6e, 0x82, 0x80, 0x80, 0x00, 0x01, 0x00, 0x04, 0x63, 0x6f, 0x64, 0x65, 0x8e, 0x80, 0x80, 0x00,
            0x01, 0x89, 0x80, 0x80, 0x00, 0x00, 0x14, 0x00, 0x14, 0x01, 0x40, 0x09, 0x01, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        CHECK_EQ(invoke<int>(*plan.result[0], { box(0), box(1) }), 1);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(100), box(1) }), 101);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(-1), box(1)}), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(std::numeric_limits<int>::max()), box(1) }), std::numeric_limits<int>::min());
    }

    {
        // Generated from:
        //    (module
        //     (func "locals" (param $x i32) (result i32) (local $num i32)
        //      (set_local $num (get_local $x))
        //      (return (get_local $num))
        //      )
        //     )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x86, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x01, 0x01, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e,
            0x82, 0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x89, 0x80, 0x80,
            0x00, 0x01, 0x00, 0x06, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x73, 0x04, 0x63, 0x6f, 0x64, 0x65, 0x91,
            0x80, 0x80, 0x00, 0x01, 0x8c, 0x80, 0x80, 0x00, 0x01, 0x01, 0x01, 0x14, 0x00, 0x15, 0x01, 0x14,
            0x01, 0x09, 0x01, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        CHECK_EQ(invoke<int>(*plan.result[0], { box(0) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(10) }), 10);
    }

    {
        // Generated from:
        //    (module
        //     (func "dumb-mult" (param $x i32) (param $y i32) (result i32) (local $total i32) (local $i i32) (local $j i32)
        //      (set_local $total (i32.const 0))
        //      (set_local $i (i32.const 0))
        //      (block
        //       (loop
        //        (br_if 1 (i32.eq (get_local $i) (get_local $x)))
        //        (set_local $j (i32.const 0))
        //        (set_local $i (i32.add (get_local $i) (i32.const 1)))
        //        (loop
        //         (br_if 1 (i32.eq (get_local $j) (get_local $y)))
        //         (set_local $total (i32.add (get_local $total) (i32.const 1)))
        //         (set_local $j (i32.add (get_local $j) (i32.const 1)))
        //         (br 0)
        //         )
        //        )
        //       )
        //      (return (get_local $total))
        //      )
        //     )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x86, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x01, 0x01, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f, 0x6e,
            0x82, 0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x86, 0x80, 0x80,
            0x00, 0x01, 0x00, 0x03, 0x73, 0x75, 0x6d, 0x04, 0x63, 0x6f, 0x64, 0x65, 0xae, 0x80, 0x80, 0x00,
            0x01, 0xa9, 0x80, 0x80, 0x00, 0x01, 0x01, 0x01, 0x10, 0x00, 0x15, 0x01, 0x02, 0x01, 0x14, 0x00,
            0x10, 0x00, 0x4d, 0x07, 0x00, 0x00, 0x14, 0x00, 0x14, 0x01, 0x40, 0x15, 0x01, 0x14, 0x00, 0x10,
            0x01, 0x41, 0x15, 0x00, 0x06, 0x00, 0x01, 0x0f, 0x0f, 0x14, 0x01, 0x09, 0x01, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        CHECK_EQ(invoke<int>(*plan.result[0], { box(0) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(1) }), 1);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(2)}), 3);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(100) }), 5050);
    }

    {
        // Generated from:
        //    (module
        //     (func "dumb-mult" (param $x i32) (param $y i32) (result i32) (local $total i32) (local $i i32) (local $j i32)
        //      (set_local $total (i32.const 0))
        //      (set_local $i (i32.const 0))
        //      (block (loop
        //              (br_if 1 (i32.eq (get_local $i) (get_local $x)))
        //              (set_local $j (i32.const 0))
        //              (set_local $i (i32.add (get_local $i) (i32.const 1)))
        //              (loop
        //               (br_if 1 (i32.eq (get_local $j) (get_local $y)))
        //               (set_local $total (i32.add (get_local $total) (i32.const 1)))
        //               (set_local $j (i32.add (get_local $j) (i32.const 1)))
        //               (br 0)
        //               )
        //              ))
        //      (return (get_local $total))
        //      )
        //     )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x87, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x02, 0x01, 0x01, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f,
            0x6e, 0x82, 0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x8c, 0x80,
            0x80, 0x00, 0x01, 0x00, 0x09, 0x64, 0x75, 0x6d, 0x62, 0x2d, 0x6d, 0x75, 0x6c, 0x74, 0x04, 0x63,
            0x6f, 0x64, 0x65, 0xc7, 0x80, 0x80, 0x00, 0x01, 0xc2, 0x80, 0x80, 0x00, 0x01, 0x03, 0x01, 0x10,
            0x00, 0x15, 0x02, 0x10, 0x00, 0x15, 0x03, 0x01, 0x02, 0x14, 0x03, 0x14, 0x00, 0x4d, 0x07, 0x00,
            0x01, 0x10, 0x00, 0x15, 0x04, 0x14, 0x03, 0x10, 0x01, 0x40, 0x15, 0x03, 0x02, 0x14, 0x04, 0x14,
            0x01, 0x4d, 0x07, 0x00, 0x01, 0x14, 0x02, 0x10, 0x01, 0x40, 0x15, 0x02, 0x14, 0x04, 0x10, 0x01,
            0x40, 0x15, 0x04, 0x06, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x14, 0x02, 0x09, 0x01, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        CHECK_EQ(invoke<int>(*plan.result[0], { box(0), box(1) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(1), box(0) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(2), box(1) }), 2);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(1), box(2) }), 2);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(2), box(2) }), 4);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(2), box(6) }), 12);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(100), box(6) }), 600);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(100), box(100) }), 10000);
    }

    {
        // Generated from:
        //    (module
        //     (func "dumb-less-than" (param $x i32) (param $y i32) (result i32)
        //      (loop
        //       (block
        //        (block
        //         (br_if 0 (i32.eq (get_local $x) (i32.const 0)))
        //         (br 1)
        //         )
        //        (return (i32.const 1))
        //        )
        //       (block
        //        (block
        //         (br_if 0 (i32.eq (get_local $x) (get_local $y)))
        //         (br 1)
        //         )
        //        (return (i32.const 0))
        //        )
        //       (set_local $x (i32.sub (get_local $x) (i32.const 1)))
        //       (br 0)
        //       )
        //      )
        //     )
        Vector<uint8_t> vector = {
            0x00, 0x61, 0x73, 0x6d, 0x0c, 0x00, 0x00, 0x00, 0x04, 0x74, 0x79, 0x70, 0x65, 0x87, 0x80, 0x80,
            0x00, 0x01, 0x40, 0x02, 0x01, 0x01, 0x01, 0x01, 0x08, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f,
            0x6e, 0x82, 0x80, 0x80, 0x00, 0x01, 0x00, 0x06, 0x65, 0x78, 0x70, 0x6f, 0x72, 0x74, 0x91, 0x80,
            0x80, 0x00, 0x01, 0x00, 0x0e, 0x64, 0x75, 0x6d, 0x62, 0x2d, 0x6c, 0x65, 0x73, 0x73, 0x2d, 0x74,
            0x68, 0x61, 0x6e, 0x04, 0x63, 0x6f, 0x64, 0x65, 0xb9, 0x80, 0x80, 0x00, 0x01, 0xb4, 0x80, 0x80,
            0x00, 0x00, 0x02, 0x01, 0x01, 0x14, 0x00, 0x14, 0x01, 0x4d, 0x07, 0x00, 0x00, 0x06, 0x00, 0x01,
            0x0f, 0x10, 0x00, 0x09, 0x01, 0x0f, 0x01, 0x01, 0x14, 0x00, 0x10, 0x00, 0x4d, 0x07, 0x00, 0x00,
            0x06, 0x00, 0x01, 0x0f, 0x10, 0x01, 0x09, 0x01, 0x0f, 0x14, 0x00, 0x10, 0x01, 0x41, 0x15, 0x00,
            0x06, 0x00, 0x00, 0x0f, 0x0f
        };

        Plan plan(*vm, vector);
        if (plan.result.size() != 1 || !plan.result[0]) {
            dataLogLn("Module failed to compile correctly.");
            CRASH();
        }

        // Test this doesn't crash.
        CHECK_EQ(invoke<int>(*plan.result[0], { box(0), box(1) }), 1);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(1), box(0) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(2), box(1) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(1), box(2) }), 1);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(2), box(2) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(1), box(1) }), 0);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(2), box(6) }), 1);
        CHECK_EQ(invoke<int>(*plan.result[0], { box(100), box(6) }), 0);
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

