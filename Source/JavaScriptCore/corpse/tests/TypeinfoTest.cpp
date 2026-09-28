/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE(LLDB)

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/Watchpoint.h>
#include <cxxabi.h>
#include <lldb/API/LLDB.h>
#include <optional>
#include <stdlib.h>
#include <string_view>
#include <typeinfo>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringCommon.h>

#endif // HAVE(LLDB)

namespace JSCToolsTest {

#if HAVE(LLDB)

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
            return UTF8CString(byteCast<char8_t>(characters.span()));
        characters.append(*character);
    }
    return { };
}

// The mangled name in the type_info of the object at `object`, read out of the
// corpse by following the object's vtable pointer.
UTF8CString dynamicTypeName(Snapshot& snapshot, Address object)
{
    Memory& memory = snapshot.memory();
    auto vtable = memory.ptr<uint64_t>(object);
    TEST_ASSERT(vtable, "the target object's vtable pointer is readable");
    if (!vtable)
        return { };

    // The type_info pointer sits in the word before the vtable's first entry.
    auto typeInfo = memory.ptr<uint64_t>(Address { *vtable }.stripped() - sizeof(uint64_t));
    TEST_ASSERT(typeInfo, "the target object's type_info pointer is readable");
    if (!typeInfo)
        return { };

    // A type_info is its own vtable pointer followed by its name pointer.
    auto namePointer = memory.ptr<uint64_t>(Address { *typeInfo }.stripped() + sizeof(uint64_t));
    TEST_ASSERT(namePointer, "the target object's type_info name pointer is readable");
    if (!namePointer)
        return { };

    // libc++ sets the top bit of the name pointer when the name is not unique across images.
    constexpr uint64_t nonUniqueBit = 1ull << 63;
    CString name = readCString(memory, Address { *namePointer & ~nonUniqueBit }.stripped());
    TEST_ASSERT(!name.isNull(), "the target object's type_info name is readable");
    return name;
}

UTF8CString demangledTypeName(const UTF8CString& mangledName)
{
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangledName.legacyCStringPointer(), nullptr, nullptr, &status);
    auto freeDemangled = makeScopeExit([&] {
        free(demangled);
    });
    TEST_ASSERT(!status && demangled, "the target's type_info name demangles");
    if (status || !demangled)
        return { };
    return UTF8CString(byteCast<char8_t>(unsafeSpan(demangled)));
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
    TEST_ASSERT_EQ(completeCount, 1u, "the target's debug info has exactly one definition of the target object's class");
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

void analyze(Snapshot& snapshot, Address object)
{
    CString mangledName = dynamicTypeName(snapshot, object);
    if (mangledName.isNull())
        return;
    TEST_ASSERT(std::string_view { mangledName.legacyCStringPointer() } == typeid(JSC::StringFireDetail).name(),
        "the target's RTTI names the class the object really is, not the one it is held as");
    CString className = demangledTypeName(mangledName);
    if (className.isNull())
        return;

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

    lldb::SBType type = completeType(target, className);
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

} // anonymous namespace

void testTypeinfo()
{
    SuiteTracer tracer("Typeinfo");
    if (!tracer.shouldRun())
        return;

    // The object is held as a JSC::FireDetail, so only the target's RTTI says
    // which class it really is.
    analyzeInAndOutOfProcess([] {
        static JSC::StringFireDetail object(targetString);
        return Address { &object };
    }, analyze);
}

#else // No SB API, so there is nothing to ask.

void testTypeinfo()
{
    SuiteTracer tracer("Typeinfo");
    if (!tracer.shouldRun())
        return;

#if ENABLE(MYA_HEAP)
    TEST_ASSERT(false, "mya_heap is enabled but liblldb's headers were not found");
#else
    skipSuite("Typeinfo", "mya_heap is not enabled");
#endif
}

#endif // HAVE(LLDB)

} // namespace JSCToolsTest
