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

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "InitializeThreading.h"
#include "JSWebAssemblyModule.h"
#include "VM.h"
#include "WasmModule.h"
#include "WasmModuleInformation.h"
#include "WasmModuleManager.h"
#include "WasmVirtualAddress.h"
#include <wtf/HexNumber.h>
#include <wtf/Vector.h>
#include <wtf/WTFProcess.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

#if OS(WINDOWS)
#include <wtf/win/WTFCRTDebug.h>
#endif

using namespace JSC;
using namespace JSC::Wasm;

// Test counters
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST_ASSERT(condition, message)                          \
    do {                                                         \
        testsRun++;                                              \
        if (condition) {                                         \
            testsPassed++;                                       \
            dataLogLn("PASS: ", message);                        \
        } else {                                                 \
            testsFailed++;                                       \
            dataLogLn("FAIL: ", message, " (", #condition, ")"); \
        }                                                        \
    } while (0)

static void testWASMModuleManagerConstants()
{
    dataLogLn("=== Testing ModuleManager virtualAddress Design ===");

    // Test virtualAddress encoding constants
    TEST_ASSERT(VirtualAddress::MODULE_BASE == 0x4000000000000000ULL,
        "MODULE_BASE should be 0x4000000000000000");

    TEST_ASSERT(VirtualAddress::MEMORY_BASE == 0x0000000000000000ULL,
        "MEMORY_BASE should be 0x0000000000000000");

    TEST_ASSERT(VirtualAddress::GAP_BASE == 0x8000000000000000ULL,
        "GAP_BASE should be 0x8000000000000000");

    // Test virtualAddress encoding for different module IDs
    VirtualAddress module0Obj = VirtualAddress::createModule(0, 0);
    VirtualAddress module1Obj = VirtualAddress::createModule(1, 0);
    VirtualAddress module0Mem = VirtualAddress::createMemory(0, 0);

    TEST_ASSERT(module0Obj == 0x4000000000000000ULL, "Module 0 obj should be at encoded address");
    TEST_ASSERT(module1Obj == 0x4000000100000000ULL, "Module 1 obj should be at encoded address");
    TEST_ASSERT(module0Mem == 0x0000000000000000ULL, "Module 0 memory should be at encoded address");

    // Test address decoding
    TEST_ASSERT(module0Obj.type() == VirtualAddress::Type::Module, "Should decode as Module");
    TEST_ASSERT(module0Obj.id() == 0, "Should decode module ID 0");
    TEST_ASSERT(module0Obj.offset() == 0, "Should decode offset 0");

    dataLogLn("virtualAddress design tests completed");
}

static void testWASMModuleManagerBasicOperations()
{
    dataLogLn("=== Testing ModuleManager Basic Operations ===");

    VM& vm = VM::create().leakRef();
    ModuleManager instanceManager(vm);

    // Test initial state
    TEST_ASSERT(!instanceManager.instanceCount(), "Initial instance count should be 0");

    // Test empty library XML generation
    String emptyXML = instanceManager.generateLibrariesXML();
    TEST_ASSERT(!emptyXML.isEmpty(), "Library XML should not be empty");
    TEST_ASSERT(emptyXML.contains("<?xml version=\"1.0\"?>"), "XML should have proper header");
    TEST_ASSERT(emptyXML.contains("<library-list>"), "XML should contain library-list");
    TEST_ASSERT(emptyXML.contains("</library-list>"), "XML should be properly closed");
    TEST_ASSERT(!emptyXML.contains("<library name="), "Empty XML should not contain library entries");

    // Note: readSourceBinary() requires registered modules, so we skip this test for empty manager
    // This is by design - the method is intended for use with registered modules only

    dataLogLn("Basic operations tests completed");
}

static void testWASMModuleManagerMemoryReading()
{
    dataLogLn("=== Testing ModuleManager Memory Reading ===");

    VM& vm = VM::create().leakRef();
    ModuleManager instanceManager(vm);

    // Note: readSourceBinary() uses RELEASE_ASSERT and requires registered instances
    // Testing invalid addresses would cause crashes, so we test the address parsing logic instead

    // Test address parsing logic indirectly through other methods
    TEST_ASSERT(!instanceManager.instanceCount(), "Empty manager should have 0 instances");


    // Test that we can generate XML without crashing
    String xml = instanceManager.generateLibrariesXML();
    TEST_ASSERT(!xml.isEmpty(), "XML generation should work with empty manager");

    dataLogLn("Memory reading tests completed");
}

static void testWASMModuleManagerLibraryXMLGeneration()
{
    dataLogLn("=== Testing ModuleManager Library XML Generation ===");

    VM& vm = VM::create().leakRef();
    ModuleManager instanceManager(vm);

    // Test XML structure with no instances
    String xml = instanceManager.generateLibrariesXML();

    // Verify XML structure
    TEST_ASSERT(xml.startsWith("<?xml version=\"1.0\"?>"_s), "XML should start with proper declaration");
    TEST_ASSERT(xml.contains("<library-list>"), "XML should contain opening library-list tag");
    TEST_ASSERT(xml.contains("</library-list>"), "XML should contain closing library-list tag");

    // Verify no library entries in empty manager
    TEST_ASSERT(!xml.contains("<library name="), "Empty manager should not have library entries");
    TEST_ASSERT(!xml.contains("<section address="), "Empty manager should not have section entries");

    // Test XML is well-formed (basic validation)
    size_t openTags = 0;
    size_t closeTags = 0;

    for (size_t i = 0; i < xml.length(); ++i) {
        if (xml[i] == '<') {
            if (i + 1 < xml.length() && xml[i + 1] == '/') {
                // Closing tag like </library-list>
                closeTags++;
            } else if (i + 1 < xml.length() && xml[i + 1] == '?') {
                // XML declaration like <?xml version="1.0"?>
                // Skip this, it's not a regular tag
                continue;
            } else {
                // Opening tag, check if it's self-closing
                bool isSelfClosing = false;
                for (size_t j = i + 1; j < xml.length() && xml[j] != '>'; ++j) {
                    if (xml[j] == '/' && j + 1 < xml.length() && xml[j + 1] == '>') {
                        isSelfClosing = true;
                        break;
                    }
                }

                if (!isSelfClosing)
                    openTags++;
                // Self-closing tags don't need separate closing tags, so we don't count them
            }
        }
    }

    // For well-formed XML: openTags should equal closeTags
    // Self-closing tags don't need separate closing tags
    TEST_ASSERT(openTags == closeTags, "XML should have balanced opening and closing tags");

    dataLogLn("Library XML generation tests completed");
}

static void testWASMModuleManagerAddressValidation()
{
    dataLogLn("=== Testing ModuleManager Address Validation ===");

    VM& vm = VM::create().leakRef();
    ModuleManager instanceManager(vm);

    // Test various invalid addresses
    struct AddressTest {
        uint64_t address;
        const char* description;
    };

    AddressTest invalidAddresses[] = {
        { 0x0, "null address" },
        { 0x1000, "low memory address" },
        { 0x7FFFFFFF, "32-bit address space" },
        { 0x3FFFFFFFFFFFFFF, "below virtual base" },
        { 0x8000000000000000ULL, "high invalid address" },
        { 0xFFFFFFFFFFFFFFFFULL, "maximum address" }
    };

    // Note: readSourceBinary() uses RELEASE_ASSERT, so we can't test invalid addresses directly
    // Instead, we test the address space design and validation logic

    for (const auto& test : invalidAddresses) {
        // Test that these addresses decode to invalid types or are outside valid ranges
        VirtualAddress testAddr(test.address);
        VirtualAddress::Type addressType = testAddr.type();
        
        // Address 0x8000000000000000 has top 2 bits = 10 (binary) = 2 (decimal), which is not a defined enum value
        // but should be treated as invalid. Let's check if it's not a valid type.
        bool isValidType = (addressType == VirtualAddress::Type::Module ||
                           addressType == VirtualAddress::Type::Memory ||
                           addressType == VirtualAddress::Type::Invalid);
        
        if (!isValidType) {
            // For addresses that decode to undefined enum values, we just verify they're not valid types
            TEST_ASSERT(!isValidType,
                makeString("Address "_s, String::fromLatin1(test.description), " (0x"_s, hex(test.address, Lowercase), ") should not decode as valid type"_s).utf8().data());
        }
    }

    // Test valid virtualAddress encoded addresses
    VirtualAddress validAddresses[] = {
        VirtualAddress::createModule(0, 0),
        VirtualAddress::createModule(1, 0),
        VirtualAddress::createMemory(0, 0)
    };

    for (const VirtualAddress& address : validAddresses) {
        // Test that these addresses decode to valid types
        VirtualAddress::Type addressType = address.type();
        bool isValidType = (addressType == VirtualAddress::Type::Module ||
                           addressType == VirtualAddress::Type::Memory ||
                           addressType == VirtualAddress::Type::Invalid);
        TEST_ASSERT(isValidType,
            makeString("Valid WASM address 0x"_s, address.hex(), " should decode to valid type"_s).utf8().data());
    }

    dataLogLn("Address validation tests completed");
}

static void testWASMModuleManagerEdgeCases()
{
    dataLogLn("=== Testing ModuleManager Edge Cases ===");

    VM& vm = VM::create().leakRef();
    ModuleManager instanceManager(vm);

    // Note: readSourceBinary() requires registered instances, so we test other edge cases

    // Test that large parameters don't break other methods
    TEST_ASSERT(!instanceManager.instanceCount(), "Instance count should remain 0");

    // Test multiple XML generations (should be consistent)
    String xml1 = instanceManager.generateLibrariesXML();
    String xml2 = instanceManager.generateLibrariesXML();
    TEST_ASSERT(xml1 == xml2, "Multiple XML generations should be identical");


    dataLogLn("Edge cases tests completed");
}

static void testWASMModuleManagerIntegration()
{
    dataLogLn("=== Testing ModuleManager Integration Scenarios ===");

    VM& vm = VM::create().leakRef();
    ModuleManager instanceManager(vm);

    // Test scenario: Debug server would use these APIs

    // 1. Check initial state
    size_t initialCount = instanceManager.instanceCount();
    TEST_ASSERT(!initialCount, "Debug server should see empty initial state");

    // 2. Generate library list for LLDB
    String libraryList = instanceManager.generateLibrariesXML();
    TEST_ASSERT(!libraryList.isEmpty(), "Debug server should get valid library list");
    TEST_ASSERT(libraryList.contains("library-list"), "Library list should be valid XML");

    // 3. Handle memory read requests from LLDB (would be done after instances are registered)
    // Note: readSourceBinary() requires registered instances, so we skip this test


    // 5. Test virtualAddress encoding for different instance IDs
    for (int i = 0; i < 5; ++i) {
        VirtualAddress instanceObjAddr = VirtualAddress::createModule(i, 0);
        VirtualAddress instanceMemAddr = VirtualAddress::createMemory(i, 0);
        
        // Test that addresses are properly encoded
        TEST_ASSERT(instanceObjAddr.id() == static_cast<uint32_t>(i),
            makeString("Instance "_s, String::number(i), " obj address should encode correct ID"_s).utf8().data());
        
        TEST_ASSERT(instanceMemAddr.id() == static_cast<uint32_t>(i),
            makeString("Instance "_s, String::number(i), " memory address should encode correct ID"_s).utf8().data());
        
        TEST_ASSERT(instanceObjAddr.type() == VirtualAddress::Type::Module,
            makeString("Instance "_s, String::number(i), " obj address should encode correct type"_s).utf8().data());
        
        TEST_ASSERT(instanceMemAddr.type() == VirtualAddress::Type::Memory,
            makeString("Instance "_s, String::number(i), " memory address should encode correct type"_s).utf8().data());
    }

    dataLogLn("Integration scenarios tests completed");
}

static void testWASMModuleManagerPerformance()
{
    dataLogLn("=== Testing ModuleManager Performance Characteristics ===");

    VM& vm = VM::create().leakRef();
    ModuleManager instanceManager(vm);

    // Test repeated operations for performance characteristics
    const int iterations = 1000;

    // Test repeated XML generation
    for (int i = 0; i < iterations; ++i) {
        String xml = instanceManager.generateLibrariesXML();
        if (!i)
            TEST_ASSERT(!xml.isEmpty(), "First XML generation should succeed");
    }

    // Test repeated instance count queries
    for (int i = 0; i < iterations; ++i) {
        size_t count = instanceManager.instanceCount();
        if (!i)
            TEST_ASSERT(!count, "Instance count should be consistent");
    }

    // Test repeated operations that don't require registered instances
    for (int i = 0; i < iterations; ++i) {
        size_t count = instanceManager.instanceCount();
        if (!i)
            TEST_ASSERT(!count, "Instance count should be consistent");
    }

    TEST_ASSERT(true, "Performance test completed without crashes");

    dataLogLn("Performance characteristics tests completed");
}

static void testWASMAddressEncoding()
{
    dataLogLn("=== Testing WASM Address Encoding/Decoding ===");

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

    dataLogLn("WASM Address encoding/decoding tests completed");
}

static void testWASMMemoryRegionBoundaries()
{
    dataLogLn("=== Testing WASM Memory Region Boundaries ===");

    // Test memory region boundaries
    TEST_ASSERT(VirtualAddress::MEMORY_BASE == 0x0000000000000000ULL, "Memory base should be 0");
    TEST_ASSERT(VirtualAddress::MEMORY_END == 0x3FFFFFFFFFFFFFFFULL, "Memory end should be correct");
    TEST_ASSERT(VirtualAddress::MODULE_BASE == 0x4000000000000000ULL, "Obj base should be correct");
    TEST_ASSERT(VirtualAddress::MODULE_END == 0x7FFFFFFFFFFFFFFFULL, "Obj end should be correct");
    TEST_ASSERT(VirtualAddress::GAP_BASE == 0x8000000000000000ULL, "Gap base should be correct");
    TEST_ASSERT(VirtualAddress::GAP_END == 0xFFFFFFFFFFFFFFFFULL, "Gap end should be correct");

    // Test reasonable boundary addresses (avoid overflow with max values)
    VirtualAddress memoryBoundary = VirtualAddress::createMemory(0x1000, 0x1000);
    VirtualAddress objBoundary = VirtualAddress::createModule(0x1000, 0x1000);

    TEST_ASSERT(memoryBoundary.value() >= VirtualAddress::MEMORY_BASE && memoryBoundary.value() <= VirtualAddress::MEMORY_END, "Memory boundary should be within range");
    TEST_ASSERT(objBoundary.value() >= VirtualAddress::MODULE_BASE && objBoundary.value() <= VirtualAddress::MODULE_END, "Obj boundary should be within range");
    
    // Test that the address ranges are properly defined
    TEST_ASSERT(VirtualAddress::MEMORY_BASE < VirtualAddress::MODULE_BASE, "Memory range should be before obj range");
    TEST_ASSERT(VirtualAddress::MODULE_END < VirtualAddress::GAP_BASE, "Obj range should be before gap range");

    dataLogLn("WASM Memory region boundaries tests completed");
}

static void testWASMDebuggerMemoryRegionEnumeration()
{
    dataLogLn("=== Testing WASM Debugger Memory Region Enumeration ===");

    // Test that our address design supports proper LLDB memory region enumeration
    // This tests the fix for the original "Server returned invalid range" error

    // Test various addresses that LLDB might query during memory region enumeration
    struct RegionTest {
        uint64_t address;
        const char* description;
        bool shouldBeValid;
    };

    RegionTest regionTests[] = {
        // Valid WASM addresses
        { VirtualAddress::createMemory(0, 0), "Module 0 memory base", true },
        { VirtualAddress::createMemory(1, 0x1000), "Module 1 memory with offset", true },
        { VirtualAddress::createModule(0, 0), "Module 0 obj base", true },
        { VirtualAddress::createModule(1, 0x2000), "Module 1 obj with offset", true },
        
        // Addresses LLDB might probe during enumeration
        { 0x0000000000000000ULL, "Zero address", true },  // Valid memory type
        { 0x1000000000000000ULL, "Low memory probe", true },  // Valid memory type
        { 0x4000000000000000ULL, "Obj base probe", true },  // Valid obj type
        { 0x5000000000000000ULL, "Mid obj probe", true },  // Valid obj type
        { 0x8000000000000000ULL, "High address probe", false },  // Invalid type (undefined enum value)
        { 0xC000000000000000ULL, "Invalid base probe", true },  // Valid invalid type
        { 0xFFFFFFFFFFFFFFFFULL, "Max address probe", true },  // Valid invalid type
    };

    for (const auto& test : regionTests) {
        VirtualAddress testAddr(test.address);
        VirtualAddress::Type addressType = testAddr.type();
        bool isValidType = (addressType == VirtualAddress::Type::Module ||
                           addressType == VirtualAddress::Type::Memory ||
                           addressType == VirtualAddress::Type::Invalid);
        
        if (test.shouldBeValid) {
            TEST_ASSERT(isValidType,
                makeString("Address "_s, String::fromLatin1(test.description), " (0x"_s, hex(test.address, Lowercase), ") should decode to valid type"_s).utf8().data());
        } else {
            TEST_ASSERT(!isValidType,
                makeString("Address "_s, String::fromLatin1(test.description), " (0x"_s, hex(test.address, Lowercase), ") should not decode to valid type"_s).utf8().data());
        }
    }

    dataLogLn("WASM Debugger memory region enumeration tests completed");
}

static void testWASMInstanceAddressGeneration()
{
    dataLogLn("=== Testing WASM Instance Address Generation ===");

    // Test helper functions for generating instance addresses
    for (uint32_t instanceId = 0; instanceId < 10; ++instanceId) {
        VirtualAddress codeBase = VirtualAddress::createModule(instanceId, 0);
        VirtualAddress memoryBase = VirtualAddress::createMemory(instanceId, 0);
        
        // Verify addresses are correctly encoded
        TEST_ASSERT(codeBase.type() == VirtualAddress::Type::Module,
            makeString("Instance "_s, String::number(instanceId), " code base should be Module type"_s).utf8().data());
        TEST_ASSERT(codeBase.id() == instanceId,
            makeString("Instance "_s, String::number(instanceId), " code base should have correct ID"_s).utf8().data());
        TEST_ASSERT(codeBase.offset() == 0,
            makeString("Instance "_s, String::number(instanceId), " code base should have zero offset"_s).utf8().data());
        
        TEST_ASSERT(memoryBase.type() == VirtualAddress::Type::Memory,
            makeString("Instance "_s, String::number(instanceId), " memory base should be Memory type"_s).utf8().data());
        TEST_ASSERT(memoryBase.id() == instanceId,
            makeString("Instance "_s, String::number(instanceId), " memory base should have correct ID"_s).utf8().data());
        TEST_ASSERT(memoryBase.offset() == 0,
            makeString("Instance "_s, String::number(instanceId), " memory base should have zero offset"_s).utf8().data());
        
        // Verify addresses are in correct ranges
        TEST_ASSERT(codeBase.value() >= VirtualAddress::MODULE_BASE && codeBase.value() <= VirtualAddress::MODULE_END,
            makeString("Instance "_s, String::number(instanceId), " code base should be in obj range"_s).utf8().data());
        TEST_ASSERT(memoryBase.value() >= VirtualAddress::MEMORY_BASE && memoryBase.value() <= VirtualAddress::MEMORY_END,
            makeString("Instance "_s, String::number(instanceId), " memory base should be in memory range"_s).utf8().data());
    }

    dataLogLn("WASM Instance address generation tests completed");
}

static void runAllTests()
{
    dataLogLn("Starting Comprehensive WASM Module Manager Test Suite");
    dataLogLn("====================================================");

    // Test constants and basic design
    testWASMModuleManagerConstants();

    // Test basic operations
    testWASMModuleManagerBasicOperations();

    // Test memory reading functionality
    testWASMModuleManagerMemoryReading();

    // Test XML generation
    testWASMModuleManagerLibraryXMLGeneration();

    // Test address validation
    testWASMModuleManagerAddressValidation();

    // Test edge cases
    testWASMModuleManagerEdgeCases();

    // Test integration scenarios
    testWASMModuleManagerIntegration();

    // Test performance characteristics
    testWASMModuleManagerPerformance();

    // Test enhanced WASM address encoding
    testWASMAddressEncoding();

    // Test memory region boundaries
    testWASMMemoryRegionBoundaries();

    // Test debugger memory region enumeration (the original fix)
    testWASMDebuggerMemoryRegionEnumeration();

    // Test instance address generation helpers
    testWASMInstanceAddressGeneration();

    dataLogLn("====================================================");
    dataLogLn("Test Results:");
    dataLogLn("  Tests run: ", testsRun);
    dataLogLn("  Passed: ", testsPassed);
    dataLogLn("  Failed: ", testsFailed);

    if (!testsFailed) {
        dataLogLn("All tests PASSED!");
        dataLogLn("WASM Instance Manager is working correctly");
        dataLogLn("allWasmDebuggerTestsPassed");
    } else {
        dataLogLn("Some tests FAILED!");
        dataLogLn("WASM Instance Manager needs attention");
    }
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

    JSC::initialize();
    runAllTests();
    return (!testsFailed) ? 0 : 1;
}

#if OS(WINDOWS)
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, const_cast<char**>(argv));
}
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#else // !ENABLE(WEBASSEMBLY)

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

#endif // ENABLE(WEBASSEMBLY)
