/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "WasmTypeDefinition.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCJSValueInlines.h"
#include "JSWebAssemblyArray.h"
#include "JSWebAssemblyException.h"
#include "JSWebAssemblyStruct.h"
#include "WasmCallee.h"
#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"
#include "WasmTypeSectionState.h"
#include "WebAssemblyFunctionBase.h"
#include <wtf/CommaPrinter.h>
#include <wtf/FastMalloc.h>
#include <wtf/StringPrintStream.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC { namespace Wasm {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TypeInformation);

void StorageType::dump(PrintStream& out) const
{
    if (is<Type>())
        out.print(makeString(as<Type>().kind));
    else {
        ASSERT(is<PackedType>());
        out.print(makeString(as<PackedType>()));
    }
}

RefPtr<RTT> RTT::tryCreateFunction(bool isFinalType, RTTFunctionPayload&& payload)
{
    auto result = tryFastMalloc(allocationSize(std::max(1u, inlinedDisplaySize)));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(RTTKind::Function, isFinalType, /*fieldCount*/ 0, WTF::move(payload)));
}

RefPtr<RTT> RTT::tryCreateFunction(const RTT& supertype, bool isFinalType, RTTFunctionPayload&& payload)
{
    unsigned allocationCount = std::max(supertype.displaySizeExcludingThis() + 2, inlinedDisplaySize);
    auto result = tryFastMalloc(allocationSize(allocationCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(RTTKind::Function, supertype, isFinalType, /*fieldCount*/ 0, WTF::move(payload)));
}

RefPtr<RTT> RTT::tryCreateStruct(bool isFinalType, RTTStructPayload&& payload)
{
    StructFieldCount fieldCount = payload.fieldCount();
    auto result = tryFastMalloc(allocationSize(std::max(1u, inlinedDisplaySize)));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(RTTKind::Struct, isFinalType, fieldCount, WTF::move(payload)));
}

RefPtr<RTT> RTT::tryCreateStruct(const RTT& supertype, bool isFinalType, RTTStructPayload&& payload)
{
    StructFieldCount fieldCount = payload.fieldCount();
    unsigned allocationCount = std::max(supertype.displaySizeExcludingThis() + 2, inlinedDisplaySize);
    auto result = tryFastMalloc(allocationSize(allocationCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(RTTKind::Struct, supertype, isFinalType, fieldCount, WTF::move(payload)));
}

RefPtr<RTT> RTT::tryCreateArray(bool isFinalType, RTTArrayPayload&& payload)
{
    auto result = tryFastMalloc(allocationSize(std::max(1u, inlinedDisplaySize)));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(RTTKind::Array, isFinalType, /*fieldCount*/ 0, WTF::move(payload)));
}

RefPtr<RTT> RTT::tryCreateArray(const RTT& supertype, bool isFinalType, RTTArrayPayload&& payload)
{
    unsigned allocationCount = std::max(supertype.displaySizeExcludingThis() + 2, inlinedDisplaySize);
    auto result = tryFastMalloc(allocationSize(allocationCount));
    void* memory = nullptr;
    if (!result.getValue(memory))
        return nullptr;
    return adoptRef(new (NotNull, memory) RTT(RTTKind::Array, supertype, isFinalType, /*fieldCount*/ 0, WTF::move(payload)));
}

bool RTT::isSubRTT(const RTT& parent) const
{
    if (this == &parent)
        return true;
    if (displaySizeExcludingThis() < parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

bool RTT::isStrictSubRTT(const RTT& parent) const
{
    if (displaySizeExcludingThis() <= parent.displaySizeExcludingThis())
        return false;
    return &parent == displayEntry(parent.displaySizeExcludingThis());
}

String RTT::toString() const
{
    return WTF::toString(*this);
}

RTT::~RTT() = default;

void RTT::rewriteInternalRefs(TypeSectionState* state, const Vector<Ref<const RTT>>& groupMembers, TypeIndex recursionGroupIndex)
{
    // Walk every ref-bearing TypeSlot in the payload. For each placeholder
    // Projection ref, rewrite the slot's Type::index to the canonical RTT
    // pointer AND set the slot's anchor to a Ref to that RTT in the same
    // step. External (non-placeholder) refs were already anchored at
    // construction time; assert we don't double-anchor them.
    visitChildrenRTT([&](TypeSlot& slot) {
        Type t = slot.type;
        if (!isRefWithTypeIndex(t))
            return;
        if (t.index == invalidTypeIndex)
            return;
        // Internal recgroup refs are tagged Projection pointers (placeholder
        // tag bit set) produced by replacePlaceholders/substitute. Untagged
        // indices are bare RTT* (already canonical) and don't need rewriting.
        if (!isPlaceholderRef(t.index))
            return;
        ASSERT(!slot.rttAnchor);
        const Projection* projection = untagProjection(t.index);
        RefPtr<const RTT> canonical = nullptr;
        if (projection->recursionGroup() == recursionGroupIndex) {
            ProjectionIndex pi = projection->projectionIndex();
            ASSERT(pi < groupMembers.size());
            canonical = groupMembers[pi].ptr();
        } else {
            // Cross-group projection: the projection must have been
            // registered when its own recursion group was canonicalized
            // (parser processes recgroups in declaration order). If state
            // is provided, ensure registration lazily; if not (standalone
            // RTT canonicalization), the projection's rtt() must already be set.
            if (state && !projection->rtt())
                state->registerCanonicalRTT(*projection);
            canonical = projection->rtt();
            RELEASE_ASSERT(canonical);
        }
        slot.type = Type { t.kind, std::bit_cast<TypeIndex>(canonical.get()) };
        slot.rttAnchor = WTF::move(canonical);
    });
}

void RTT::clearReferencedRTTs()
{
    visitChildrenRTT([](TypeSlot& slot) {
        slot.rttAnchor = nullptr;
    });
}

void RTT::dump(PrintStream& out) const
{
    switch (kind()) {
    case RTTKind::Function: {
        out.print("("_s);
        {
            CommaPrinter comma;
            for (FunctionArgCount arg = 0; arg < argumentCount(); ++arg)
                out.print(comma, makeString(argumentType(arg).kind));
        }
        out.print(")"_s);
        out.print(" -> ["_s);
        {
            CommaPrinter comma;
            for (FunctionArgCount ret = 0; ret < returnCount(); ++ret)
                out.print(comma, makeString(returnType(ret).kind));
        }
        out.print("]"_s);
        return;
    }
    case RTTKind::Struct: {
        out.print("("_s);
        CommaPrinter comma;
        for (StructFieldCount fieldIndex = 0; fieldIndex < fieldCount(); ++fieldIndex)
            out.print(comma, field(fieldIndex).mutability ? "immutable "_s : "mutable "_s, makeString(field(fieldIndex).type));
        out.print(")"_s);
        return;
    }
    case RTTKind::Array: {
        out.print("("_s);
        out.print(elementType().mutability ? "immutable "_s : "mutable "_s, makeString(elementType().type));
        out.print(")"_s);
        return;
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
}

const RTT& TypeInformation::signatureForJSException()
{
    return *singleton().m_Void_Externref;
}

TypeInformation::TypeInformation()
{
    // m_Void_Externref must be canonicalized so the JS-side WebAssembly.JSTag
    // (whose RTT is m_Void_Externref) matches a wasm module's imported
    // `(tag (param externref))` whose RTT comes from the canonical recgroup
    // table.
    m_Void_Externref = canonicalizeStandaloneRTTImpl(typeDefinitionForFunction({ }, { externrefType() }));
}

// Returns a Ref to an external (already-canonical) RTT referenced by this Type,
// or nullptr if the Type is not such a ref. Used to anchor RTT lifetimes from
// payloads that point at other RTTs (cycles allowed; see RTT comment).
RefPtr<const RTT> TypeInformation::extractExternalRTT(Type type)
{
    if (!isRefWithTypeIndex(type))
        return nullptr;
    if (type.index == invalidTypeIndex)
        return nullptr;
    // Placeholder Projection refs are intra-recgroup; they get rewritten to
    // canonical RTT pointers later (rewriteInternalRefs), at which point we
    // anchor them. Skip here.
    if (isPlaceholderRef(type.index))
        return nullptr;
    return std::bit_cast<const RTT*>(type.index);
}

// Returns either { internal: true, projectionIndex } if the ref points to a
// member of the post-rewrite recursion group, otherwise { internal: false,
// externalRTT }. Both branches assume Type::index encodes a canonical RTT
// pointer (RTT::rewriteInternalRefs is called before this so internal refs
// are also RTT pointers, not Projection pointers). The IntraLookup callable
// signals intra-group membership; it returns std::optional<size_t> where a
// present value is the relative projection index within the current
// canonical group, and nullopt means the ref is external.
struct EncodedRef {
    bool internal { false };
    union {
        ProjectionIndex projectionIndex;
        SUPPRESS_UNCOUNTED_MEMBER const RTT* externalRTT;
    };
};

template<typename IntraLookup>
inline EncodedRef encodeRef(Type type, IntraLookup&& intra)
{
    EncodedRef out;
    if (!isRefWithTypeIndex(type)) {
        out.internal = false;
        out.externalRTT = nullptr;
        return out;
    }
    const RTT* rtt = std::bit_cast<const RTT*>(type.index);
    if (auto idx = intra(rtt)) {
        out.internal = true;
        out.projectionIndex = static_cast<ProjectionIndex>(*idx);
        return out;
    }
    out.internal = false;
    out.externalRTT = rtt;
    return out;
}

template<typename IntraLookup>
inline unsigned hashType(Type type, IntraLookup&& intra)
{
    unsigned h = WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(type.kind));
    if (isRefWithTypeIndex(type)) {
        EncodedRef r = encodeRef(type, intra);
        if (r.internal)
            h = WTF::pairIntHash(h, r.projectionIndex | 0x80000000u);
        else
            h = WTF::pairIntHash(h, WTF::PtrHash<const RTT*>::hash(r.externalRTT));
    } else if (type.index)
        h = WTF::pairIntHash(h, static_cast<unsigned>(type.index));
    return h;
}

template<typename IntraLookupA, typename IntraLookupB>
inline bool equalTypes(Type a, IntraLookupA&& aIntra, Type b, IntraLookupB&& bIntra)
{
    if (a.kind != b.kind)
        return false;
    if (isRefWithTypeIndex(a)) {
        if (!isRefWithTypeIndex(b))
            return false;
        EncodedRef ra = encodeRef(a, aIntra);
        EncodedRef rb = encodeRef(b, bIntra);
        if (ra.internal != rb.internal)
            return false;
        if (ra.internal)
            return ra.projectionIndex == rb.projectionIndex;
        return ra.externalRTT == rb.externalRTT;
    }
    return a.index == b.index;
}

template<typename IntraLookup>
inline unsigned hashFieldType(FieldType field, IntraLookup&& intra)
{
    unsigned h = static_cast<unsigned>(field.mutability);
    if (field.type.is<PackedType>())
        h = WTF::pairIntHash(h, 0x40000000u | static_cast<unsigned>(field.type.as<PackedType>()));
    else
        h = WTF::pairIntHash(h, hashType(field.type.as<Type>(), intra));
    return h;
}

template<typename IntraLookupA, typename IntraLookupB>
inline bool equalFieldTypes(FieldType a, IntraLookupA&& aIntra, FieldType b, IntraLookupB&& bIntra)
{
    if (a.mutability != b.mutability)
        return false;
    if (a.type.is<PackedType>() != b.type.is<PackedType>())
        return false;
    if (a.type.is<PackedType>())
        return a.type.as<PackedType>() == b.type.as<PackedType>();
    return equalTypes(a.type.as<Type>(), aIntra, b.type.as<Type>(), bIntra);
}

template<typename IntraLookup>
unsigned hashRTTForRecGroup(const RTT& rtt, IntraLookup&& intra)
{
    unsigned h = static_cast<unsigned>(rtt.kind());
    h = WTF::pairIntHash(h, rtt.isFinalType() ? 1 : 0);
    h = WTF::pairIntHash(h, rtt.displaySizeExcludingThis());
    if (rtt.displaySizeExcludingThis()) {
        const RTT* superRTT = rtt.displayEntry(rtt.displaySizeExcludingThis() - 1);
        if (auto idx = intra(superRTT))
            h = WTF::pairIntHash(h, static_cast<unsigned>(*idx) | 0x80000000u);
        else
            h = WTF::pairIntHash(h, WTF::PtrHash<const RTT*>::hash(superRTT));
    }
    switch (rtt.kind()) {
    case RTTKind::Function: {
        const auto& payload = rtt.functionPayload();
        h = WTF::pairIntHash(h, payload.argumentCount());
        h = WTF::pairIntHash(h, payload.returnCount());
        for (FunctionArgCount i = 0; i < payload.argumentCount(); ++i)
            h = WTF::pairIntHash(h, hashType(payload.argumentType(i), intra));
        for (FunctionArgCount i = 0; i < payload.returnCount(); ++i)
            h = WTF::pairIntHash(h, hashType(payload.returnType(i), intra));
        break;
    }
    case RTTKind::Struct: {
        const auto& payload = rtt.structPayload();
        h = WTF::pairIntHash(h, payload.fieldCount());
        for (StructFieldCount i = 0; i < payload.fieldCount(); ++i)
            h = WTF::pairIntHash(h, hashFieldType(payload.field(i), intra));
        break;
    }
    case RTTKind::Array:
        h = WTF::pairIntHash(h, hashFieldType(rtt.arrayPayload().elementType(), intra));
        break;
    }
    return h;
}

template<typename IntraLookupA, typename IntraLookupB>
bool equalRTTsForRecGroup(const RTT& a, IntraLookupA&& aIntra, const RTT& b, IntraLookupB&& bIntra)
{
    // Cheap rejects first: kind / is_final / display depth / per-kind arity.
    if (a.kind() != b.kind())
        return false;
    if (a.isFinalType() != b.isFinalType())
        return false;
    if (a.displaySizeExcludingThis() != b.displaySizeExcludingThis())
        return false;
    switch (a.kind()) {
    case RTTKind::Function: {
        const auto& pa = a.functionPayload();
        const auto& pb = b.functionPayload();
        if (pa.argumentCount() != pb.argumentCount() || pa.returnCount() != pb.returnCount())
            return false;
        if (pa.argumentsOrResultsIncludeI64() != pb.argumentsOrResultsIncludeI64())
            return false;
        if (pa.argumentsOrResultsIncludeV128() != pb.argumentsOrResultsIncludeV128())
            return false;
        if (pa.argumentsOrResultsIncludeExnref() != pb.argumentsOrResultsIncludeExnref())
            return false;
        break;
    }
    case RTTKind::Struct:
        if (a.structPayload().fieldCount() != b.structPayload().fieldCount())
            return false;
        break;
    case RTTKind::Array:
        break;
    }

    if (a.displaySizeExcludingThis()) {
        const RTT* aSuper = a.displayEntry(a.displaySizeExcludingThis() - 1);
        const RTT* bSuper = b.displayEntry(b.displaySizeExcludingThis() - 1);
        auto aIdx = aIntra(aSuper);
        auto bIdx = bIntra(bSuper);
        bool aInternal = aIdx.has_value();
        bool bInternal = bIdx.has_value();
        if (aInternal != bInternal)
            return false;
        if (aInternal) {
            if (*aIdx != *bIdx)
                return false;
        } else if (aSuper != bSuper)
            return false;
    }
    switch (a.kind()) {
    case RTTKind::Function: {
        const auto& pa = a.functionPayload();
        const auto& pb = b.functionPayload();
        for (FunctionArgCount i = 0; i < pa.argumentCount(); ++i) {
            if (!equalTypes(pa.argumentType(i), aIntra, pb.argumentType(i), bIntra))
                return false;
        }
        for (FunctionArgCount i = 0; i < pa.returnCount(); ++i) {
            if (!equalTypes(pa.returnType(i), aIntra, pb.returnType(i), bIntra))
                return false;
        }
        return true;
    }
    case RTTKind::Struct: {
        const auto& pa = a.structPayload();
        const auto& pb = b.structPayload();
        for (StructFieldCount i = 0; i < pa.fieldCount(); ++i) {
            if (!equalFieldTypes(pa.field(i), aIntra, pb.field(i), bIntra))
                return false;
        }
        return true;
    }
    case RTTKind::Array:
        return equalFieldTypes(a.arrayPayload().elementType(), aIntra, b.arrayPayload().elementType(), bIntra);
    }
    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

// Multi-member recgroup lookup: constant-time check against the candidate
// entry's groupId. Each canonical RTT carries its m_canonicalGroupId and
// m_canonicalIndexInGroup, set once during canonicalization. Mirrors V8's
// RecursionGroupRange::Contains arithmetic.
struct GroupIdLookup {
    uint32_t groupId;
    std::optional<size_t> operator()(const RTT* rtt) const
    {
        if (rtt->canonicalGroupId() == groupId)
            return rtt->canonicalIndexInGroup();
        return std::nullopt;
    }
};

// Singleton lookup: only matches the entry's own RTT at projection index 0.
struct SingletonSelfRef {
    SUPPRESS_UNCOUNTED_MEMBER const RTT* self;
    std::optional<size_t> operator()(const RTT* rtt) const
    {
        if (rtt == self)
            return size_t { 0 };
        return std::nullopt;
    }
};

Ref<const RTT> TypeInformation::typeDefinitionForFunction(const Vector<Type, 16>& results, const Vector<Type, 16>& args)
{
    ASSERT(!results.contains(Wasm::Types::Void));
    ASSERT(!args.contains(Wasm::Types::Void));

    // Build the RTT payload directly from the parser-supplied types.
    bool hasRecursiveReference = false;
    bool argumentsOrResultsIncludeI64 = false;
    bool argumentsOrResultsIncludeV128 = false;
    bool argumentsOrResultsIncludeExnref = false;

    // Build the signature directly as TypeSlots so external RTT refs are
    // anchored inline -- no separate addReferencedRTT pass.
    Vector<TypeSlot, 16> signatureBuffer(results.size() + args.size());
    for (unsigned i = 0; i < results.size(); ++i) {
        signatureBuffer[i] = TypeSlot { results[i], extractExternalRTT(results[i]) };
        hasRecursiveReference |= isRefWithRecursiveReference(results[i]);
        argumentsOrResultsIncludeI64 |= results[i].isI64();
        argumentsOrResultsIncludeV128 |= results[i].isV128();
        argumentsOrResultsIncludeExnref |= isExnref(results[i]);
    }
    for (unsigned i = 0; i < args.size(); ++i) {
        signatureBuffer[results.size() + i] = TypeSlot { args[i], extractExternalRTT(args[i]) };
        hasRecursiveReference |= isRefWithRecursiveReference(args[i]);
        argumentsOrResultsIncludeI64 |= args[i].isI64();
        argumentsOrResultsIncludeV128 |= args[i].isV128();
        argumentsOrResultsIncludeExnref |= isExnref(args[i]);
    }

    RTTFunctionPayload payload {
        static_cast<FunctionArgCount>(args.size()),
        static_cast<FunctionArgCount>(results.size()),
        signatureBuffer.span(),
        argumentsOrResultsIncludeI64,
        argumentsOrResultsIncludeV128,
        argumentsOrResultsIncludeExnref,
        hasRecursiveReference
    };
    auto rtt = RTT::tryCreateFunction(/*isFinalType=*/true, WTF::move(payload));
    RELEASE_ASSERT(rtt);
    return rtt.releaseNonNull();
}

Ref<const RTT> TypeInformation::rttForFunction(const Vector<Type, 16>& returnTypes, const Vector<Type, 16>& argumentTypes)
{
    // Canonicalize so RTT identity matches signatures minted later by parseType.
    // Used by JS API entry points (e.g., WebAssembly.Tag, WebAssembly.Function)
    // where two structurally-identical signatures must compare equal.
    return canonicalizeStandaloneRTT(typeDefinitionForFunction(returnTypes, argumentTypes));
}

Ref<const RTT> TypeInformation::typeDefinitionForStruct(const Vector<FieldType>& fields)
{
    bool hasRefFieldTypes = false;
    bool hasRecursiveReference = false;
    unsigned currentFieldOffset = 0;
    auto entries = FixedVector<StructFieldEntry>::createWithSizeFromGenerator(fields.size(), [&](size_t i) -> StructFieldEntry {
        const FieldType& fieldType = fields[i];
        hasRefFieldTypes |= isRefType(fieldType.type);
        hasRecursiveReference |= isRefWithRecursiveReference(fieldType.type);
        currentFieldOffset = WTF::roundUpToMultipleOf(typeAlignmentInBytes(fieldType.type), currentFieldOffset);
        unsigned offset = currentFieldOffset;
        currentFieldOffset += typeSizeInBytes(fieldType.type);
        // Anchor external RTT ref inline (null for PackedType or non-ref Type).
        RefPtr<const RTT> anchor;
        if (fieldType.type.is<Type>())
            anchor = extractExternalRTT(fieldType.type.as<Type>());
        return StructFieldEntry { fieldType, offset, WTF::move(anchor) };
    });
    size_t instancePayloadSize = WTF::roundUpToMultipleOf<sizeof(uint64_t)>(currentFieldOffset);
    RTTStructPayload payload {
        WTF::move(entries),
        instancePayloadSize,
        hasRefFieldTypes,
        hasRecursiveReference,
    };
    auto rtt = RTT::tryCreateStruct(/*isFinalType=*/true, WTF::move(payload));
    RELEASE_ASSERT(rtt);
    return rtt.releaseNonNull();
}

Ref<const RTT> TypeInformation::typeDefinitionForArray(FieldType elementType)
{
    RefPtr<const RTT> anchor;
    if (elementType.type.is<Type>())
        anchor = extractExternalRTT(elementType.type.as<Type>());
    RTTArrayPayload payload {
        elementType,
        WTF::move(anchor),
        isRefWithRecursiveReference(elementType.type)
    };
    auto rtt = RTT::tryCreateArray(/*isFinalType=*/true, WTF::move(payload));
    RELEASE_ASSERT(rtt);
    return rtt.releaseNonNull();
}

bool TypeInformation::isRefWithRecursiveReference(Type type)
{
    // External (canonical) Type::index values are RTT pointers; they never
    // name a placeholder. Parser-internal placeholder Projections are wrapped
    // in TypeIndex via placeholderRefIndex().
    return isRefWithTypeIndex(type) && isPlaceholderRef(type.index);
}

bool TypeInformation::isRefWithRecursiveReference(StorageType storageType)
{
    if (storageType.is<PackedType>())
        return false;
    return isRefWithRecursiveReference(storageType.as<Type>());
}

Ref<const RTT> TypeInformation::getCanonicalRTT(TypeIndex type)
{
    // External TypeIndex form: a direct RTT pointer. No lookup needed.
    return *std::bit_cast<const RTT*>(type);
}

// =====================================================================
// Isorecursive recursion-group canonicalization.
//
// Two recursion groups parsed from different modules should produce the SAME
// canonical RTT instances when their structures are equal. The hard case is
// recursive recgroups: each module's parser builds Projection objects to
// express intra-group references, but those Projections live at distinct
// addresses per module. So the per-member RTT payloads contain *different*
// Projection pointers across modules even when the recgroup is structurally
// identical.
//
// We canonicalize at the recgroup level using relative-index encoding for
// intra-group references in the structural payload (mirroring V8's
// CanonicalHashing/CanonicalEquality).
//
// A Type ref inside a member's payload is "internal" iff
// TypeInformation::get(type.index) is a Projection whose recursionGroup ==
// the candidate's recursionGroupIndex. In that case the ref is hashed/
// compared by the relative projection index. Otherwise the ref is "external"
// and is hashed/compared by the canonical RTT pointer obtained from
// TypeInformation::get(type.index).rtt(). This way, modules that have
// already canonicalized their external dependencies (true once we walk
// recgroups in their declared order) produce the same key.
// =====================================================================
unsigned CanonicalRecursionGroupEntryHash::hash(const CanonicalRecursionGroupEntry& entry)
{
    GroupIdLookup lookup { entry.groupId };
    unsigned h = entry.rtts.size();
    for (const auto& rtt : entry.rtts)
        h = WTF::pairIntHash(h, hashRTTForRecGroup(rtt, lookup));
    return h;
}

bool CanonicalRecursionGroupEntryHash::equal(const CanonicalRecursionGroupEntry& a, const CanonicalRecursionGroupEntry& b)
{
    if (a.rtts.size() != b.rtts.size())
        return false;
    GroupIdLookup aLookup { a.groupId };
    GroupIdLookup bLookup { b.groupId };
    for (size_t i = 0; i < a.rtts.size(); ++i) {
        if (!equalRTTsForRecGroup(a.rtts[i], aLookup, b.rtts[i], bLookup))
            return false;
    }
    return true;
}

bool CanonicalRecursionGroupEntry::operator==(const CanonicalRecursionGroupEntry& other) const
{
    return CanonicalRecursionGroupEntryHash::equal(*this, other);
}

unsigned CanonicalSingletonEntryHash::hash(const CanonicalSingletonEntry& entry)
{
    ASSERT(entry.rtt);
    return hashRTTForRecGroup(*entry.rtt, SingletonSelfRef { entry.rtt.get() });
}

bool CanonicalSingletonEntryHash::equal(const CanonicalSingletonEntry& a, const CanonicalSingletonEntry& b)
{
    if (!a.rtt || !b.rtt)
        return a.rtt == b.rtt;
    return equalRTTsForRecGroup(*a.rtt, SingletonSelfRef { a.rtt.get() }, *b.rtt, SingletonSelfRef { b.rtt.get() });
}

bool CanonicalSingletonEntry::operator==(const CanonicalSingletonEntry& other) const
{
    return CanonicalSingletonEntryHash::equal(*this, other);
}

Vector<Ref<const RTT>> TypeInformation::canonicalizeRecursionGroup(TypeSectionState* state, TypeIndex recursionGroupIndex, Vector<Ref<const RTT>>&& candidateRTTs)
{
    TypeInformation& info = singleton();
    return info.canonicalizeRecursionGroupImpl(state, recursionGroupIndex, WTF::move(candidateRTTs));
}

Ref<const RTT> TypeInformation::canonicalizeSingleton(TypeSectionState* state, TypeIndex recursionGroupIndex, Ref<const RTT>&& candidate)
{
    TypeInformation& info = singleton();
    return info.canonicalizeSingletonImpl(state, recursionGroupIndex, WTF::move(candidate));
}

Vector<Ref<const RTT>> TypeInformation::canonicalizeRecursionGroupImpl(TypeSectionState* state, TypeIndex recursionGroupIndex, Vector<Ref<const RTT>>&& candidateRTTs)
{
    // Size 1 is legal here: a `(rec (<single recursive type>))` -- typeCount==1
    // with self-references -- takes the multi-member path because the singleton
    // fast path in parseRecursionGroup only handles the non-recursive case.
    // The loop/HashSet logic below is correct for any size >= 1.
    ASSERT(!candidateRTTs.isEmpty());

    Locker locker { m_lock };

    // Rewrite internal Projection refs to canonical RTT pointers
    // (self-references among the candidate RTTs themselves). Non-recursive
    // RTTs have no placeholder-tagged refs in their payloads, so skip the
    // rewrite walk entirely for them.
    for (auto& rtt : candidateRTTs) {
        if (rtt->hasRecursiveReference())
            const_cast<RTT&>(rtt.get()).rewriteInternalRefs(state, candidateRTTs, recursionGroupIndex);
    }

    // Assign a tentative groupId and per-member indices to each candidate
    // RTT so hash/equal can O(1)-detect intra-group refs via
    // rtt->canonicalGroupId() == entry.groupId (no Vector scan, no HashMap).
    // If the candidate turns out to match an existing canonical entry, the
    // candidate RTTs are dropped (fresh allocations with no outside owners);
    // their tentative ids never leak out.
    uint32_t candidateGroupId = m_nextCanonicalGroupId++;
    for (uint32_t i = 0; i < candidateRTTs.size(); ++i)
        candidateRTTs[i]->setCanonicalGroup(candidateGroupId, i);

    // Retain external refs to the candidate RTTs across the HashSet::add
    // below. If the candidate turns out to be a duplicate, the moved-in
    // entry is destroyed, dropping its owning Vector. The candidates would
    // otherwise keep each other alive through the intra-group rttAnchor
    // cycles created by rewriteInternalRefs -- we break those cycles
    // explicitly here. Inline capacity keeps this off the heap for the
    // common small-recgroup case.
    Vector<Ref<const RTT>, 8> retainer(candidateRTTs.size(), [&](size_t i) {
        return candidateRTTs[i].copyRef();
    });

    CanonicalRecursionGroupEntry candidate { recursionGroupIndex, candidateGroupId, WTF::move(candidateRTTs) };
    auto addResult = m_canonicalRecursionGroups.add(WTF::move(candidate));

    if (!addResult.isNewEntry) {
        // Duplicate: break the intra-group cycles so the candidate RTTs
        // (now referenced only by retainer) can actually be freed when
        // retainer goes out of scope.
        for (auto& rtt : retainer)
            const_cast<RTT&>(rtt.get()).clearReferencedRTTs();
    }
    // After add, addResult.iterator->rtts is either the existing canonical RTTs
    // or the just-inserted candidate's RTTs (both work).
    return addResult.iterator->rtts;
}

Ref<const RTT> TypeInformation::canonicalizeSingletonImpl(TypeSectionState* state, TypeIndex recursionGroupIndex, Ref<const RTT>&& candidate)
{
    // Rewrite intra-group placeholder refs to the candidate RTT* so the
    // singleton hash/equal can detect self-refs via pointer identity.
    // Non-recursive payloads have nothing to rewrite; skip the walk.
    bool hasRecursiveReference = candidate->hasRecursiveReference();
    if (hasRecursiveReference) {
        // rewriteInternalRefs consults the Vector<Ref<const RTT>>& to map
        // projection indices to RTT pointers. A singleton's only valid
        // projection index is 0, pointing at the candidate itself.
        Vector<Ref<const RTT>> groupMembers;
        groupMembers.append(candidate.copyRef());
        const_cast<RTT&>(candidate.get()).rewriteInternalRefs(state, groupMembers, recursionGroupIndex);
    }

    Locker locker { m_lock };
    // Retain an external ref across HashSet::add so we can break the
    // self-cycle that rewriteInternalRefs wrote into the candidate's
    // TypeSlot anchors if the candidate is discarded as a duplicate. Only
    // needed when the candidate actually went through rewriteInternalRefs
    // (recursive case); non-recursive singletons have no self-refs to break.
    Ref<const RTT> retainer = candidate.copyRef();
    CanonicalSingletonEntry entry { WTF::move(candidate) };
    auto addResult = m_canonicalSingletonGroups.add(WTF::move(entry));
    if (!addResult.isNewEntry && hasRecursiveReference)
        const_cast<RTT&>(retainer.get()).clearReferencedRTTs();
    return Ref<const RTT> { *addResult.iterator->rtt };
}

Ref<const RTT> TypeInformation::canonicalizeStandaloneRTT(Ref<const RTT>&& candidate)
{
    return singleton().canonicalizeStandaloneRTTImpl(WTF::move(candidate));
}

Ref<const RTT> TypeInformation::canonicalizeStandaloneRTTImpl(Ref<const RTT>&& candidate)
{
    // Standalone RTTs (built-in signatures) have no recursive references
    // and no TypeSectionState. They are always singletons. recursionGroupIndex
    // is irrelevant since there are no placeholder refs to rewrite.
    ASSERT(!candidate->hasRecursiveReference());
    return canonicalizeSingletonImpl(nullptr, 0, WTF::move(candidate));
}

bool TypeInformation::isReferenceValueAssignable(JSValue refValue, bool allowNull, TypeIndex typeIndex)
{
    if (refValue.isNull())
        return allowNull;

    if (typeIndexIsType(typeIndex)) {
        switch (static_cast<TypeKind>(typeIndex)) {
        case TypeKind::Externref:
        case TypeKind::Anyref:
            // Casts to these types cannot fail as any value can be an externref/hostref.
            return true;
        case TypeKind::Funcref:
            return dynamicDowncast<WebAssemblyFunctionBase>(refValue);
        case TypeKind::Eqref:
            return (refValue.isInt32() && refValue.asInt32() <= maxI31ref && refValue.asInt32() >= minI31ref) || is<JSWebAssemblyArray>(refValue) || is<JSWebAssemblyStruct>(refValue);
        case TypeKind::Exnref:
            // Exnref and Noexnref are in a different heap hierarchy
            return dynamicDowncast<JSWebAssemblyException>(refValue);
        case TypeKind::Noexnref:
        case TypeKind::Noneref:
        case TypeKind::Nofuncref:
        case TypeKind::Noexternref:
            return false;
        case TypeKind::I31ref:
            return refValue.isInt32() && refValue.asInt32() <= maxI31ref && refValue.asInt32() >= minI31ref;
        case TypeKind::Arrayref:
            return dynamicDowncast<JSWebAssemblyArray>(refValue);
        case TypeKind::Structref:
            return dynamicDowncast<JSWebAssemblyStruct>(refValue);
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
        return false;
    }

    RefPtr rtt = TypeInformation::getCanonicalRTT(typeIndex);
    switch (rtt->kind()) {
    case RTTKind::Function: {
        WebAssemblyFunctionBase* funcRef = dynamicDowncast<WebAssemblyFunctionBase>(refValue);
        if (!funcRef)
            return false;
        return funcRef->rtt()->isSubRTT(*rtt);
    }
    case RTTKind::Array:
    case RTTKind::Struct: {
        auto* object = dynamicDowncast<WebAssemblyGCObjectBase>(refValue);
        if (!object)
            return false;
        return object->rtt().isSubRTT(*rtt);
    }
    }

    RELEASE_ASSERT_NOT_REACHED();
    return false;
}

void TypeInformation::tryCleanup()
{
    auto& info = singleton();
    Locker locker { info.m_lock };

    bool changed = false;
    do {
        changed = false;
        info.m_canonicalSingletonGroups.removeIf([&](const CanonicalSingletonEntry& entry) {
            if (entry.rtt && entry.rtt->hasOneRef()) {
                changed = true;
                return true;
            }
            return false;
        });
    } while (changed);

    // Multi-member m_canonicalRecursionGroups: skipped. Members anchor each
    // other through TypeSlot::rttAnchor (rewriteInternalRefs builds the
    // cycles intentionally), so per-member hasOneRef() never fires. A
    // correct collector would have to detect that the entire group's
    // external refcount is zero and drop all members atomically; not worth
    // the complexity until profiling justifies it.
}

bool NODELETE Type::definitelyIsCellOrNull() const
{
    if (!isRefType(*this))
        return false;

    if (typeIndexIsType(index)) {
        switch (static_cast<TypeKind>(index)) {
        case TypeKind::Funcref:
        case TypeKind::Arrayref:
        case TypeKind::Structref:
        case TypeKind::Exnref:
            return true;
        default:
            return false;
        }
    }
    return true;
}

bool Type::definitelyIsWasmGCObjectOrNull() const
{
    if (!isRefType(*this))
        return false;

    if (typeIndexIsType(index)) {
        switch (static_cast<TypeKind>(index)) {
        case TypeKind::Arrayref:
        case TypeKind::Structref:
            return true;
        default:
            return false;
        }
    }

    if (RefPtr rtt = TypeInformation::tryGetRTT(index))
        return rtt->kind() == RTTKind::Struct || rtt->kind() == RTTKind::Array;

    return false;
}

void Type::dump(PrintStream& out) const
{
    TypeKind kindToPrint = kind;
    if (index != invalidTypeIndex) {
        if (typeIndexIsType(index)) {
            // If the index is negative, it represents a TypeKind.
            kindToPrint = static_cast<TypeKind>(index);
        } else if (index & projectionTagBit) {
            out.print(*untagProjection(index));
            return;
        } else {
            out.print(*reinterpret_cast<const RTT*>(index));
            return;
        }
    }
    switch (kindToPrint) {
#define CREATE_CASE(name, ...) case TypeKind::name: out.print(#name); break;
        FOR_EACH_WASM_TYPE(CREATE_CASE)
#undef CREATE_CASE
    }
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
