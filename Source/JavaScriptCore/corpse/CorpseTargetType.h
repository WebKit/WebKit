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

#pragma once

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA_HEAP)

#include <JavaScriptCore/CorpseAddress.h>
#include <functional>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Corpse {

struct TargetClass;
struct TargetType;

using TargetTypeRef = std::reference_wrapper<const TargetType>;

// A base or a field of a class, at `offset` in an object of that class.
struct TargetMember {
    enum class Kind : uint8_t {
        Base,
        Field,
    };

    const TargetClass& classType() const;

    Kind kind;
    uint64_t offset;
    const TargetType& type;
    // Empty for a base.
    String name;
};

// A base class subobject, at `offset` in a complete object of the class.
struct TargetBase {
    const TargetClass& classType() const;

    uint64_t offset;
    const TargetType& type;
};

// A class, struct or union, as one image's debug info defines it. Each image that defines a class has a
// TargetClass of its own, since images are never matched by name.
struct TargetClass {
    WTF_MAKE_NONCOPYABLE(TargetClass);
public:
    // Why the debug info does not say what an object of this class holds, so that a reader of its own must.
    enum class NeedsReader : uint8_t {
        No,
        Bitfield,
        Union,
        // A flexible array member, whose length is not in the debug info.
        FlexibleArray,
        // Its pointers do not say how many objects they point to, or which of them are valid.
        Container,
        // libc++ holds the value as an _Atomic(T), which liblldb does not look through.
        Atomic,
        // The Objective-C runtime describes the object.
        ObjectiveCObject,
        // The class holds a pointer to memory the Swift runtime describes.
        SwiftPointer,
        // The class holds the bytes of a Swift value.
        SwiftValue,
    };

    TargetClass(String&& name, String&& image, size_t byteSize, bool hasVTable, NeedsReader, Vector<TargetMember>&& members, Vector<TargetBase>&& virtualBases);

    const String name;
    const String image;
    const size_t byteSize;
    const bool hasVTable;
    const NeedsReader needsReader;
    // The direct non-virtual bases, then the fields. Empty when the class needs a reader.
    const Vector<TargetMember> members;
    // Every virtual base, direct or indirect, where a complete object of this class holds it.
    const Vector<TargetBase> virtualBases;
    // Every base class subobject of a complete object of this class, direct or indirect.
    const Vector<TargetBase> bases;
};

// A class that one image only declares. Only as what a pointer points to: the walk finds the object through an
// image that defines the class.
struct TargetDeclaration {
    String name;
    String image;
};

// A pointer or reference. SnapshotDebugInfo::pointee() describes what it points to the first time the walk
// follows one, so that a class can point to itself.
struct TargetPointer {
    String pointee;
};

// A value that holds nothing the walk follows.
struct TargetOpaque {
    enum class Kind : uint8_t {
        Scalar,
        Enumeration,
        MemberPointer,
        Vector,
        // Only as what a pointer points to.
        Function,
        // A class in CorpseIgnoredDeclarations.h. Only as what a pointer points to.
        IgnoredDeclaration,
    };

    String name;
    size_t byteSize;
    Kind kind;
};

struct TargetArray {
    const TargetType& element;
    uint64_t count;
};

// A type from one image's debug info. Owned by the SnapshotDebugInfo.
struct TargetType : Variant<TargetClass, TargetDeclaration, TargetPointer, TargetOpaque, TargetArray> {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(TargetType);
    using Base = Variant<TargetClass, TargetDeclaration, TargetPointer, TargetOpaque, TargetArray>;
    using Base::Base;
};

String name(const TargetType&);
size_t byteSize(const TargetType&);

// liblldb spells some template arguments differently in different images, such as 1024 and 1024UL.
String withoutTemplateArguments(StringView);
bool sameName(StringView, StringView);

// Whether the two are the same class: the same name, other than its template arguments, and the same
// members of the same structure at the same offsets.
bool sameClass(const TargetClass&, const TargetClass&);

// sameClass() for classes. Other types are compared by structure.
bool sameType(const TargetType&, const TargetType&);

// A class, at offset 0, or its base at `offset`, when that is the same class as the TargetClass.
const TargetType* baseAt(const TargetType&, uint64_t offset, const TargetClass&);

// A class, or its base at offset 0, when that is the class the TargetDeclaration names.
const TargetType* definitionOf(const TargetType&, const TargetDeclaration&);

// What is at `address` in a snapshot, read as `type`.
struct TargetObject {
    Address address;
    const TargetType& type;
};

// What a pointer designates: `subobject`, which is `node`, or one of its bases when `node` is a class.
struct TargetCanonicalPointer {
    TargetObject node;
    TargetObject subobject;
};

} // namespace Corpse
} // namespace JSC

namespace WTF {

void printInternal(PrintStream&, JSC::Corpse::TargetClass::NeedsReader);
void printInternal(PrintStream&, JSC::Corpse::TargetOpaque::Kind);

} // namespace WTF

#endif // ENABLE(MYA_HEAP)
