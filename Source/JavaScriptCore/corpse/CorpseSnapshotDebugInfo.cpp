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
#include "CorpseIgnoredDeclarations.h"
#include "CorpseImage.h"
#include "CorpseLimits.h"
#include "CorpseSnapshot.h"
#include <algorithm>
#include <array>
#include <string_view>
#include <wtf/Hasher.h>
#include <wtf/TZoneMallocInlines.h>

namespace JSC {
namespace Corpse {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SnapshotDebugInfo);

static unsigned long long forReport(Address address)
{
    return address.toTargetVMAddress();
}

namespace {

using NeedsReader = TargetClass::NeedsReader;

// Class templates whose objects the debug info does not describe as they are laid out.
struct ClassTemplateNeedingReader {
    std::string_view name;
    NeedsReader needsReader;
};

constexpr std::array classTemplatesNeedingReaders {
    ClassTemplateNeedingReader { "WTF::Deque", NeedsReader::Container },
    ClassTemplateNeedingReader { "WTF::HashTable", NeedsReader::Container },
    ClassTemplateNeedingReader { "WTF::SentinelLinkedList", NeedsReader::Container },
    ClassTemplateNeedingReader { "WTF::Vector", NeedsReader::Container },
    ClassTemplateNeedingReader { "std::__1::__cxx_atomic_base_impl", NeedsReader::Atomic },
    // Swift's C++ interoperability holds Swift memory through these, from <swift/_SwiftCxxInteroperability.h>.
    ClassTemplateNeedingReader { "swift::_impl::RefCountedClass", NeedsReader::SwiftPointer },
    ClassTemplateNeedingReader { "swift::_impl::OpaqueStorage", NeedsReader::SwiftPointer },
};

} // anonymous namespace

static std::string_view nameView(lldb::SBType& type)
{
    const char* name = type.GetName();
    return name ? std::string_view { name } : std::string_view { };
}

static String nameOf(lldb::SBType type)
{
    return String::fromUTF8(type.GetName());
}

static String pathOf(lldb::SBModule module)
{
    std::array<char, maxImagePathLength> path { };
    module.GetFileSpec().GetPath(path.data(), path.size());
    return String::fromUTF8(path.data());
}

static lldb::SBType canonicalType(lldb::SBType type)
{
    return type.GetCanonicalType().GetUnqualifiedType();
}

static bool isRecord(lldb::SBType& type)
{
    return type.GetTypeClass() & (lldb::eTypeClassClass | lldb::eTypeClassStruct | lldb::eTypeClassUnion);
}

bool SnapshotDebugInfo::isDefinition(lldb::SBType& type)
{
    return type.GetByteSize() || type.GetNumberOfFields();
}

static bool isFlexibleArray(lldb::SBType type)
{
    return type.GetTypeClass() == lldb::eTypeClassArray && !type.GetByteSize();
}

static bool isIgnoredDeclaration(const String& name)
{
    for (std::string_view ignored : ignoredDeclarations) {
        if (sameName(name, String::fromUTF8(ignored)))
            return true;
    }
    return false;
}

static std::optional<NeedsReader> needsReaderByName(lldb::SBType& type)
{
    std::string_view templateName = nameView(type);
    templateName = templateName.substr(0, templateName.find('<'));
    for (const ClassTemplateNeedingReader& classTemplate : classTemplatesNeedingReaders) {
        if (classTemplate.name == templateName)
            return classTemplate.needsReader;
    }
    return std::nullopt;
}

// The class Swift's C++ interoperability generates for a Swift value of fixed layout holds the value's bytes in a
// `_storage` array, and gives them out through _getOpaquePointer().
static bool isSwiftInlineValue(lldb::SBType& type)
{
    if (type.GetNumberOfFields() != 1 || type.GetNumberOfDirectBaseClasses())
        return false;
    lldb::SBTypeMember field = type.GetFieldAtIndex(0);
    const char* fieldName = field.GetName();
    if (!fieldName || std::string_view { fieldName } != "_storage" || canonicalType(field.GetType()).GetTypeClass() != lldb::eTypeClassArray)
        return false;
    for (uint32_t i = 0; i < type.GetNumberOfMemberFunctions(); ++i) {
        const char* functionName = type.GetMemberFunctionAtIndex(i).GetName();
        if (functionName && std::string_view { functionName } == "_getOpaquePointer")
            return true;
    }
    return false;
}

static NeedsReader needsReaderOf(lldb::SBType& type)
{
    if (type.GetTypeClass() == lldb::eTypeClassUnion)
        return NeedsReader::Union;
    if (std::optional<NeedsReader> byName = needsReaderByName(type))
        return *byName;
    if (isSwiftInlineValue(type))
        return NeedsReader::SwiftValue;
    for (uint32_t i = 0; i < type.GetNumberOfFields(); ++i) {
        lldb::SBTypeMember field = type.GetFieldAtIndex(i);
        if (field.IsBitfield())
            return NeedsReader::Bitfield;
        if (isFlexibleArray(canonicalType(field.GetType())))
            return NeedsReader::FlexibleArray;
    }
    return NeedsReader::No;
}

// A direct virtual base is also among the virtual bases.
static bool isVirtualBase(lldb::SBType& type, lldb::SBTypeMember& base)
{
    lldb::SBType baseType = base.GetType();
    for (uint32_t i = 0; i < type.GetNumberOfVirtualBaseClasses(); ++i) {
        lldb::SBType virtualBase = type.GetVirtualBaseClassAtIndex(i).GetType();
        if (virtualBase == baseType)
            return true;
    }
    return false;
}

SnapshotDebugInfo::ClassKey::ClassKey(lldb::SBType definition)
    : type(definition)
{
    Hasher hasher;
    add(hasher, nameOf(type));
    add(hasher, type.GetByteSize());
    for (uint32_t i = 0; i < type.GetNumberOfDirectBaseClasses(); ++i)
        add(hasher, type.GetDirectBaseClassAtIndex(i).GetOffsetInBytes());
    for (uint32_t i = 0; i < type.GetNumberOfFields(); ++i) {
        lldb::SBTypeMember field = type.GetFieldAtIndex(i);
        add(hasher, String::fromUTF8(field.GetName()));
        add(hasher, field.GetOffsetInBits());
    }
    hash = hasher.hash();
}

SnapshotDebugInfo::SnapshotDebugInfo()
    : m_debugger(lldb::SBDebugger::Create(false))
    , m_target(m_debugger.CreateTarget(""))
{
    RELEASE_ASSERT(m_target.IsValid());
}

SnapshotDebugInfo::~SnapshotDebugInfo()
{
    m_pointees.clear();
    m_classes.clear();
    m_types.clear();
    m_debugger.DeleteTarget(m_target);
    lldb::SBDebugger::Destroy(m_debugger);
}

Ref<SnapshotDebugInfo> SnapshotDebugInfo::create(Snapshot& snapshot)
{
    int pid = snapshot.process()->pid();
    CORPSE_DIAGNOSTICS(diagnostics, "opening the debug info of pid %d", pid);
    lldb::SBDebugger::Initialize();
    Ref debugInfo = adoptRef(*new SnapshotDebugInfo);
    lldb::SBTarget& target = debugInfo->m_target;
    for (const Image& image : snapshot.images()) {
        lldb::SBModule module = target.AddModule(image.path().legacyCStringPointer(), nullptr, image.uuid().legacyCStringPointer());
        lldb::SBAddress header = module.GetObjectFileHeaderAddress();
        if (!module.IsValid() || !header.IsValid() || !target.SetModuleLoadAddress(module, image.loadAddress().toTargetVMAddress() - header.GetFileAddress()).Success())
            CORPSE_REPORT("Could not open the debug info of '%s'", image.path().legacyCStringPointer());
    }
    return debugInfo;
}

const TargetType& SnapshotDebugInfo::addType(std::unique_ptr<TargetType>&& type)
{
    m_types.append(WTF::move(type));
    return *m_types.last();
}

const TargetType& SnapshotDebugInfo::opaqueType(lldb::SBType type, TargetOpaque::Kind kind)
{
    return addType(makeUnique<TargetType>(WTF::InPlaceType<TargetOpaque>, nameOf(type), type.GetByteSize(), kind));
}

Vector<TargetTypeRef> SnapshotDebugInfo::parameterTypes(Address address)
{
    CORPSE_DIAGNOSTICS(diagnostics, "reading the parameters of the function at 0x%llx", forReport(address));
    lldb::SBFunction function = m_target.ResolveLoadAddress(address.toTargetVMAddress()).GetFunction();
    if (!function.IsValid()) {
        CORPSE_REPORT("No debug info describes a function at 0x%llx", forReport(address));
        return { };
    }
    lldb::SBModule module = function.GetStartAddress().GetModule();
    lldb::SBValueList parameters = function.GetBlock().GetVariables(m_target, true, false, false);
    Vector<TargetTypeRef> types;
    for (uint32_t i = 0; i < parameters.GetSize(); ++i) {
        const TargetType* type = definedType(parameters.GetValueAtIndex(i).GetType(), module);
        if (!type)
            return { };
        types.append(*type);
    }
    return types;
}

const TargetType* SnapshotDebugInfo::pointee(const TargetPointer& pointer)
{
    auto entry = m_pointees.find(&pointer);
    RELEASE_ASSERT(entry != m_pointees.end());
    if (entry->value.isDescribed)
        return entry->value.described;
    // Copies, since describing the pointee adds to m_pointees.
    lldb::SBType type = entry->value.type;
    lldb::SBModule module = entry->value.module;
    const TargetType* described = valueType(type, module);
    Pointee& pointee = m_pointees.find(&pointer)->value;
    pointee.isDescribed = true;
    pointee.described = described;
    return described;
}

const TargetType* SnapshotDebugInfo::valueType(lldb::SBType heldType, const lldb::SBModule& module)
{
    using Kind = TargetOpaque::Kind;
    lldb::SBType type = canonicalType(heldType);
    switch (type.GetTypeClass()) {
    case lldb::eTypeClassClass:
    case lldb::eTypeClassStruct:
    case lldb::eTypeClassUnion:
        return classType(type, module);
    case lldb::eTypeClassPointer:
    case lldb::eTypeClassReference: {
        lldb::SBType pointee = canonicalType(type.GetPointeeType());
        const TargetType& pointer = addType(makeUnique<TargetType>(WTF::InPlaceType<TargetPointer>, nameOf(pointee)));
        m_pointees.add(&std::get<TargetPointer>(pointer), Pointee { pointee, module });
        return &pointer;
    }
    case lldb::eTypeClassObjCObjectPointer:
    case lldb::eTypeClassBlockPointer: {
        // A block's pointee is its function type, so the block is named after the pointer.
        String name = type.GetTypeClass() == lldb::eTypeClassBlockPointer ? nameOf(type) : nameOf(canonicalType(type.GetPointeeType()));
        const TargetType& object = addType(makeUnique<TargetType>(WTF::InPlaceType<TargetClass>, WTF::move(name), pathOf(module), 0, false, NeedsReader::ObjectiveCObject, Vector<TargetMember> { }, Vector<TargetBase> { }));
        const TargetType& pointer = addType(makeUnique<TargetType>(WTF::InPlaceType<TargetPointer>, std::get<TargetClass>(object).name));
        m_pointees.add(&std::get<TargetPointer>(pointer), Pointee { { }, { }, true, &object });
        return &pointer;
    }
    case lldb::eTypeClassArray: {
        const TargetType* element = definedType(type.GetArrayElementType(), module);
        if (!element)
            return nullptr;
        size_t elementSize = byteSize(*element);
        if (!elementSize || !type.GetByteSize()) {
            CORPSE_REPORT("The array %s has no length", type.GetName());
            return nullptr;
        }
        return &addType(makeUnique<TargetType>(WTF::InPlaceType<TargetArray>, *element, type.GetByteSize() / elementSize));
    }
    case lldb::eTypeClassBuiltin:
    case lldb::eTypeClassComplexFloat:
    case lldb::eTypeClassComplexInteger:
        return &opaqueType(type, Kind::Scalar);
    case lldb::eTypeClassEnumeration:
        return &opaqueType(type, Kind::Enumeration);
    case lldb::eTypeClassMemberPointer:
        return &opaqueType(type, Kind::MemberPointer);
    case lldb::eTypeClassVector:
        return &opaqueType(type, Kind::Vector);
    case lldb::eTypeClassFunction:
        return &opaqueType(type, Kind::Function);
    default:
        CORPSE_REPORT("The type %s is of a kind (%u) this does not describe", type.GetName(), static_cast<unsigned>(type.GetTypeClass()));
        return nullptr;
    }
}

const TargetType* SnapshotDebugInfo::definedType(lldb::SBType type, const lldb::SBModule& module)
{
    const TargetType* result = valueType(type, module);
    if (auto* declaration = result ? std::get_if<TargetDeclaration>(result) : nullptr) {
        CORPSE_REPORT("'%s' only declares the class %s", declaration->image.utf8().legacyCStringPointer(), declaration->name.utf8().legacyCStringPointer());
        return nullptr;
    }
    return result;
}

const TargetType* SnapshotDebugInfo::classType(lldb::SBType type, const lldb::SBModule& module)
{
    ASSERT(isRecord(type));
    ClassKey key { type };
    if (auto existing = m_classes.find(key); existing != m_classes.end())
        return existing->value;
    const TargetType* result = nullptr;
    if (!isDefinition(type)) {
        String name = nameOf(type);
        if (isIgnoredDeclaration(name))
            result = &opaqueType(type, TargetOpaque::Kind::IgnoredDeclaration);
        else
            result = &addType(makeUnique<TargetType>(WTF::InPlaceType<TargetDeclaration>, WTF::move(name), pathOf(module)));
    } else {
        CORPSE_DIAGNOSTICS(diagnostics, "reading the class %s", type.GetName());
        if (std::unique_ptr<TargetType> created = createClass(type, module))
            result = &addType(WTF::move(created));
    }
    m_classes.add(WTF::move(key), result);
    return result;
}

std::unique_ptr<TargetType> SnapshotDebugInfo::createClass(lldb::SBType type, const lldb::SBModule& module)
{
    NeedsReader needsReader = needsReaderOf(type);
    Vector<TargetMember> members;
    Vector<TargetBase> virtualBases;
    if (needsReader == NeedsReader::No) {
        for (uint32_t i = 0; i < type.GetNumberOfDirectBaseClasses(); ++i) {
            lldb::SBTypeMember base = type.GetDirectBaseClassAtIndex(i);
            if (isVirtualBase(type, base))
                continue;
            const TargetType* baseType = definedType(base.GetType(), module);
            if (!baseType)
                return nullptr;
            members.append({ TargetMember::Kind::Base, base.GetOffsetInBytes(), *baseType, { } });
        }
        for (uint32_t i = 0; i < type.GetNumberOfFields(); ++i) {
            lldb::SBTypeMember field = type.GetFieldAtIndex(i);
            const TargetType* fieldType = definedType(field.GetType(), module);
            if (!fieldType)
                return nullptr;
            members.append({ TargetMember::Kind::Field, field.GetOffsetInBytes(), *fieldType, String::fromUTF8(field.GetName()) });
        }
        for (uint32_t i = 0; i < type.GetNumberOfVirtualBaseClasses(); ++i) {
            lldb::SBTypeMember base = type.GetVirtualBaseClassAtIndex(i);
            const TargetType* baseType = definedType(base.GetType(), module);
            if (!baseType)
                return nullptr;
            virtualBases.append({ base.GetOffsetInBytes(), *baseType });
        }
    }
    bool hasVTable = type.IsPolymorphicClass() || type.GetNumberOfVirtualBaseClasses();
    return makeUnique<TargetType>(WTF::InPlaceType<TargetClass>, nameOf(type), pathOf(module), type.GetByteSize(), hasVTable, needsReader, WTF::move(members), WTF::move(virtualBases));
}

// "className::name(...)", where `name` has no scope of its own.
static bool isDefinitelyMemberOf(std::string_view member, std::string_view className)
{
    if (!member.starts_with(className) || !member.substr(className.size()).starts_with("::"))
        return false;
    std::string_view name = member.substr(className.size() + 2);
    name = name.substr(0, name.find('('));
    return !name.empty() && name.find("::") == std::string_view::npos;
}

// https://itanium-cxx-abi.github.io/cxx-abi/abi.html#vtable
struct VTablePrefix {
    int64_t offsetToTop;
    uint64_t typeInfo;
};

static constexpr std::string_view vtableNamePrefix = "vtable for ";

TargetCanonicalPointer SnapshotDebugInfo::canonicalPointer(Snapshot& snapshot, Address address, const TargetType& staticType)
{
    TargetCanonicalPointer staticPointer { { address, staticType }, { address, staticType } };
    const TargetClass& staticClass = std::get<TargetClass>(staticType);
    if (!staticClass.hasVTable)
        return staticPointer;
    CString staticName = staticClass.name.utf8();

    CORPSE_DIAGNOSTICS(diagnostics, "reading the dynamic type of the %s at 0x%llx", staticName.legacyCStringPointer(), forReport(address));
    Memory& memory = snapshot.memory();
    auto vtablePointer = memory.ptr<uint64_t>(address);
    Address addressPoint = vtablePointer ? Address { *vtablePointer }.stripped() : Address { };
    lldb::SBSymbol vtable = m_target.ResolveLoadAddress(addressPoint.toTargetVMAddress()).GetSymbol();
    const char* vtableName = vtable.GetName();
    if (!vtableName || !std::string_view { vtableName }.starts_with(vtableNamePrefix)) {
        CORPSE_REPORT("The %s at 0x%llx has no vtable pointer", staticName.legacyCStringPointer(), forReport(address));
        return staticPointer;
    }
    std::string_view className = std::string_view { vtableName }.substr(vtableNamePrefix.size());

    Address start { vtable.GetStartAddress().GetLoadAddress(m_target) };
    uint64_t size = vtable.GetEndAddress().GetLoadAddress(m_target) - start.toTargetVMAddress();
    auto prefix = memory.ptr<VTablePrefix>(addressPoint - sizeof(VTablePrefix));
    auto slots = memory.span<uint64_t>(start, size / sizeof(uint64_t));
    if (size > maxVTableSize || !prefix || !slots) {
        CORPSE_REPORT("Could not read the %s at 0x%llx", vtableName, forReport(start));
        return staticPointer;
    }
    Address node = address + static_cast<uint64_t>(prefix->offsetToTop);
    for (uint64_t slot : slots) {
        lldb::SBFunction function = m_target.ResolveLoadAddress(Address { slot }.stripped().toTargetVMAddress()).GetFunction();
        const char* name = function.GetName();
        if (!name || !isDefinitelyMemberOf(name, className))
            continue;
        lldb::SBType thisType = canonicalType(function.GetBlock().GetVariables(m_target, true, false, false).GetValueAtIndex(0).GetType().GetPointeeType());
        const TargetType* dynamicType = definedType(thisType, function.GetStartAddress().GetModule());
        if (!dynamicType)
            return staticPointer;
        const TargetType* base = baseAt(*dynamicType, address - node, staticClass);
        if (!base) {
            CORPSE_REPORT("The %s at 0x%llx is not a %s", JSC::Corpse::name(*dynamicType).utf8().legacyCStringPointer(), forReport(node), staticName.legacyCStringPointer());
            return staticPointer;
        }
        return { { node, *dynamicType }, { address, *base } };
    }
    CORPSE_REPORT("No member function of the class in the %s at 0x%llx is in the debug info", vtableName, forReport(start));
    return staticPointer;
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA_HEAP)
