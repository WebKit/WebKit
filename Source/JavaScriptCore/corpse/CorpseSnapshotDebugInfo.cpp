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

#if ENABLE(MYA) && HAVE(LLDB)

#include "CorpseError.h"
#include "CorpseImage.h"
#include "CorpseLimits.h"
#include "CorpseSnapshot.h"
#include "CorpseTargetType.h"
#include <algorithm>
#include <array>
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

const TargetType& SnapshotDebugInfo::type(const lldb::SBType& type, const lldb::SBModule& module)
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
    types.append(std::unique_ptr<TargetType>(new TargetType(canonical, module)));
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

static constexpr std::string_view vtableNamePrefix = "vtable for ";

// The vtable that the first word of the object at `address` points into, and where.
static lldb::SBSymbol vtableAt(lldb::SBTarget& target, Memory& memory, Address address, Address& addressPoint)
{
    auto vtablePointer = memory.ptr<uint64_t>(address);
    addressPoint = vtablePointer ? Address { *vtablePointer }.stripped() : Address { };
    lldb::SBSymbol vtable = target.ResolveLoadAddress(addressPoint.toTargetVMAddress()).GetSymbol();
    const char* vtableName = vtable.GetName();
    return vtableName && std::string_view { vtableName }.starts_with(vtableNamePrefix) ? vtable : lldb::SBSymbol { };
}

bool SnapshotDebugInfo::startsWithVTablePointer(Snapshot& snapshot, Address address)
{
    Address addressPoint;
    return vtableAt(*m_target, snapshot.memory(), address, addressPoint).IsValid();
}

const TargetType* SnapshotDebugInfo::dynamicTypeAt(Snapshot& snapshot, Address address, Address& completeObject)
{
    CORPSE_DIAGNOSTICS(diagnostics, "reading the dynamic type of the object at 0x%llx", forReport(address));
    Memory& memory = snapshot.memory();
    Address addressPoint;
    lldb::SBSymbol vtable = vtableAt(*m_target, memory, address, addressPoint);
    const char* vtableName = vtable.GetName();
    if (!vtable.IsValid()) {
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
        // Not IsTypeComplete(): liblldb 21 can call a class it has laid out in full incomplete, depending on which compile
        // units it read first, such as JSC::IsoSubspace after JSC::GigacageAlignedMemoryAllocator's destructor.
        // A class only declared in its compile unit is not polymorphic.
        if (type.IsPolymorphicClass()) {
            completeObject = address + static_cast<uint64_t>(prefix->offsetToTop);
            return &this->type(type, function.GetStartAddress().GetModule());
        }
    }
    CORPSE_REPORT("No member function of the class in the %s at 0x%llx has a polymorphic 'this' in its debug info", vtableName, forReport(start));
    return nullptr;
}

static bool isRecord(lldb::SBType& type)
{
    return type.GetTypeClass() & (lldb::eTypeClassClass | lldb::eTypeClassStruct | lldb::eTypeClassUnion);
}

// Types whose fields do not say which of their pointers are valid, or how many objects
// they point to. Each of these will get a reader of its own; until then, nothing inside one is followed.
static constexpr std::array<std::string_view, 4> typesWithReaders {
    "WTF::Deque",
    "WTF::HashTable",
    "WTF::SentinelLinkedList",
    "WTF::Vector",
};

static bool hasReader(lldb::SBType& type)
{
    // Which member of a union is live is not in the union, and where a virtual base is depends on the complete object.
    if (type.GetTypeClass() == lldb::eTypeClassUnion || type.GetNumberOfVirtualBaseClasses())
        return true;
    const char* name = type.GetName();
    std::string_view templateName = name ? std::string_view { name } : std::string_view { };
    templateName = templateName.substr(0, templateName.find('<'));
    return std::ranges::find(typesWithReaders, templateName) != typesWithReaders.end();
}

// A type found in another type's description is described by the same image.
void SnapshotDebugInfo::appendPointers(lldb::SBType type, uint64_t offset, const lldb::SBModule& module, Vector<TargetPointer>& pointers)
{
    type = type.GetCanonicalType();
    if (type.IsPointerType() || type.IsReferenceType()) {
        lldb::SBType pointee = type.GetPointeeType().GetCanonicalType().GetUnqualifiedType();
        if (isRecord(pointee))
            pointers.append({ offset, &this->type(pointee, module) });
        return;
    }
    if (type.IsArrayType()) {
        lldb::SBType elementType = type.GetArrayElementType();
        uint64_t elementSize = elementType.GetByteSize();
        Vector<TargetPointer> element;
        appendPointers(elementType, 0, module, element);
        for (uint64_t elementOffset = 0; !element.isEmpty() && elementOffset < type.GetByteSize(); elementOffset += elementSize) {
            for (const TargetPointer& pointer : element)
                pointers.append({ offset + elementOffset + pointer.offset, pointer.pointee });
        }
        return;
    }
    if (!isRecord(type) || hasReader(type))
        return;
    for (uint32_t i = 0; i < type.GetNumberOfDirectBaseClasses(); ++i) {
        lldb::SBTypeMember base = type.GetDirectBaseClassAtIndex(i);
        appendPointers(base.GetType(), offset + base.GetOffsetInBytes(), module, pointers);
    }
    for (uint32_t i = 0; i < type.GetNumberOfFields(); ++i) {
        lldb::SBTypeMember field = type.GetFieldAtIndex(i);
        if (!field.IsBitfield())
            appendPointers(field.GetType(), offset + field.GetOffsetInBytes(), module, pointers);
    }
}

const Vector<TargetPointer>& SnapshotDebugInfo::pointers(const TargetType& type)
{
    if (!type.m_pointers) {
        Vector<TargetPointer> pointers;
        appendPointers(*type.m_type, 0, *type.m_module, pointers);
        type.m_pointers = WTF::move(pointers);
    }
    return *type.m_pointers;
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA) && HAVE(LLDB)
