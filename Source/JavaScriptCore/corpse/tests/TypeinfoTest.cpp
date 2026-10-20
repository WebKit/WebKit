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

#include "LibJSCToolsTestUtilities.h"
#include <JavaScriptCore/CorpsePlatform.h>

// This file is and LLM-generated test that the dependencies needed for MYA_HEAP are
// installed and the build configuration is correct.

#if ENABLE(MYA_HEAP)

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/CorpseSnapshotDebugInfo.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/Watchpoint.h>
#include <array>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#include <bmalloc/pas_enumerator.h>
#include <bmalloc/pas_heap_lock.h>
#include <bmalloc/pas_root.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#include <dlfcn.h>
#include <lldb/API/LLDB.h>
#include <string>
#include <string_view>
#include <wtf/FastMalloc.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringCommon.h>

#endif // ENABLE(MYA_HEAP)

namespace JSCToolsTest {

#if ENABLE(MYA_HEAP)

namespace TypeinfoTypes {

template<size_t count>
struct Counted {
    std::array<int, count> values;
};

extern Counted<3> countedGlobal;
Counted<3> countedGlobal { };

// A template argument of class type, which DWARF describes by its type alone.
struct Bounds {
    double minimum;
    double maximum;
};

template<Bounds bounds>
struct Bounded {
    double value;
};

struct UsesWideBounds {
    virtual ~UsesWideBounds();
    Bounded<Bounds { -1, 1 }> value;
};

struct DerivesNarrowBounds : Bounded<Bounds { 0, 1 }> {
    virtual ~DerivesNarrowBounds();
};

UsesWideBounds::~UsesWideBounds() = default;
DerivesNarrowBounds::~DerivesNarrowBounds() = default;

} // namespace TypeinfoTypes

class SnapshotDebugInfoTester {
public:
    // Describes, as a heap walk would, every class the debug info of `image` defines. Each class that cannot be
    // described is reported. Returns how many were described.
    static size_t describeClasses(JSC::Corpse::SnapshotDebugInfo& debugInfo, const JSC::Corpse::Image& image)
    {
        lldb::SBTarget& target = debugInfo.m_target;
        lldb::SBModule module;
        for (uint32_t i = 0; i < target.GetNumModules(); ++i) {
            lldb::SBModule candidate = target.GetModuleAtIndex(i);
            if (candidate.GetObjectFileHeaderAddress().GetLoadAddress(target) == image.loadAddress().toTargetVMAddress())
                module = candidate;
        }
        TEST_ASSERT(module.IsValid(), "liblldb opens each image of the build");
        size_t described = 0;
        constexpr uint32_t records = lldb::eTypeClassClass | lldb::eTypeClassStruct | lldb::eTypeClassUnion;
        for (uint32_t i = 0; i < module.GetNumCompileUnits(); ++i) {
            lldb::SBCompileUnit unit = module.GetCompileUnitAtIndex(i);
            lldb::SBTypeList types = unit.GetTypes(records);
            for (uint32_t j = 0; j < types.GetSize(); ++j) {
                lldb::SBType type = types.GetTypeAtIndex(j);
                // The list also has Objective-C classes, which the Objective-C runtime describes, not the debug info.
                if (!(type.GetTypeClass() & records) || !JSC::Corpse::SnapshotDebugInfo::isDefinition(type))
                    continue;
                if (debugInfo.valueType(type, module))
                    ++described;
            }
        }
        return described;
    }
};

namespace {

using JSC::Corpse::Address;
using JSC::Corpse::Memory;
using JSC::Corpse::Snapshot;

constexpr const char* targetString = "typeinfo-target";

UTF8CString readCString(Memory& memory, Address address)
{
    constexpr size_t maxLength = 256;
    Vector<char> characters;
    for (size_t offset = 0; offset < maxLength; ++offset) {
        auto character = memory.ptr<char>(address + offset);
        if (!character)
            return { };
        if (!*character)
            return UTF8CString::fromUTF8(characters.span());
        characters.append(*character);
    }
    return { };
}

// The one complete definition of `name` in `target`'s debug info. Declarations
// have no layout, so they do not count.
lldb::SBType completeType(lldb::SBTarget target, const UTF8CString& name)
{
    lldb::SBTypeList types = target.FindTypes(name.legacyCStringPointer());
    lldb::SBType found;
    unsigned completeCount = 0;
    for (uint32_t index = 0; index < types.GetSize(); ++index) {
        lldb::SBType candidate = types.GetTypeAtIndex(index);
        if (!candidate.IsTypeComplete())
            continue;
        ++completeCount;
        found = candidate;
    }
    TEST_ASSERT_EQ(completeCount, 1u, "the target's debug info has exactly one definition of each class the test reads");
    return completeCount == 1 ? found : lldb::SBType { };
}

lldb::SBTypeMember memberNamed(lldb::SBType type, std::string_view name)
{
    for (uint32_t index = 0; index < type.GetNumberOfFields(); ++index) {
        auto member = type.GetFieldAtIndex(index);
        const char* memberName = member.GetName();
        if (memberName && name == memberName)
            return member;
    }
    TEST_ASSERT(false, "the debug info has every member the test reads");
    return { };
}

lldb::SBType baseAtStart(lldb::SBType type)
{
    for (uint32_t index = 0; index < type.GetNumberOfDirectBaseClasses(); ++index) {
        auto base = type.GetDirectBaseClassAtIndex(index);
        if (!base.GetOffsetInBytes())
            return base.GetType();
    }
    TEST_ASSERT(false, "the debug info puts a base class at the start of the target object");
    return { };
}

Vector<lldb::SBSymbol> symbolsNamed(lldb::SBModule module, std::string_view name)
{
    Vector<lldb::SBSymbol> result;
    size_t count = module.GetNumSymbols();
    for (size_t index = 0; index < count; ++index) {
        lldb::SBSymbol symbol = module.GetSymbolAtIndex(index);
        const char* symbolName = symbol.GetName();
        if (symbolName && name == symbolName)
            result.append(symbol);
    }
    return result;
}

// The type of the global variable a data symbol names, found as the heap walk
// finds the type of each global root: by the symbol's linkage name.
lldb::SBType globalVariableType(lldb::SBTarget target, lldb::SBModule module, lldb::SBSymbol symbol)
{
    const char* linkageName = symbol.GetMangledName();
    if (!linkageName)
        return { };
    lldb::addr_t address = symbol.GetStartAddress().GetFileAddress();
    lldb::SBValueList candidates = module.FindGlobalVariables(target, linkageName, 8);
    for (uint32_t index = 0; index < candidates.GetSize(); ++index) {
        lldb::SBValue candidate = candidates.GetValueAtIndex(index);
        if (candidate.GetAddress().GetFileAddress() == address)
            return candidate.GetType();
    }
    return { };
}

// Every class of each image of the build a heap walk reads is described: this executable and JavaScriptCore.
void describeImages()
{
    Dl_info executable { };
    Dl_info javaScriptCore { };
    TEST_ASSERT(dladdr(reinterpret_cast<void*>(&testTypeinfo), &executable) && executable.dli_fbase, "the test's own image is known");
    TEST_ASSERT(dladdr(reinterpret_cast<void*>(&JSContextGroupCreate), &javaScriptCore) && javaScriptCore.dli_fname, "JavaScriptCore's image is known");
    if (!executable.dli_fbase || !javaScriptCore.dli_fname)
        return;
    Vector<const void*> headers { executable.dli_fbase, javaScriptCore.dli_fbase };

    auto snapshot = takeSnapshot(getpid());
    if (!snapshot)
        return;
    Ref debugInfo = JSC::Corpse::SnapshotDebugInfo::create(*snapshot);
    for (const void* header : headers) {
        const auto& images = snapshot->images();
        size_t index = images.findIf([&](auto& image) {
            return image.loadAddress() == Address { header };
        });
        TEST_ASSERT(index != notFound, "the snapshot lists each image of the build");
        if (index == notFound)
            continue;
        const JSC::Corpse::Image* image = &images[index];
        MonotonicTime start = MonotonicTime::now();
        size_t described = SnapshotDebugInfoTester::describeClasses(debugInfo.get(), *image);
        TEST_ASSERT(described, "each image of the build has classes to describe");
        if (verbose)
            dataLogLn("    described ", described, " classes of ", image->path().legacyCStringPointer(), " in ", (MonotonicTime::now() - start).milliseconds(), " ms");
    }
}

void checkLLDBVersion()
{
    std::string_view version { lldb::SBDebugger::GetVersionString() };
    constexpr std::string_view prefix = "lldb version ";
    size_t start = version.find(prefix);
    TEST_ASSERT(start != std::string_view::npos, "liblldb reports its version");
    if (start == std::string_view::npos)
        return;
    unsigned major = 0;
    for (char character : version.substr(start + prefix.size())) {
        if (character < '0' || character > '9')
            break;
        major = major * 10 + (character - '0');
    }
    TEST_ASSERT_EQ(major, 21u, "the build loads liblldb 21: older ones lack SB API the heap walk calls, and newer ones cannot read WebCore's debug info");
}

// Checks one image of the build, as a heap walk reads every image of its target.
void checkImage(lldb::SBDebugger debugger, const char* path, std::string_view copiedConstantName)
{
    lldb::SBError error;
    lldb::SBTarget target = debugger.CreateTarget(path, nullptr, nullptr, false, error);
    TEST_ASSERT(target.IsValid(), "liblldb opens each image of the build");
    if (!target.IsValid())
        return;
    auto deleteTarget = makeScopeExit([&] {
        debugger.DeleteTarget(target);
    });
    lldb::SBModule module = target.GetModuleAtIndex(0);

#if OS(DARWIN)
    TEST_ASSERT(module.GetUUIDString(), "each image of the build has an identity that refuses a rebuilt copy");
#else
    // liblldb makes up a UUID from the file's CRC for an image with no build-id, which the target's memory does not hold.
    TEST_ASSERT(module.FindSection(".note.gnu.build-id").IsValid(), "each image of the build has an identity that refuses a rebuilt copy");
#endif
    TEST_ASSERT(!module.FindSection(".gdb_index").IsValid(), "each image of the build has its debug info in itself, not split out");
#if OS(DARWIN)
    const char* symbolFileDirectory = module.GetSymbolFileSpec().GetDirectory();
    TEST_ASSERT(symbolFileDirectory && std::string_view { symbolFileDirectory }.find(".dSYM/") != std::string_view::npos,
        "liblldb reads each image of the build from its dSYM");
#endif

    if (copiedConstantName.empty())
        return;
    Vector<lldb::SBSymbol> copies = symbolsNamed(module, copiedConstantName);
    TEST_ASSERT_EQ(copies.size(), 2u, "the image has a copy of the header's constant from each compile unit that uses it");
    for (auto& symbol : copies) {
        TEST_ASSERT(globalVariableType(target, module, symbol).IsValid(),
            "the debug info types each copy of a constant defined in a header, found by its linkage name");
    }
}

void checkImages()
{
    Dl_info executable { };
    Dl_info javaScriptCore { };
    TEST_ASSERT(dladdr(reinterpret_cast<void*>(&testTypeinfo), &executable) && executable.dli_fname, "the test's own path is known");
    TEST_ASSERT(dladdr(reinterpret_cast<void*>(&JSContextGroupCreate), &javaScriptCore) && javaScriptCore.dli_fname, "JavaScriptCore's path is known");
    if (!executable.dli_fname || !javaScriptCore.dli_fname)
        return;

    lldb::SBDebugger::Initialize();
    auto terminateLLDB = makeScopeExit([] {
        lldb::SBDebugger::Terminate();
    });
    lldb::SBDebugger debugger = lldb::SBDebugger::Create();
    auto destroyDebugger = makeScopeExit([&] {
        lldb::SBDebugger::Destroy(debugger);
    });

    checkLLDBVersion();
    TEST_ASSERT(&copiedHeaderConstant != copiedHeaderConstantInUtilities(), "two compile units use their own copies of the header's constant");
    checkImage(debugger, executable.dli_fname, "JSCToolsTest::copiedHeaderConstant");
    checkImage(debugger, javaScriptCore.dli_fname, { });

    lldb::SBError error;
    lldb::SBTarget target = debugger.CreateTarget(executable.dli_fname, nullptr, nullptr, false, error);
    if (!target.IsValid())
        return;
    auto deleteTarget = makeScopeExit([&] {
        debugger.DeleteTarget(target);
    });
    lldb::SBModule module = target.GetModuleAtIndex(0);

    // The heap walk names the type of every global root by its linkage name.
    Vector<lldb::SBSymbol> counted = symbolsNamed(module, "JSCToolsTest::TypeinfoTypes::countedGlobal");
    // The linker drops an unreferenced global's section (--gc-sections), so the test refers to it here.
    Dl_info countedInfo { };
    TEST_ASSERT(dladdr(&TypeinfoTypes::countedGlobal, &countedInfo) && countedInfo.dli_fbase == executable.dli_fbase,
        "the test's global variable is in the test's own image");
    TEST_ASSERT_EQ(counted.size(), 1u, "the test's global variable with external linkage has one symbol");
    lldb::SBType countedType = counted.isEmpty() ? lldb::SBType { } : globalVariableType(target, module, counted[0]);
    TEST_ASSERT(countedType.IsValid(), "the debug info types a global variable with external linkage, found by its linkage name");
    if (countedType.IsValid()) {
        TEST_ASSERT_EQ(countedType.GetTemplateArgumentValue(target, 0).GetValueAsUnsigned(), 3ull,
            "liblldb reads the value of a template's value argument");
    }

    // liblldb 22 gives both instances of Bounded the same template arguments and
    // parses only the first, so a class derived from the second loses its base.
    lldb::SBType wide = target.FindFirstType("JSCToolsTest::TypeinfoTypes::UsesWideBounds");
    TEST_ASSERT(wide.IsValid() && static_cast<size_t>(wide.GetByteSize()) == sizeof(TypeinfoTypes::UsesWideBounds),
        "liblldb reads a class with a member whose type is a template instance with a value of class type");
    lldb::SBType narrow = target.FindFirstType("JSCToolsTest::TypeinfoTypes::DerivesNarrowBounds");
    TEST_ASSERT(narrow.IsValid() && narrow.GetNumberOfDirectBaseClasses() == 1
        && static_cast<size_t>(narrow.GetByteSize()) == sizeof(TypeinfoTypes::DerivesNarrowBounds),
        "liblldb reads a class derived from another instance of the same template, with another value of class type");
}

void analyze(Snapshot& snapshot, Address object)
{
    CString executablePath = snapshot.process()->executablePath();
    TEST_ASSERT(!executablePath.isNull(), "the target's executable path is readable");
    if (executablePath.isNull())
        return;

    lldb::SBDebugger::Initialize();
    auto terminateLLDB = makeScopeExit([] {
        lldb::SBDebugger::Terminate();
    });
    lldb::SBDebugger debugger = lldb::SBDebugger::Create();
    auto destroyDebugger = makeScopeExit([&] {
        lldb::SBDebugger::Destroy(debugger);
    });
    TEST_ASSERT(debugger.IsValid(), "liblldb creates a debugger");
    if (!debugger.IsValid())
        return;

    lldb::SBError error;
    lldb::SBTarget target = debugger.CreateTarget(executablePath.legacyCStringPointer(), nullptr, nullptr, false, error);
    TEST_ASSERT(target.IsValid(), "liblldb opens the target's executable");
    if (!target.IsValid())
        return;
    auto deleteTarget = makeScopeExit([&] {
        debugger.DeleteTarget(target);
    });

    RELEASE_ASSERT_WITH_MESSAGE(!target.GetProcess().IsValid(),
        "Only mya may attach to a process.");

    // Each image describes the types it uses, though another image emits their code.
    lldb::SBType vm = completeType(target, "JSC::VM"_s);
    TEST_ASSERT(vm.IsValid() && static_cast<size_t>(vm.GetByteSize()) == sizeof(JSC::VM),
        "the test's own debug info defines JSC::VM, whose code is in JavaScriptCore");

    lldb::SBType type = completeType(target, "JSC::StringFireDetail"_s);
    if (!type.IsValid())
        return;
    TEST_ASSERT_EQ(static_cast<size_t>(type.GetByteSize()), sizeof(JSC::StringFireDetail),
        "the debug info gives the target object's class its size");

    lldb::SBType base = baseAtStart(type);
    if (!base.IsValid())
        return;
    TEST_ASSERT(std::string_view { base.GetName() } == "JSC::FireDetail",
        "the debug info names the base class the object was reported as");

    lldb::SBTypeMember string = memberNamed(type, "m_string");
    if (!string.IsValid())
        return;
    TEST_ASSERT_EQ(static_cast<size_t>(string.GetType().GetByteSize()), sizeof(const char*),
        "the debug info gives m_string its size");
    auto pointer = snapshot.memory().ptr<uint64_t>(object + string.GetOffsetInBytes());
    TEST_ASSERT(pointer, "the target object's m_string is readable at the offset the debug info gives");
    if (!pointer)
        return;
    CString value = readCString(snapshot.memory(), Address { *pointer }.stripped());
    TEST_ASSERT(!value.isNull() && std::string_view { value.legacyCStringPointer() } == targetString,
        "the corpse holds the target's string at the offset the debug info gives");
}

struct EnumeratedObject {
    void* address;
    size_t size;
    bool found;
};

void* readOwnMemory(pas_enumerator*, void* address, size_t, void*)
{
    return address;
}

// Runs under the heap lock, so it must not allocate from libpas.
void recordObject(pas_enumerator*, void* address, size_t size, pas_enumerator_record_kind kind, void* argument)
{
    auto& wanted = *static_cast<EnumeratedObject*>(argument);
    if (kind == pas_enumerator_object_record && address == wanted.address && size >= wanted.size)
        wanted.found = true;
}

// mya_heap enumerates a target's libpas heap through libpas's own enumerator.
void enumerateLibpasHeap()
{
    constexpr size_t size = 48;
    void* object = fastMalloc(size);
    auto freeObject = makeScopeExit([&] {
        fastFree(object);
    });
    EnumeratedObject wanted { object, size, false };

    pas_heap_lock_lock();
    pas_root* root = pas_root_create();
    pas_enumerator* enumerator = pas_enumerator_create(root, readOwnMemory, nullptr, recordObject, &wanted,
        pas_enumerator_do_not_record_meta_records, pas_enumerator_do_not_record_payload_records, pas_enumerator_record_object_records);
    bool enumerated = enumerator && pas_enumerator_enumerate_all(enumerator);
    if (enumerator)
        pas_enumerator_destroy(enumerator);
    pas_heap_lock_unlock();

    TEST_ASSERT(enumerated, "libpas enumerates this process's heap");
    TEST_ASSERT(wanted.found, "libpas's enumerator reports an object fastMalloc allocated");
}

} // anonymous namespace

void testTypeinfo()
{
    SuiteTracer tracer("Typeinfo");
    if (!tracer.shouldRun())
        return;

    // The object's layout comes from the debug info alone: its class, its base
    // and the offset of its string.
    analyzeInAndOutOfProcess([] {
        static JSC::StringFireDetail object(targetString);
        return Address { &object };
    }, analyze);

    checkImages();
    describeImages();
    enumerateLibpasHeap();
}

#else

void testTypeinfo()
{
    SuiteTracer tracer("Typeinfo");
    if (!tracer.shouldRun())
        return;

    skipSuite("Typeinfo", "mya_heap is off, or liblldb's headers were not found");
}

#endif // ENABLE(MYA_HEAP)

} // namespace JSCToolsTest
