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
#include "TestUtilities.h"

#include <wtf/DataLog.h>

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include "InitializeThreading.h"
#include "JSLock.h"
#include "Options.h"
#include "VM.h"
#include "WasmIPIntPlan.h"
#include "WasmModuleDebugInfo.h"
#include "WasmModuleInformation.h"
#include "WasmOps.h"
#include "WasmTypeDefinition.h"
#include "WasmWorklist.h"
#include <wtf/HexNumber.h>
#include <wtf/ListDump.h>
#include <wtf/text/MakeString.h>

#if OS(WINDOWS)
#include <wtf/win/WTFCRTDebug.h>
#endif

using namespace JSC;
using namespace JSC::Wasm;

namespace WasmDebugInfoTest {

int testsRun = 0;
int testsPassed = 0;
int testsFailed = 0;

VM* g_testVM;

SourceModule SourceModule::create()
{
    SourceModule module;
    // Default: [] -> []
    module.m_params = {};
    module.m_results = {};
    module.m_vm = g_testVM;
    return module;
}

SourceModule& SourceModule::withFunctionType(Vector<uint8_t> params, Vector<uint8_t> results)
{
    m_params = WTFMove(params);
    m_results = WTFMove(results);
    return *this;
}

SourceModule& SourceModule::withLocals(uint32_t count, uint8_t type)
{
    m_localsDeclaration = { 0x01, static_cast<uint8_t>(count), type };
    return *this;
}

SourceModule& SourceModule::withGlobals(bool mutable_)
{
    m_globalSection = {
        0x06, // Section ID: Global
        0x06, // Section length
        0x01, // 1 global
        toLEB128(TypeKind::I32), // Type: i32
        static_cast<uint8_t>(mutable_ ? 0x01 : 0x00), // Mutability
        0x41, 0x00, // i32.const 0
        0x0b // end
    };
    return *this;
}

SourceModule& SourceModule::withTable()
{
    m_tableSection = {
        0x04, // Section ID: Table
        0x04, // Section length
        0x01, // 1 table
        0x70, // Type: funcref
        0x00, // flags: no maximum
        0x01 // min elements: 1
    };
    return *this;
}

SourceModule& SourceModule::withMemory()
{
    m_memorySection = {
        0x05, // Section ID: Memory
        0x03, // Section length
        0x01, // 1 memory
        0x00, // flags: no maximum
        0x01 // min pages: 1
    };
    return *this;
}

SourceModule& SourceModule::withFunctionBody(const Vector<uint8_t>& body)
{
    m_functionBody = body;
    return *this;
}

SourceModule SourceModule::build()
{
    RELEASE_ASSERT(!m_isBuilt);
    m_isBuilt = true;

    // Body size = function body + locals declaration (or just 0x00 if no locals)
    uint32_t localsSize = m_localsDeclaration.isEmpty() ? 1 : m_localsDeclaration.size();
    uint32_t bodySize = m_functionBody.size() + localsSize;
    uint32_t sectionLength = 2 + bodySize; // func count + body size byte + body

    Vector<uint8_t> module = {
        // Magic number: 0x00 0x61 0x73 0x6d
        0x00, 0x61, 0x73, 0x6d,
        // Version: 1
        0x01, 0x00, 0x00, 0x00
    };

    // Type section (1 function type with custom params/results)
    uint8_t typeLength = 4 + m_params.size() + m_results.size();
    module.appendVector(Vector<uint8_t> {
        0x01, // Section ID: Type
        typeLength, // Section length
        0x01, // 1 type
        0x60, // Function type
        static_cast<uint8_t>(m_params.size()) // param count
    });
    module.appendVector(m_params);
    module.append(static_cast<uint8_t>(m_results.size())); // result count
    module.appendVector(m_results);

    // Function section (1 function with type 0)
    module.appendVector(Vector<uint8_t> {
        0x03, // Section ID: Function
        0x02, // Section length
        0x01, // 1 function
        0x00 // Type index 0
    });

    // Optional sections (in correct WASM section order)
    if (!m_tableSection.isEmpty())
        module.appendVector(m_tableSection);

    if (!m_memorySection.isEmpty())
        module.appendVector(m_memorySection);

    if (!m_globalSection.isEmpty())
        module.appendVector(m_globalSection);

    // Export section
    module.appendVector(Vector<uint8_t> {
        0x07, // Section ID: Export
        0x05, // Section length
        0x01, // 1 export
        0x01, // Name length: 1
        'f', // Export name
        0x00, // Export kind: function
        0x00 // Function index 0
    });

    // Code section (function bodies)
    module.appendVector(Vector<uint8_t> {
        0x0a, // Section ID: Code
        static_cast<uint8_t>(sectionLength), // Section length
        0x01, // 1 function body
        static_cast<uint8_t>(bodySize) // Body size
    });

    // functionDataStart points to where the locals count byte is (first byte of function payload)
    functionDataStart = module.size();

    if (m_localsDeclaration.isEmpty()) {
        module.append(0x00); // 0 local variable declarations
        bytecodeStart = module.size(); // bytecodeStart points to where bytecode begins (after locals)
    } else {
        module.appendVector(m_localsDeclaration);
        bytecodeStart = module.size(); // bytecodeStart points to where bytecode begins (after locals)
    }

    module.appendVector(m_functionBody);

    SourceModule result;
    result.bytes = WTFMove(module);
    result.functionDataStart = functionDataStart;
    result.bytecodeStart = bytecodeStart;
    result.m_vm = m_vm;
    return result;
}

SourceModule createWasmModuleWithBytecode(const Vector<uint8_t>& functionBody)
{
    return SourceModule::create()
        .withFunctionType({}, {}) // [] -> []
        .withFunctionBody(functionBody)
        .build();
}

SourceModule createWasmModuleWithLocals(const Vector<uint8_t>& functionBody)
{
    return SourceModule::create()
        .withFunctionType({}, {}) // [] -> []
        .withLocals(1, toLEB128(TypeKind::I32)) // 1 local of type i32
        .withFunctionBody(functionBody)
        .build();
}

SourceModule createWasmModuleWithGlobals(const Vector<uint8_t>& functionBody, bool mutableGlobal)
{
    return SourceModule::create()
        .withFunctionType({}, {}) // [] -> []
        .withGlobals(mutableGlobal)
        .withFunctionBody(functionBody)
        .build();
}

SourceModule createWasmModuleWithTable(const Vector<uint8_t>& functionBody)
{
    return SourceModule::create()
        .withFunctionType({}, {}) // [] -> []
        .withTable()
        .withFunctionBody(functionBody)
        .build();
}

SourceModule createWasmModuleWithMemory(const Vector<uint8_t>& functionBody)
{
    return SourceModule::create()
        .withFunctionType({}, {}) // [] -> []
        .withMemory()
        .withFunctionBody(functionBody)
        .build();
}

bool verifyOpcodeInModule(const SourceModule& sourceModule, JSC::Wasm::OpType expectedOpcode)
{
    uint8_t opcodeByte = static_cast<uint8_t>(expectedOpcode);
    for (size_t i = 0; i < sourceModule.bytes.size(); i++) {
        if (sourceModule.bytes[i] == opcodeByte)
            return true;
    }

    dataLogLn("ERROR: Module does not contain expected opcode 0x", hex(opcodeByte, 2, WTF::HexConversionMode::Lowercase), " ", expectedOpcode);
    return false;
}

bool SourceModule::parseAndVerifyDebugInfo(JSC::Wasm::OpType expectedOpcode, std::initializer_list<std::pair<uint32_t, std::initializer_list<uint32_t>>> mappings) const
{
    RELEASE_ASSERT(m_vm);

    if (!verifyOpcodeInModule(*this, expectedOpcode))
        return false;

    // Convert input mappings (relative to functionBody) to absolute module offsets
    OffsetToNextInstructions expectedMappings;
    for (const auto& [from, tos] : mappings) {
        UncheckedKeyHashSet<uint32_t> targets;
        for (uint32_t to : tos)
            targets.add(bytecodeStart + to);
        expectedMappings.add(bytecodeStart + from, WTFMove(targets));
    }

    JSC::JSLockHolder lock(*m_vm);

    Ref<JSC::Wasm::IPIntPlan> plan = adoptRef(*new JSC::Wasm::IPIntPlan(*m_vm, Vector<uint8_t>(bytes), JSC::Wasm::CompilerMode::FullCompile, JSC::Wasm::Plan::dontFinalize()));
    if (plan->failed()) {
        dataLogLn("ERROR: Failed to parse WASM module: ", plan->errorMessage());
        return false;
    }

    JSC::Wasm::ensureWorklist().enqueue(plan.get());
    plan->waitForCompletion();
    if (plan->failed()) {
        dataLogLn("ERROR: WASM module validation failed: ", plan->errorMessage());
        return false;
    }

    Ref<JSC::Wasm::ModuleInformation> moduleInfo = plan->takeModuleInformation();
    if (moduleInfo->functions.isEmpty()) {
        dataLogLn("ERROR: No functions found in module");
        return false;
    }

    RELEASE_ASSERT(moduleInfo->debugInfo && !moduleInfo->debugInfo->source.isEmpty());

    JSC::Wasm::FunctionCodeIndex functionIndex = JSC::Wasm::FunctionCodeIndex { 0 };
    const auto& function = moduleInfo->functions[functionIndex];
    JSC::Wasm::FunctionSpaceIndex spaceIndex = moduleInfo->toSpaceIndex(functionIndex);
    JSC::Wasm::TypeIndex typeIndex = moduleInfo->typeIndexFromFunctionIndexSpace(spaceIndex);
    Ref typeDefinition = JSC::Wasm::TypeInformation::get(typeIndex);

    auto functionData = moduleInfo->debugInfo->source.subspan(function.start, function.data.size());
    JSC::Wasm::FunctionDebugInfo debugInfo;
    JSC::Wasm::parseForDebugInfo(functionData, typeDefinition, moduleInfo.get(), functionIndex, debugInfo);

    size_t expectedSize = expectedMappings.size();

    // Helper to convert absolute offset to relative (for logging)
    auto toRelative = [&](uint32_t absOffset) { return absOffset - bytecodeStart; };
    auto toRelativeSet = [&](const UncheckedKeyHashSet<uint32_t>& absSet) {
        Vector<uint32_t> relative;
        for (uint32_t abs : absSet)
            relative.append(abs - bytecodeStart);
        std::sort(relative.begin(), relative.end());
        return relative;
    };

    // Check that actual and expected have exactly the same number of entries
    if (debugInfo.offsetToNextInstructions.size() != expectedSize) {
        dataLogLn("ERROR: Expected ", expectedSize, " mapping entries, but found ", debugInfo.offsetToNextInstructions.size());
        dataLogLn("Opcode: ", expectedOpcode);
        dataLogLn("Expected mappings (relative offsets):");
        for (const auto& entry : expectedMappings)
            dataLogLn("  ", toRelative(entry.key), " -> ", listDump(toRelativeSet(entry.value)));
        dataLogLn("Actual mappings (relative offsets):");
        for (const auto& entry : debugInfo.offsetToNextInstructions)
            dataLogLn("  ", toRelative(entry.key), " -> ", listDump(toRelativeSet(entry.value)));
        return false;
    }

    // If no expected mappings, we're done (both are empty)
    if (expectedMappings.isEmpty())
        return true;

    // Verify each expected entry exists and matches exactly
    for (const auto& expectedEntry : expectedMappings) {
        uint32_t expectedOffset = expectedEntry.key;
        const UncheckedKeyHashSet<uint32_t>& expectedNextOffsets = expectedEntry.value;

        UncheckedKeyHashSet<uint32_t>* actualNextOffsets = debugInfo.findNextInstructions(expectedOffset);
        if (!actualNextOffsets) {
            dataLogLn("ERROR: Expected mapping at offset ", toRelative(expectedOffset), " is missing from actual mappings");
            dataLogLn("Opcode: ", expectedOpcode);
            dataLogLn("Expected: ", listDump(toRelativeSet(expectedNextOffsets)));
            dataLogLn("Actual mappings (relative offsets):");
            for (const auto& entry : debugInfo.offsetToNextInstructions)
                dataLogLn("  ", toRelative(entry.key), " -> ", listDump(toRelativeSet(entry.value)));
            return false;
        }

        if (actualNextOffsets->size() != expectedNextOffsets.size()) {
            dataLogLn("ERROR: Offset ", toRelative(expectedOffset), " has ", actualNextOffsets->size(), " next instructions, expected ", expectedNextOffsets.size());
            dataLogLn("Actual: ", listDump(toRelativeSet(*actualNextOffsets)));
            dataLogLn("Expected: ", listDump(toRelativeSet(expectedNextOffsets)));
            return false;
        }

        for (uint32_t expectedNext : expectedNextOffsets) {
            if (!actualNextOffsets->contains(expectedNext)) {
                dataLogLn("ERROR: Offset ", toRelative(expectedOffset), " missing expected next offset ", toRelative(expectedNext));
                dataLogLn("Actual: ", listDump(toRelativeSet(*actualNextOffsets)));
                dataLogLn("Expected: ", listDump(toRelativeSet(expectedNextOffsets)));
                return false;
            }
        }
    }

    return true;
}

static int test()
{
    dataLogLn("Starting WASM Debug Info Test Suite");
    dataLogLn("===============================================");

    JSC::initialize();
    JSC::Options::setOption("enableWasmDebugger=true");

    RefPtr<VM> vm = VM::create();
    g_testVM = vm.get();

    dataLogLn("\n--- Macro-Driven Opcode Coverage Tests ---");
    testAllControlFlowOps();
    testAllUnaryOps();
    testAllBinaryOps();
    testAllMemoryOps();
    testAllSpecialOps();

    // FIXME: Add tests for extended opcode families: Ext1OpType, ExtGCOpType, ExtAtomicOpType, and ExtSIMDOpType

    dataLogLn("===============================================");
    dataLogLn("Test Results:");
    dataLogLn("  Tests run: ", testsRun);
    dataLogLn("  Passed: ", testsPassed);
    dataLogLn("  Failed: ", testsFailed);

    if (!testsFailed) {
        dataLogLn("All tests PASSED!");
        dataLogLn("WASM debug info infrastructure is working correctly");
        dataLogLn("allWasmDebugInfoTestsPassed");
    } else {
        dataLogLn("Some tests FAILED!");
        dataLogLn("WASM debug info infrastructure needs attention");
    }

    g_testVM = nullptr;
    JSLockHolder lock(*vm);
    vm = nullptr;

    return testsFailed;
}

} // namespace WasmDebugInfoTest

int testWasmDebugInfo()
{
    return WasmDebugInfoTest::test();
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#else // !ENABLE(WEBASSEMBLY)

int testWasmDebugInfo()
{
    dataLogLn("WASM debug info tests are disabled (WEBASSEMBLY not enabled)");
    dataLogLn("allWasmDebugInfoTestsPassed");
    return 0;
}

#endif // ENABLE(WEBASSEMBLY)
