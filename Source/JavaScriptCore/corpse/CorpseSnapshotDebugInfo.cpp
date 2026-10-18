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
#include "CorpseSnapshotDebugInfo.h"

#if ENABLE(MYA_HEAP)

#include "CorpseError.h"
#include "CorpseImage.h"
#include "CorpseLimits.h"
#include "CorpseSnapshot.h"
#include "CorpseTargetType.h"
#include <lldb/API/LLDB.h>
#include <string_view>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {
namespace Corpse {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SnapshotDebugInfo);

static unsigned long long forReport(Address address)
{
    return address.toTargetVMAddress();
}

SnapshotDebugInfo::SnapshotDebugInfo()
    : m_debugger(makeUniqueWithoutFastMallocCheck<lldb::SBDebugger>(lldb::SBDebugger::Create(false)))
    , m_target(makeUniqueWithoutFastMallocCheck<lldb::SBTarget>(m_debugger->CreateTarget("")))
{
    RELEASE_ASSERT(m_target->IsValid());
}

SnapshotDebugInfo::~SnapshotDebugInfo()
{
    m_types.clear();
    m_debugger->DeleteTarget(*m_target);
    lldb::SBDebugger::Destroy(*m_debugger);
}

Ref<SnapshotDebugInfo> SnapshotDebugInfo::create(Snapshot& snapshot)
{
    int pid = static_cast<int>(snapshot.process()->pid());
    CORPSE_DIAGNOSTICS(diagnostics, "opening the debug info of pid %d", pid);
    lldb::SBDebugger::Initialize();
    Ref debugInfo = adoptRef(*new SnapshotDebugInfo);
    lldb::SBTarget& target = *debugInfo->m_target;
    for (const Image& image : snapshot.images()) {
        lldb::SBModule module = target.AddModule(image.path().legacyCStringPointer(), nullptr, image.uuid().legacyCStringPointer());
        lldb::SBAddress header = module.GetObjectFileHeaderAddress();
        if (!header.IsValid() || !target.SetModuleLoadAddress(module, image.loadAddress().toTargetVMAddress() - header.GetFileAddress()).Success())
            CORPSE_REPORT("liblldb could not open the image '%s' of pid %d", image.path().legacyCStringPointer(), pid);
    }
    RELEASE_ASSERT(!target.GetProcess().IsValid());
    return debugInfo;
}

const TargetType& SnapshotDebugInfo::type(const lldb::SBType& type)
{
    lldb::SBType canonical = lldb::SBType(type).GetCanonicalType();
    const char* name = canonical.GetName();
    auto& types = m_types.ensure(String::fromUTF8(name ? name : ""), [] {
        return Vector<std::unique_ptr<TargetType>> { };
    }).iterator->value;
    for (auto& existing : types) {
        if (*existing->m_type == canonical)
            return *existing;
    }
    types.append(std::unique_ptr<TargetType>(new TargetType(canonical)));
    return *types.last();
}

// "className::name(...)", where `name` has no scope of its own.
static bool isMemberOf(std::string_view member, std::string_view className)
{
    if (!member.starts_with(className) || !member.substr(className.size()).starts_with("::"))
        return false;
    std::string_view name = member.substr(className.size() + 2);
    name = name.substr(0, name.find('('));
    return !name.empty() && name.find("::") == std::string_view::npos;
}

// Itanium C++ ABI, 2.5.2.
struct VTablePrefix {
    int64_t offsetToTop;
    uint64_t typeInfo;
};

const TargetType* SnapshotDebugInfo::dynamicTypeAt(Snapshot& snapshot, Address address, Address& completeObject)
{
    CORPSE_DIAGNOSTICS(diagnostics, "reading the dynamic type of the object at 0x%llx", forReport(address));
    Memory& memory = snapshot.memory();
    auto vtablePointer = memory.ptr<uint64_t>(address);
    Address addressPoint = vtablePointer ? Address { *vtablePointer }.stripped() : Address { };
    lldb::SBSymbol vtable = m_target->ResolveLoadAddress(addressPoint.toTargetVMAddress()).GetSymbol();
    const char* vtableName = vtable.GetName();
    constexpr std::string_view vtableNamePrefix = "vtable for ";
    if (!vtableName || !std::string_view { vtableName }.starts_with(vtableNamePrefix)) {
        CORPSE_REPORT("The object at 0x%llx has no vtable pointer", forReport(address));
        return nullptr;
    }
    std::string_view className = std::string_view { vtableName }.substr(vtableNamePrefix.size());

    Address start { vtable.GetStartAddress().GetLoadAddress(*m_target) };
    uint64_t size = vtable.GetEndAddress().GetLoadAddress(*m_target) - start.toTargetVMAddress();
    auto prefix = memory.ptr<VTablePrefix>(addressPoint - sizeof(VTablePrefix));
    auto slots = memory.span<uint64_t>(start, size / sizeof(uint64_t));
    if (size > maxVTableSize || !prefix || !slots) {
        CORPSE_REPORT("Could not read the %s at 0x%llx", vtableName, forReport(start));
        return nullptr;
    }
    for (uint64_t slot : slots) {
        lldb::SBFunction function = m_target->ResolveLoadAddress(Address { slot }.stripped().toTargetVMAddress()).GetFunction();
        const char* name = function.GetName();
        if (!name || !isMemberOf(name, className))
            continue;
        lldb::SBType type = function.GetBlock().GetVariables(*m_target, true, false, false).GetValueAtIndex(0).GetType().GetPointeeType().GetUnqualifiedType();
        if (type.IsTypeComplete()) {
            completeObject = address + static_cast<uint64_t>(prefix->offsetToTop);
            return &this->type(type);
        }
    }
    CORPSE_REPORT("No member function of the class in the %s at 0x%llx has a complete 'this' in its debug info", vtableName, forReport(start));
    return nullptr;
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA_HEAP)
