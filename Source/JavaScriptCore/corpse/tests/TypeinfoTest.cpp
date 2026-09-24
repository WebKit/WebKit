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

#if ENABLE(MYA_HEAP)

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/SourceProvider.h>
#include <array>
#include <cxxabi.h>
#include <errno.h>
#include <lldb/API/LLDB.h>
#include <optional>
#include <signal.h>
#include <spawn.h>
#include <stdlib.h>
#include <string_view>
#include <sys/wait.h>
#include <typeinfo>
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringCommon.h>

extern char** environ;

#if OS(DARWIN) && !defined(BUILDING_WITH_CMAKE)
// We should only link this if it is actually found; Xcode doesn't have optional dependencies.
__asm__(".linker_option \"-llldb\"");
#endif

#endif // ENABLE(MYA_HEAP)

namespace JSCToolsTest {

#if ENABLE(MYA_HEAP)

namespace {

using JSC::Corpse::Address;
using JSC::Corpse::Process;
using JSC::Corpse::Snapshot;

TextPosition targetStartPosition()
{
    return { OrdinalNumber::fromZeroBasedInt(1234), OrdinalNumber::fromZeroBasedInt(5678) };
}

// The target holds a JSC::StringSourceProvider through its base class, so only
// the target's RTTI says which class the object really is.
Ref<JSC::SourceProvider> createTargetObject()
{
    return JSC::StringSourceProvider::create("1 + 1"_s, JSC::SourceOrigin { }, "typeinfo-target.js"_s,
        JSC::SourceTaintedOrigin::Untainted, targetStartPosition());
}

CString readCString(const Snapshot& snapshot, Address address)
{
    constexpr size_t maxLength = 256;
    Vector<char> characters;
    for (size_t offset = 0; offset < maxLength; ++offset) {
        auto character = snapshot.read<char>(address + offset);
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
CString dynamicTypeName(const Snapshot& snapshot, Address object)
{
    auto vtable = snapshot.read<uint64_t>(object);
    TEST_ASSERT(vtable, "the target object's vtable pointer is readable");
    if (!vtable)
        return { };

    // The type_info pointer sits in the word before the vtable's first entry.
    auto typeInfo = snapshot.read<uint64_t>(Address { *vtable }.stripped() - sizeof(uint64_t));
    TEST_ASSERT(typeInfo, "the target object's type_info pointer is readable");
    if (!typeInfo)
        return { };

    // A type_info is its own vtable pointer followed by its name pointer.
    auto namePointer = snapshot.read<uint64_t>(Address { *typeInfo }.stripped() + sizeof(uint64_t));
    TEST_ASSERT(namePointer, "the target object's type_info name pointer is readable");
    if (!namePointer)
        return { };

    // libc++ sets the top bit of the name pointer when the name is not unique across images.
    constexpr uint64_t nonUniqueBit = 1ull << 63;
    CString name = readCString(snapshot, Address { *namePointer & ~nonUniqueBit }.stripped());
    TEST_ASSERT(!name.isNull(), "the target object's type_info name is readable");
    return name;
}

CString demangledTypeName(const CString& mangledName)
{
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangledName.data(), nullptr, nullptr, &status);
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
lldb::SBType completeType(lldb::SBTarget target, const CString& name)
{
    lldb::SBTypeList types = target.FindTypes(name.data());
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
    TEST_ASSERT(std::string_view { mangledName.data() } == typeid(JSC::StringSourceProvider).name(),
        "the target's RTTI names the class the object really is, not the one it is held as");
    CString className = demangledTypeName(mangledName);
    if (className.isNull())
        return;

    CString executablePath = snapshot.process()->executablePath();
    TEST_ASSERT(!executablePath.isNull(), "the target's executable path is readable");
    if (executablePath.isNull())
        return;

    lldb::SBDebugger::Initialize();
    lldb::SBDebugger debugger = lldb::SBDebugger::Create();
    auto destroyDebugger = makeScopeExit([&] {
        lldb::SBDebugger::Destroy(debugger);
    });
    TEST_ASSERT(debugger.IsValid(), "liblldb creates a debugger");
    if (!debugger.IsValid())
        return;

    lldb::SBError error;
    lldb::SBTarget target = debugger.CreateTarget(executablePath.data(), nullptr, nullptr, false, error);
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
    TEST_ASSERT_EQ(static_cast<size_t>(type.GetByteSize()), sizeof(JSC::StringSourceProvider),
        "the debug info gives the target object's class its size");

    lldb::SBType base = baseAtStart(type);
    if (!base.IsValid())
        return;

    lldb::SBTypeMember startPosition = memberNamed(base, "m_startPosition");
    if (!startPosition.IsValid())
        return;
    TEST_ASSERT_EQ(static_cast<size_t>(startPosition.GetType().GetByteSize()), sizeof(TextPosition),
        "the debug info gives m_startPosition its size");
    auto value = snapshot.read<TextPosition>(object + startPosition.GetOffsetInBytes());
    TEST_ASSERT(value, "the target object's m_startPosition is readable at the offset the debug info gives");
    if (!value)
        return;
    TEST_ASSERT(*value == targetStartPosition(),
        "the corpse holds the target's m_startPosition at the offset the debug info gives");
}

void attachAndAnalyze(pid_t pid, Address object)
{
    RefPtr<Process> process = Process::create(pid);
    bool attached = process->attach();
    TEST_ASSERT(attached, "attaching to the target process succeeds");
    if (!attached)
        return;
    Snapshot snapshot(process);
    TEST_ASSERT(snapshot.isValid(), "a snapshot of the target process is valid");
    if (!snapshot.isValid())
        return;

    analyze(snapshot, object);
}

void testInThisProcess()
{
    Ref<JSC::SourceProvider> object = createTargetObject();
    attachAndAnalyze(getpid(), Address { object.ptr() });
}

// The analysis and the target are separate processes: this process launches a
// copy of itself as the target and takes a corpse of it.
void testInSeparateProcess()
{
    // The target writes the address of its object to its stdout.
    std::array<int, 2> addressPipe { -1, -1 };
    TEST_ASSERT(!pipe(addressPipe.data()), "a pipe from the target opens");
    auto closePipe = makeScopeExit([&] {
        for (int fd : addressPipe) {
            if (fd >= 0)
                close(fd);
        }
    });
    if (addressPipe[0] < 0)
        return;

    CString executablePath = Process::create(getpid())->executablePath();
    TEST_ASSERT(!executablePath.isNull(), "this process's executable path is readable");
    if (executablePath.isNull())
        return;

    char* const arguments[] = {
        const_cast<char*>(executablePath.data()),
        const_cast<char*>("--typeinfo-target"),
        nullptr
    };
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, addressPipe[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, addressPipe[0]);
    posix_spawn_file_actions_addclose(&actions, addressPipe[1]);
    pid_t child = 0;
    int error = posix_spawn(&child, executablePath.data(), &actions, nullptr, arguments, environ);
    posix_spawn_file_actions_destroy(&actions);
    TEST_ASSERT(!error, "the target process launches");
    if (error) {
        dataLogLn("    posix_spawn: ", safeStrerror(error));
        return;
    }
    auto killChild = makeScopeExit([&] {
        kill(child, SIGKILL);
        while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) { }
    });

    uint64_t object = 0;
    bool reported = read(addressPipe[0], &object, sizeof(object)) == sizeof(object);
    TEST_ASSERT(reported, "the target reports the address of its object");
    if (!reported)
        return;

    attachAndAnalyze(child, Address { object });
}

} // anonymous namespace

void testTypeinfo()
{
    SuiteTracer tracer("Typeinfo");
    if (!tracer.shouldRun())
        return;

    testInThisProcess();
    if (!linuxSkip("Typeinfo in a separate process", "attaching to another process is not implemented on Linux yet"))
        testInSeparateProcess();
}

int runTypeinfoTarget()
{
    Ref<JSC::SourceProvider> object = createTargetObject();
    auto address = reinterpret_cast<uint64_t>(object.ptr());
    if (write(STDOUT_FILENO, &address, sizeof(address)) != sizeof(address))
        return 1;

    // The analysis kills this process when it is done with the object.
    while (true)
        pause();
}

#else // No SB API, so there is nothing to ask.

void testTypeinfo()
{
    SuiteTracer tracer("Typeinfo");
    if (!tracer.shouldRun())
        return;

#if ASSERT_ENABLED
    TEST_ASSERT(false, "we expected to test mya_heap in this configuration");
#else
    skipSuite("Typeinfo", "mya_heap is not enabled");
#endif
}

int runTypeinfoTarget()
{
    return 1;
}

#endif // ENABLE(MYA_HEAP)

} // namespace JSCToolsTest
