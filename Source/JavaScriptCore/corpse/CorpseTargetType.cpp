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
#include "CorpseTargetType.h"

#if ENABLE(MYA_HEAP)

#include <wtf/PrintStream.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace JSC {
namespace Corpse {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(TargetType);

const TargetClass& TargetMember::classType() const
{
    return std::get<TargetClass>(type);
}

const TargetClass& TargetBase::classType() const
{
    return std::get<TargetClass>(type);
}

static void appendBases(Vector<TargetBase>& bases, uint64_t offset, const TargetClass& targetClass)
{
    for (const TargetMember& member : targetClass.members) {
        if (member.kind != TargetMember::Kind::Base)
            continue;
        bases.append({ offset + member.offset, member.type });
        appendBases(bases, offset + member.offset, member.classType());
    }
}

// A complete object holds every virtual base of its class once, so a base subobject's own virtual bases are
// among the complete object's.
static Vector<TargetBase> basesOf(const Vector<TargetMember>& members, const Vector<TargetBase>& virtualBases)
{
    Vector<TargetBase> bases;
    for (const TargetMember& member : members) {
        if (member.kind != TargetMember::Kind::Base)
            continue;
        bases.append({ member.offset, member.type });
        appendBases(bases, member.offset, member.classType());
    }
    for (const TargetBase& base : virtualBases) {
        bases.append(base);
        appendBases(bases, base.offset, base.classType());
    }
    return bases;
}

TargetClass::TargetClass(String&& name, String&& image, size_t byteSize, bool hasVTable, NeedsReader needsReader, Vector<TargetMember>&& members, Vector<TargetBase>&& virtualBases)
    : name(WTF::move(name))
    , image(WTF::move(image))
    , byteSize(byteSize)
    , hasVTable(hasVTable)
    , needsReader(needsReader)
    , members(WTF::move(members))
    , virtualBases(WTF::move(virtualBases))
    , bases(basesOf(this->members, this->virtualBases))
{
}

String withoutTemplateArguments(StringView name)
{
    StringBuilder builder;
    unsigned depth = 0;
    for (char16_t character : name.codeUnits()) {
        if (character == '<')
            ++depth;
        else if (character == '>' && depth)
            --depth;
        else if (!depth)
            builder.append(character);
    }
    return builder.toString();
}

bool sameName(StringView a, StringView b)
{
    return withoutTemplateArguments(a) == withoutTemplateArguments(b);
}

static bool sameMembers(const TargetClass&, const TargetClass&);

bool sameClass(const TargetClass& a, const TargetClass& b)
{
    return sameName(a.name, b.name) && sameMembers(a, b);
}

const TargetType* baseAt(const TargetType& object, uint64_t offset, const TargetClass& type)
{
    const TargetClass& objectClass = std::get<TargetClass>(object);
    if (!offset && sameClass(objectClass, type))
        return &object;
    for (const TargetBase& base : objectClass.bases) {
        if (base.offset == offset && sameClass(base.classType(), type))
            return &base.type;
    }
    return nullptr;
}

const TargetType* definitionOf(const TargetType& object, const TargetDeclaration& declaration)
{
    const TargetClass& objectClass = std::get<TargetClass>(object);
    if (sameName(objectClass.name, declaration.name))
        return &object;
    for (const TargetBase& base : objectClass.bases) {
        if (!base.offset && sameName(base.classType().name, declaration.name))
            return &base.type;
    }
    return nullptr;
}

// The names of nested types are not compared, since they may be template instances.
static bool sameStructure(const TargetType& a, const TargetType& b)
{
    if (&a == &b)
        return true;
    return WTF::switchOn(a,
        [&](const TargetClass& aClass) {
            auto* bClass = std::get_if<TargetClass>(&b);
            return bClass && sameMembers(aClass, *bClass);
        },
        [&](const TargetDeclaration& aDeclaration) {
            auto* bDeclaration = std::get_if<TargetDeclaration>(&b);
            return bDeclaration && sameName(aDeclaration.name, bDeclaration->name);
        },
        [&](const TargetPointer&) {
            return std::holds_alternative<TargetPointer>(b);
        },
        [&](const TargetOpaque& aOpaque) {
            auto* bOpaque = std::get_if<TargetOpaque>(&b);
            return bOpaque && aOpaque.kind == bOpaque->kind && aOpaque.byteSize == bOpaque->byteSize;
        },
        [&](const TargetArray& aArray) {
            auto* bArray = std::get_if<TargetArray>(&b);
            return bArray && aArray.count == bArray->count && sameStructure(aArray.element, bArray->element);
        });
}

bool sameType(const TargetType& a, const TargetType& b)
{
    auto* aClass = std::get_if<TargetClass>(&a);
    auto* bClass = std::get_if<TargetClass>(&b);
    if (aClass || bClass)
        return aClass && bClass && sameClass(*aClass, *bClass);
    return sameStructure(a, b);
}

static bool sameMembers(const TargetClass& a, const TargetClass& b)
{
    if (&a == &b)
        return true;
    if (a.byteSize != b.byteSize || a.hasVTable != b.hasVTable || a.needsReader != b.needsReader || a.members.size() != b.members.size() || a.virtualBases.size() != b.virtualBases.size())
        return false;
    for (size_t i = 0; i < a.members.size(); ++i) {
        const TargetMember& aMember = a.members[i];
        const TargetMember& bMember = b.members[i];
        if (aMember.kind != bMember.kind || aMember.offset != bMember.offset || aMember.name != bMember.name || !sameStructure(aMember.type, bMember.type))
            return false;
    }
    for (size_t i = 0; i < a.virtualBases.size(); ++i) {
        const TargetBase& aBase = a.virtualBases[i];
        const TargetBase& bBase = b.virtualBases[i];
        if (aBase.offset != bBase.offset || !sameStructure(aBase.type, bBase.type))
            return false;
    }
    return true;
}

String name(const TargetType& type)
{
    return WTF::switchOn(type,
        [](const TargetClass& targetClass) {
            return targetClass.name;
        },
        [](const TargetDeclaration& declaration) {
            return declaration.name;
        },
        [](const TargetPointer& pointer) {
            return makeString(pointer.pointee, " *"_s);
        },
        [](const TargetOpaque& opaque) {
            return opaque.name;
        },
        [](const TargetArray& array) {
            return makeString(name(array.element), '[', array.count, ']');
        });
}

size_t byteSize(const TargetType& type)
{
    return WTF::switchOn(type,
        [](const TargetClass& targetClass) -> size_t {
            return targetClass.byteSize;
        },
        [](const TargetDeclaration&) -> size_t {
            return 0;
        },
        [](const TargetPointer&) -> size_t {
            return sizeof(uint64_t);
        },
        [](const TargetOpaque& opaque) -> size_t {
            return opaque.byteSize;
        },
        [](const TargetArray& array) -> size_t {
            return byteSize(array.element) * array.count;
        });
}

} // namespace Corpse
} // namespace JSC

namespace WTF {

void printInternal(PrintStream& out, JSC::Corpse::TargetClass::NeedsReader needsReader)
{
    using NeedsReader = JSC::Corpse::TargetClass::NeedsReader;
    switch (needsReader) {
    case NeedsReader::No:
        out.print("No");
        return;
    case NeedsReader::Bitfield:
        out.print("Bitfield");
        return;
    case NeedsReader::Union:
        out.print("Union");
        return;
    case NeedsReader::FlexibleArray:
        out.print("FlexibleArray");
        return;
    case NeedsReader::Container:
        out.print("Container");
        return;
    case NeedsReader::Atomic:
        out.print("Atomic");
        return;
    case NeedsReader::ObjectiveCObject:
        out.print("ObjectiveCObject");
        return;
    case NeedsReader::SwiftPointer:
        out.print("SwiftPointer");
        return;
    case NeedsReader::SwiftValue:
        out.print("SwiftValue");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

void printInternal(PrintStream& out, JSC::Corpse::TargetOpaque::Kind kind)
{
    using Kind = JSC::Corpse::TargetOpaque::Kind;
    switch (kind) {
    case Kind::Scalar:
        out.print("Scalar");
        return;
    case Kind::Enumeration:
        out.print("Enumeration");
        return;
    case Kind::MemberPointer:
        out.print("MemberPointer");
        return;
    case Kind::Vector:
        out.print("Vector");
        return;
    case Kind::Function:
        out.print("Function");
        return;
    case Kind::IgnoredDeclaration:
        out.print("IgnoredDeclaration");
        return;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WTF

#endif // ENABLE(MYA_HEAP)
