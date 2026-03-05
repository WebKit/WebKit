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

#include <wtf/DataLog.h>

#if OS(WINDOWS)
#include <windows.h>
#include <wtf/win/WTFCRTDebug.h>
#endif

#if ENABLE(WEBASSEMBLY_DEBUGGER)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "ExecutionHandlerIdleStopTest.h"
#include "ExecutionHandlerTest.h"
#include "GDBPacketParserTest.h"
#include "InitializeThreading.h"
#include "Options.h"
#include "TestUtilities.h"
#include "WasmVirtualAddress.h"
#include <wtf/HexNumber.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

using namespace JSC;
using namespace JSC::Wasm;

// Test counter for VirtualAddress tests (assertion-based, crash on failure)
static int testsFailed = 0;

static void testWASMVirtualAddressConstants()
{
    dataLogLn("=== Testing VirtualAddress Design ===");

    // Test virtualAddress encoding constants
    TEST_ASSERT(VirtualAddress::MODULE_BASE == 0x4000000000000000ULL,
        "MODULE_BASE should be 0x4000000000000000");

    TEST_ASSERT(VirtualAddress::MEMORY_BASE == 0x0000000000000000ULL,
        "MEMORY_BASE should be 0x0000000000000000");

    TEST_ASSERT(VirtualAddress::INVALID_BASE == 0x8000000000000000ULL,
        "INVALID_BASE should be 0x8000000000000000");

    // Test virtualAddress encoding for different module IDs
    VirtualAddress module0Obj = VirtualAddress::createModule(0, 0);
    VirtualAddress module1Obj = VirtualAddress::createModule(1, 0);
    VirtualAddress module0Mem = VirtualAddress::createMemory(0, 0);

    TEST_ASSERT(module0Obj == 0x4000000000000000ULL, "Module 0 obj should be at encoded address");
    TEST_ASSERT(module1Obj == 0x4000000100000000ULL, "Module 1 obj should be at encoded address");
    TEST_ASSERT(module0Mem == 0x0000000000000000ULL, "Module 0 memory should be at encoded address");

    // Test address decoding
    TEST_ASSERT(module0Obj.type() == VirtualAddress::Type::Module, "Should decode as Module");
    TEST_ASSERT(!module0Obj.id(), "Should decode module ID 0");
    TEST_ASSERT(!module0Obj.offset(), "Should decode offset 0");

    dataLogLn("VirtualAddress design tests completed");
}

static void testWASMVirtualAddressEncoding()
{
    dataLogLn("=== Testing VirtualAddress Encoding/Decoding ===");

    // Test all address type combinations
    struct AddressTest {
        VirtualAddress::Type type;
        uint32_t moduleId;
        uint32_t offset;
        const char* description;
    };

    AddressTest tests[] = {
        { VirtualAddress::Type::Memory, 0, 0, "Module 0 memory base" },
        { VirtualAddress::Type::Memory, 1, 0x1000, "Module 1 memory offset" },
        { VirtualAddress::Type::Memory, 0x1000, 0x2000, "Module 4096 memory offset" },
        { VirtualAddress::Type::Module, 0, 0, "Module 0 obj base" },
        { VirtualAddress::Type::Module, 1, 0x2000, "Module 1 obj offset" },
        { VirtualAddress::Type::Module, 0x2000, 0x3000, "Module 8192 obj offset" }
    };

    for (const auto& test : tests) {
        VirtualAddress encoded = (test.type == VirtualAddress::Type::Memory)
            ? VirtualAddress::createMemory(test.moduleId, test.offset)
            : VirtualAddress::createModule(test.moduleId, test.offset);

        VirtualAddress::Type decodedType = encoded.type();
        uint32_t decodedId = encoded.id();
        uint32_t decodedOffset = encoded.offset();

        TEST_ASSERT(decodedType == test.type,
            makeString("Address encoding/decoding type mismatch for "_s, String::fromLatin1(test.description)).utf8().data());
        TEST_ASSERT(decodedId == test.moduleId,
            makeString("Address encoding/decoding ID mismatch for "_s, String::fromLatin1(test.description)).utf8().data());
        TEST_ASSERT(decodedOffset == test.offset,
            makeString("Address encoding/decoding offset mismatch for "_s, String::fromLatin1(test.description)).utf8().data());
    }

    dataLogLn("VirtualAddress encoding/decoding tests completed");
}

static void testWASMVirtualAddressBoundaries()
{
    dataLogLn("=== Testing VirtualAddress Boundaries ===");

    // Test memory region boundaries
    TEST_ASSERT(VirtualAddress::MEMORY_BASE == 0x0000000000000000ULL, "Memory base should be 0");
    TEST_ASSERT(VirtualAddress::MEMORY_END == 0x3FFFFFFFFFFFFFFFULL, "Memory end should be correct");
    TEST_ASSERT(VirtualAddress::MODULE_BASE == 0x4000000000000000ULL, "Module base should be correct");
    TEST_ASSERT(VirtualAddress::MODULE_END == 0x7FFFFFFFFFFFFFFFULL, "Module end should be correct");
    TEST_ASSERT(VirtualAddress::INVALID_BASE == 0x8000000000000000ULL, "Invalid base should be correct");
    TEST_ASSERT(VirtualAddress::INVALID_END == 0xFFFFFFFFFFFFFFFFULL, "Invalid end should be correct");

    // Test reasonable boundary addresses (avoid overflow with max values)
    VirtualAddress memoryBoundary = VirtualAddress::createMemory(0x1000, 0x1000);
    VirtualAddress moduleBoundary = VirtualAddress::createModule(0x1000, 0x1000);

    TEST_ASSERT(memoryBoundary.value() >= VirtualAddress::MEMORY_BASE && memoryBoundary.value() <= VirtualAddress::MEMORY_END, "Memory boundary should be within range");
    TEST_ASSERT(moduleBoundary.value() >= VirtualAddress::MODULE_BASE && moduleBoundary.value() <= VirtualAddress::MODULE_END, "Module boundary should be within range");

    // Test that the address ranges are properly defined
    TEST_ASSERT(VirtualAddress::MEMORY_BASE < VirtualAddress::MODULE_BASE, "Memory range should be before module range");
    TEST_ASSERT(VirtualAddress::MODULE_END < VirtualAddress::INVALID_BASE, "Module range should be before invalid range");

    dataLogLn("VirtualAddress boundaries tests completed");
}

static void testWASMVirtualAddressLLDBEnumeration()
{
    dataLogLn("=== Testing VirtualAddress LLDB Enumeration ===");

    struct RegionTest {
        uint64_t address;
        const char* description;
        bool shouldBeValid;
    };

    RegionTest regionTests[] = {
        // Core WASM addresses
        { VirtualAddress::createMemory(0, 0), "Module 0 memory base", true },
        { VirtualAddress::createModule(0, 0), "Module 0 module base", true },
        { 0x8000000000000000ULL, "Invalid type probe", true }, // Invalid type (0x02)
        { 0xC000000000000000ULL, "Invalid2 type probe", true }, // Invalid2 type (0x03)
    };

    for (const auto& test : regionTests) {
        VirtualAddress testAddr(test.address);
        VirtualAddress::Type addressType = testAddr.type();
        bool isValidType = (addressType == VirtualAddress::Type::Module || addressType == VirtualAddress::Type::Memory || testAddr.isInvalidType());

        if (test.shouldBeValid) {
            TEST_ASSERT(isValidType,
                makeString("Address "_s, String::fromLatin1(test.description), " (0x"_s, hex(test.address, Lowercase), ") should decode to valid type"_s).utf8().data());
        } else {
            TEST_ASSERT(!isValidType,
                makeString("Address "_s, String::fromLatin1(test.description), " (0x"_s, hex(test.address, Lowercase), ") should not decode to valid type"_s).utf8().data());
        }
    }

    dataLogLn("VirtualAddress LLDB enumeration tests completed");
}

static void testWASMVirtualAddressEdgeCases()
{
    dataLogLn("=== Testing VirtualAddress Edge Cases ===");

    // Test maximum values for each field
    uint32_t maxId = 0x3FFFFFFF; // 30 bits
    uint32_t maxOffset = 0xFFFFFFFF; // 32 bits

    // Test maximum ID values
    VirtualAddress maxMemoryId = VirtualAddress::createMemory(maxId, 0);
    VirtualAddress maxModuleId = VirtualAddress::createModule(maxId, 0);

    TEST_ASSERT(maxMemoryId.id() == maxId, "Should handle maximum memory ID");
    TEST_ASSERT(maxModuleId.id() == maxId, "Should handle maximum module ID");
    TEST_ASSERT(maxMemoryId.type() == VirtualAddress::Type::Memory, "Max ID should preserve memory type");
    TEST_ASSERT(maxModuleId.type() == VirtualAddress::Type::Module, "Max ID should preserve module type");

    // Test maximum offset values
    VirtualAddress maxMemoryOffset = VirtualAddress::createMemory(0, maxOffset);
    VirtualAddress maxModuleOffset = VirtualAddress::createModule(0, maxOffset);

    TEST_ASSERT(maxMemoryOffset.offset() == maxOffset, "Should handle maximum memory offset");
    TEST_ASSERT(maxModuleOffset.offset() == maxOffset, "Should handle maximum module offset");

    // Test Invalid type addresses
    VirtualAddress invalidAddr1(0x8000000000000000ULL);
    VirtualAddress invalidAddr2(0xC000000000000000ULL);
    TEST_ASSERT(invalidAddr1.type() == VirtualAddress::Type::Invalid, "Should decode as Invalid type");
    TEST_ASSERT(invalidAddr2.type() == VirtualAddress::Type::Invalid2, "Should decode as Invalid2 type");
    TEST_ASSERT(invalidAddr1.isInvalidType(), "Invalid address should be recognized as invalid");
    TEST_ASSERT(invalidAddr2.isInvalidType(), "Invalid2 address should be recognized as invalid");

    // Test address range boundaries precisely
    VirtualAddress memoryEnd(VirtualAddress::MEMORY_END);
    VirtualAddress moduleStart(VirtualAddress::MODULE_BASE);
    VirtualAddress moduleEnd(VirtualAddress::MODULE_END);
    VirtualAddress invalidStart(VirtualAddress::INVALID_BASE);

    TEST_ASSERT(memoryEnd.type() == VirtualAddress::Type::Memory, "Memory end should be Memory type");
    TEST_ASSERT(moduleStart.type() == VirtualAddress::Type::Module, "Module start should be Module type");
    TEST_ASSERT(moduleEnd.type() == VirtualAddress::Type::Module, "Module end should be Module type");
    TEST_ASSERT(invalidStart.type() == VirtualAddress::Type::Invalid, "Invalid start should be Invalid type");

    dataLogLn("VirtualAddress edge cases tests completed");
}

static void testWASMVirtualAddressHashTraits()
{
    dataLogLn("=== Testing VirtualAddress Hash Traits ===");

    // Test empty value
    VirtualAddress emptyAddr = WTF::HashTraits<VirtualAddress>::emptyValue();
    TEST_ASSERT(!emptyAddr.value(), "Empty value should be 0");

    // Test deleted value
    VirtualAddress deletedAddr;
    WTF::HashTraits<VirtualAddress>::constructDeletedValue(deletedAddr);
    TEST_ASSERT(WTF::HashTraits<VirtualAddress>::isDeletedValue(deletedAddr), "Should recognize deleted value");
    TEST_ASSERT(deletedAddr.value() == std::numeric_limits<uint64_t>::max(), "Deleted value should be max uint64");

    // Test hash function consistency
    VirtualAddress addr1 = VirtualAddress::createModule(123, 456);
    VirtualAddress addr2 = VirtualAddress::createModule(123, 456);
    VirtualAddress addr3 = VirtualAddress::createModule(124, 456);

    unsigned hash1 = WTF::DefaultHash<VirtualAddress>::hash(addr1);
    unsigned hash2 = WTF::DefaultHash<VirtualAddress>::hash(addr2);
    unsigned hash3 = WTF::DefaultHash<VirtualAddress>::hash(addr3);

    TEST_ASSERT(hash1 == hash2, "Equal addresses should have equal hashes");
    TEST_ASSERT(hash1 != hash3, "Different addresses should have different hashes");

    // Test equality function
    TEST_ASSERT(WTF::DefaultHash<VirtualAddress>::equal(addr1, addr2), "Equal addresses should be equal");
    TEST_ASSERT(!WTF::DefaultHash<VirtualAddress>::equal(addr1, addr3), "Different addresses should not be equal");

    dataLogLn("VirtualAddress hash traits tests completed");
}

static void testWASMVirtualAddressOperators()
{
    dataLogLn("=== Testing VirtualAddress Operators ===");

    VirtualAddress addr = VirtualAddress::createModule(42, 1000);
    uint64_t expectedValue = 0x4000002A000003E8ULL; // Manually calculated

    // Test uint64_t conversion operator
    uint64_t convertedValue = static_cast<uint64_t>(addr);
    TEST_ASSERT(convertedValue == expectedValue, "uint64_t conversion should work correctly");
    TEST_ASSERT(convertedValue == addr.value(), "Conversion should match value() method");

    // Test operator uint64_t() directly
    uint64_t directConversion = addr;
    TEST_ASSERT(directConversion == expectedValue, "Direct conversion should work");

    // Test hex() method consistency
    String hexStr = addr.hex();
    String expectedHex = makeString(WTF::hex(expectedValue, WTF::Lowercase));
    TEST_ASSERT(hexStr == expectedHex, "hex() method should match expected format");

    dataLogLn("VirtualAddress operators tests completed");
}

static int runAllTests()
{
    dataLogLn("Starting WASM Debugger Test Suite");
    dataLogLn("===============================================");

    dataLogLn("\n--- GDB Packet Parser Tests ---");
    testGDBPacketParser();

    dataLogLn("\n--- VirtualAddress Infrastructure Tests ---");
    testWASMVirtualAddressConstants();
    testWASMVirtualAddressEncoding();
    testWASMVirtualAddressBoundaries();
    testWASMVirtualAddressLLDBEnumeration();
    testWASMVirtualAddressEdgeCases();
    testWASMVirtualAddressHashTraits();
    testWASMVirtualAddressOperators();

    dataLogLn("\n--- WASM Debug Info Tests ---");
    int debugInfoTestsFailed = testWasmDebugInfo();

    dataLogLn("\n--- WASM Debugger Execution Handler Tests ---");
    int executionHandlerTestsFailed = testExecutionHandler();

    dataLogLn("\n--- WASM Debugger Idle VM Stress Tests ---");
    int idleStopTestsFailed = testExecutionHandlerIdleStop();

    dataLogLn("===============================================");
    dataLogLn("Combined Test Results:");
    dataLogLn("  VirtualAddress Tests - PASSED (assertion-based)");
    dataLogLn("  WASM Debug Info Tests - See detailed results above");
    dataLogLn("  WASM Debugger Stress Tests - See detailed results above");
    dataLogLn("  WASM Debugger Idle VM Tests - See detailed results above");
    dataLogLn("  Total Failures: ", testsFailed, " (VirtualAddress) + ", debugInfoTestsFailed, " (Debug Info) + ", executionHandlerTestsFailed, " (Stress) + ", idleStopTestsFailed, " (Idle VM) = ", testsFailed + debugInfoTestsFailed + executionHandlerTestsFailed + idleStopTestsFailed);

    int totalFailures = testsFailed + debugInfoTestsFailed + executionHandlerTestsFailed + idleStopTestsFailed;
    if (!totalFailures) {
        dataLogLn("All tests PASSED!");
        dataLogLn("WASM debugger infrastructure is working correctly");
        dataLogLn("allWasmDebuggerTestsPassed");
    } else {
        dataLogLn("Some tests FAILED!");
        dataLogLn("WASM debugger infrastructure needs attention");
    }

    return totalFailures;
}

int main(int argc, char** argv)
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

#if OS(WINDOWS)
    // Cygwin calls ::SetErrorMode(SEM_FAILCRITICALERRORS), which we will inherit. This is bad for
    // testing/debugging, as it causes the post-mortem debugger not to be invoked. We reset the
    // error mode here to work around Cygwin's behavior. See <http://webkit.org/b/55222>.
    ::SetErrorMode(0);

    WTF::disableCRTDebugAssertDialog();
#endif

    JSC::Config::configureForTesting();
    JSC::initialize();
    JSC::Options::setOption("enableWasmDebugger=true");
    return runAllTests();
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#else // !ENABLE(WEBASSEMBLY_DEBUGGER)

int main(int argc, char** argv)
{
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);

    dataLogLn("WASM debugger tests are disabled (WEBASSEMBLY not enabled)");
    dataLogLn("allWasmDebuggerTestsPassed");
    return 0;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif

#endif // ENABLE(WEBASSEMBLY_DEBUGGER)
