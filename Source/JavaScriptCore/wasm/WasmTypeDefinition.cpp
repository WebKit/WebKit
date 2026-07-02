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
#include "Options.h"
#include "WasmCallee.h"
#include "WasmCallingConvention.h"
#include "WasmFormat.h"
#include "WasmIPIntGenerator.h"
#include "WasmTypeDefinitionInlines.h"
#include "WasmTypeSectionState.h"
#include "WebAssemblyFunctionBase.h"
#include <wtf/CommaPrinter.h>
#include <wtf/DataLog.h>
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

RTTGroup::~RTTGroup()
{
    // Null out each member's back-pointer so a member that outlives the
    // group via an external Ref reports canonicalGroup() == nullptr
    // (preventing a hash/equal false-positive if another group's address
    // were later reused). With today's tryCleanup this destructor fires
    // only for the duplicate-on-insert path where the members are about to
    // be destroyed too -- the null-out is defensive for future reclamation.
    for (auto& rtt : m_rtts)
        rtt->setCanonicalGroup(nullptr, 0);
}

void RTT::rewriteInternalRefs(TypeSectionState* state, std::span<const Ref<const RTT>> groupMembers, const RecursionGroup* recursionGroup)
{
    // Walk every ref-bearing TypeSlot in the payload. For each placeholder
    // Projection ref, rewrite the slot's Type::index to the canonical RTT
    // pointer and anchor a Ref to that RTT in the same step. External
    // (non-placeholder) refs were already anchored at construction time by
    // extractExternalRTT and take the !isPlaceholderRef early-return below
    // -- they're not touched here. The assert verifies the dual invariant:
    // extractExternalRTT must NOT have anchored a placeholder slot
    // (placeholders get a null anchor at construction, then get their anchor
    // here). After rewriting, the slot's index is a bare RTT* so a second
    // pass would early-return -- the assert is not protecting against
    // double-rewriting in that direct sense.
    visitChildrenRTT([&](TypeSlot& slot) {
        Type t = slot.type;
        if (!isRefWithTypeIndex(t))
            return;
        if (t.index == invalidTypeIndex)
            return;
        // Placeholder-tagged Projection pointers are emitted by the parser
        // (createPlaceholderProjection in WasmParser.h / WasmSectionParser.cpp)
        // for intra-rec-group refs. Untagged indices are bare RTT* (already
        // canonical) and don't need rewriting.
        if (!isPlaceholderRef(t.index))
            return;
        ASSERT(!slot.rttAnchor);
        const Projection* projection = untagProjection(t.index);
        RefPtr<const RTT> canonical = nullptr;
        if (projection->recursionGroup() == recursionGroup) {
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
        slot.type = Type { t.kind, canonical->asTypeIndex() };
        slot.rttAnchor = WTF::move(canonical);
    });
}

void RTT::clearReferencedRTTs()
{
    visitChildrenRTT([](TypeSlot& slot) {
        slot.rttAnchor = nullptr;
    });
}

void RTT::clearAllDisplayRefs()
{
    for (unsigned i = 0; i < (m_displaySizeExcludingThis + 1); ++i)
        at(i) = nullptr;
}

void RTT::setSelfDisplaySlot() const
{
    ASSERT(!at(m_displaySizeExcludingThis));
    const_cast<RTT*>(this)->at(m_displaySizeExcludingThis) = this;
}

void TypeInformation::breakCyclesForReclamation(const RTT& rtt)
{
    RTT& mutableRTT = const_cast<RTT&>(rtt);
    mutableRTT.clearAllDisplayRefs();
    mutableRTT.clearReferencedRTTs();
    rtt.setCanonicalGroup(nullptr, 0);
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

// Returns a Variant<ProjectionIndex, const RTT*>: the first alternative for
// refs that point to a member of the post-rewrite recursion group (internal),
// the second alternative for refs to an already-canonicalized external RTT.
// Both branches assume Type::index encodes a canonical RTT pointer
// (RTT::rewriteInternalRefs is called before this so internal refs are also
// RTT pointers, not Projection pointers). The IntraLookup callable signals
// intra-group membership; it returns std::optional<size_t> where a present
// value is the relative projection index within the current canonical group,
// and nullopt means the ref is external.
using EncodedRef = Variant<ProjectionIndex, const RTT*>;

template<typename IntraLookup>
inline EncodedRef encodeRef(const RTT* rtt, IntraLookup&& intra)
{
    if (auto idx = intra(rtt))
        return EncodedRef { static_cast<ProjectionIndex>(*idx) };
    return EncodedRef { rtt };
}

template<typename IntraLookup>
inline EncodedRef encodeRef(Type type, IntraLookup&& intra)
{
    ASSERT(isRefWithTypeIndex(type));
    return encodeRef(std::bit_cast<const RTT*>(type.index), intra);
}

// High bit on the ProjectionIndex hash keeps the internal and external hash
// spaces disjoint; no canonical RTT* pointer hash will collide with an
// in-group projection index.
inline unsigned hashEncodedRef(EncodedRef r)
{
    return WTF::switchOn(r,
        [](ProjectionIndex pi) -> unsigned { return pi | 0x80000000u; },
        [](const RTT* p) -> unsigned { return WTF::PtrHash<const RTT*>::hash(p); });
}

template<typename IntraLookup>
inline unsigned hashType(Type type, IntraLookup&& intra)
{
    unsigned h = WTF::IntHash<uint8_t>::hash(static_cast<uint8_t>(type.kind));
    if (isRefWithTypeIndex(type))
        h = WTF::pairIntHash(h, hashEncodedRef(encodeRef(type, intra)));
    else if (type.index)
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
        return encodeRef(a, aIntra) == encodeRef(b, bIntra);
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
    if (unsigned hash = rtt.hashMayBeEmpty())
        return hash;

    unsigned h = static_cast<unsigned>(rtt.kind());
    h = WTF::pairIntHash(h, rtt.isFinalType() ? 1 : 0);
    h = WTF::pairIntHash(h, rtt.displaySizeExcludingThis());
    if (rtt.displaySizeExcludingThis()) {
        const RTT* superRTT = rtt.displayEntry(rtt.displaySizeExcludingThis() - 1);
        h = WTF::pairIntHash(h, hashEncodedRef(encodeRef(superRTT, intra)));
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

    rtt.setHash(h);
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
        if (encodeRef(aSuper, aIntra) != encodeRef(bSuper, bIntra))
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
// RTTGroup. Each canonical RTT carries its canonicalGroup() and
// canonicalIndexInGroup(), set once during canonicalization. Mirrors V8's
// RecursionGroupRange::Contains arithmetic.
struct GroupLookup {
    SUPPRESS_UNCOUNTED_MEMBER const RTTGroup& group;
    std::optional<size_t> operator()(const RTT* rtt) const
    {
        if (rtt->canonicalGroup() == &group)
            return rtt->canonicalIndexInGroup();
        return std::nullopt;
    }
};

// Singleton lookup: only matches the entry's own RTT at projection index 0.
struct SingletonSelfRef {
    SUPPRESS_UNCOUNTED_MEMBER const RTT& self;
    std::optional<size_t> operator()(const RTT* rtt) const
    {
        if (rtt == &self)
            return 0;
        return std::nullopt;
    }
};

Ref<const RTT> TypeInformation::typeDefinitionForFunction(const Vector<Type, 16>& results, const Vector<Type, 16>& args)
{
    ASSERT(!results.contains(Wasm::Types::Void));
    ASSERT(!args.contains(Wasm::Types::Void));

    // Build the raw structural RTT. `isFinalType` here is a no-supertype-form
    // default -- when this shape sits under a (sub ...) declaration, the
    // Subtype wrapper in TypeSectionState::createCanonicalRTT rebuilds the
    // final RTT with the declared supertype chain and finality bit.
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
    auto rtt = RTT::tryCreateFunction(/* isFinalType */ true, WTF::move(payload));
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
    auto rtt = RTT::tryCreateStruct(/* isFinalType */ true, WTF::move(payload));
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
    auto rtt = RTT::tryCreateArray(/* isFinalType */ true, WTF::move(payload));
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
// the candidate's recursionGroup. In that case the ref is hashed/
// compared by the relative projection index. Otherwise the ref is "external"
// and is hashed/compared by the canonical RTT pointer obtained from
// TypeInformation::get(type.index).rtt(). This way, modules that have
// already canonicalized their external dependencies (true once we walk
// recgroups in their declared order) produce the same key.
// =====================================================================
unsigned CanonicalRecursionGroupEntryHash::hash(const CanonicalRecursionGroupEntry& entry)
{
    if (unsigned hash = entry.group->hashMayBeEmpty())
        return hash;

    const auto& rtts = entry.group->rtts();
    GroupLookup lookup { entry.group };
    unsigned h = rtts.size();
    for (const auto& rtt : rtts)
        h = WTF::pairIntHash(h, hashRTTForRecGroup(rtt, lookup));

    entry.group->setHash(h);
    return h;
}

bool CanonicalRecursionGroupEntryHash::equal(const CanonicalRecursionGroupEntry& a, const CanonicalRecursionGroupEntry& b)
{
    const auto& aRTTs = a.group->rtts();
    const auto& bRTTs = b.group->rtts();
    if (aRTTs.size() != bRTTs.size())
        return false;
    GroupLookup aLookup { a.group };
    GroupLookup bLookup { b.group };
    for (size_t i = 0; i < aRTTs.size(); ++i) {
        if (!equalRTTsForRecGroup(aRTTs[i], aLookup, bRTTs[i], bLookup))
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
    return hashRTTForRecGroup(entry.rtt.get(), SingletonSelfRef { entry.rtt.get() });
}

bool CanonicalSingletonEntryHash::equal(const CanonicalSingletonEntry& a, const CanonicalSingletonEntry& b)
{
    return equalRTTsForRecGroup(a.rtt.get(), SingletonSelfRef { a.rtt.get() }, b.rtt.get(), SingletonSelfRef { b.rtt.get() });
}

bool CanonicalSingletonEntry::operator==(const CanonicalSingletonEntry& other) const
{
    return CanonicalSingletonEntryHash::equal(*this, other);
}

Vector<Ref<const RTT>> TypeInformation::canonicalizeRecursionGroup(TypeSectionState* state, const RecursionGroup* recursionGroup, Vector<Ref<const RTT>>&& candidateRTTs)
{
    TypeInformation& info = singleton();
    return info.canonicalizeRecursionGroupImpl(state, recursionGroup, WTF::move(candidateRTTs));
}

Ref<const RTT> TypeInformation::canonicalizeSingleton(TypeSectionState* state, const RecursionGroup* recursionGroup, Ref<const RTT>&& candidate)
{
    TypeInformation& info = singleton();
    return info.canonicalizeSingletonImpl(state, recursionGroup, WTF::move(candidate));
}

Ref<const RTT> TypeInformation::canonicalizeSingleton(TypeSectionState* state, Ref<const RTT>&& candidate)
{
    // The no-group overload is for non-recursive candidates only. A recursive
    // candidate would carry placeholder refs whose recursionGroup() == nullptr;
    // emptySingleton() ensures those placeholders never falsely match the
    // singleton's "context" inside rewriteInternalRefs (which would then index
    // a 1-element groupMembers Vector with possibly out-of-range projection
    // indices). Recursive candidates must use the with-group overload.
    ASSERT(!candidate->hasRecursiveReference());
    TypeInformation& info = singleton();
    return info.canonicalizeSingletonImpl(state, RecursionGroup::emptySingleton(), WTF::move(candidate));
}

Vector<Ref<const RTT>> TypeInformation::canonicalizeRecursionGroupImpl(TypeSectionState* state, const RecursionGroup* recursionGroup, Vector<Ref<const RTT>>&& candidateRTTs)
{
    // Size 1 is legal here: a `(rec (<single recursive type>))` -- typeCount==1
    // with self-references -- takes the multi-member path because the singleton
    // fast path in parseRecursionGroup only handles the non-recursive case.
    // The loop/HashSet logic below is correct for any size >= 1.
    ASSERT(!candidateRTTs.isEmpty());

    auto candidateGroup = RTTGroup::create(WTF::move(candidateRTTs));
    for (uint32_t i = 0; i < candidateGroup->size(); ++i) {
        auto& rtt = candidateGroup->at(i);
        // Resolve placeholder Projection refs to bare canonical RTT
        // pointers. Pass candidateGroup->rtts(), not the moved-from
        // candidateRTTs.
        if (rtt.hasRecursiveReference())
            const_cast<RTT&>(rtt).rewriteInternalRefs(state, candidateGroup->rtts().span(), recursionGroup);
        rtt.setSelfDisplaySlot();
        rtt.setCanonicalGroup(candidateGroup.ptr(), i);
    }

    Locker locker { m_lock };
    CanonicalRecursionGroupEntry candidate { candidateGroup.copyRef() };
    auto addResult = m_canonicalRecursionGroups.add(WTF::move(candidate));
    if (!addResult.isNewEntry) {
        for (const Ref<const RTT>& member : candidateGroup->rtts())
            breakCyclesForReclamation(member.get());
    }
    return addResult.iterator->group->rtts();
}

Ref<const RTT> TypeInformation::canonicalizeSingletonImpl(TypeSectionState* state, const RecursionGroup* recursionGroup, Ref<const RTT>&& candidate)
{
    // Rewrite self-ref placeholders to the candidate RTT so hash/equal can
    // detect self-refs by pointer identity.
    if (candidate->hasRecursiveReference()) {
        Vector<Ref<const RTT>, 1> groupMembers;
        groupMembers.append(candidate.copyRef());
        const_cast<RTT&>(candidate.get()).rewriteInternalRefs(state, groupMembers.span(), recursionGroup);
    }
    candidate->setSelfDisplaySlot();

    Locker locker { m_lock };
    CanonicalSingletonEntry entry { candidate.copyRef() };
    auto addResult = m_canonicalSingletonGroups.add(WTF::move(entry));
    if (!addResult.isNewEntry)
        breakCyclesForReclamation(candidate.get());
    return addResult.iterator->rtt.copyRef();
}

Ref<const RTT> TypeInformation::canonicalizeStandaloneRTT(Ref<const RTT>&& candidate)
{
    return singleton().canonicalizeStandaloneRTTImpl(WTF::move(candidate));
}

Ref<const RTT> TypeInformation::canonicalizeStandaloneRTTImpl(Ref<const RTT>&& candidate)
{
    // Standalone RTTs (built-in signatures) have no recursive references
    // and no TypeSectionState. They are always singletons. emptySingleton()
    // is passed as the recursion group context; it is irrelevant here since
    // the assert below ensures rewriteInternalRefs is never invoked.
    ASSERT(!candidate->hasRecursiveReference());
    return canonicalizeSingletonImpl(nullptr, RecursionGroup::emptySingleton(), WTF::move(candidate));
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

    // Bacon-Rajan synchronous cycle collection. Snapshot -> trial-decrement
    // internal edges -> restore via BFS from roots -> sweep zeros.

    auto forEachCandidateGroup = [&](auto&& cb) {
        for (const auto& entry : info.m_canonicalRecursionGroups)
            cb(entry.group.get());
    };
    auto forEachCandidateRTT = [&](auto&& cb) {
        forEachCandidateGroup([&](auto& group) {
            for (const auto& rtt : group.rtts())
                cb(rtt.get());
        });
        for (const auto& entry : info.m_canonicalSingletonGroups)
            cb(entry.rtt.get());
    };

    // Phase 1: snapshot refCount() into scratch.
    forEachCandidateGroup([](const RTTGroup& group) {
        group.setVirtualRefCount(group.refCount());
    });
    forEachCandidateRTT([](const RTT& rtt) {
        rtt.setVirtualRefCount(rtt.refCount());
    });

    auto decRTT = [](const RTT* tgt) {
        if (!tgt)
            return;
        ASSERT(tgt->virtualRefCount() > 0);
        tgt->setVirtualRefCount(tgt->virtualRefCount() - 1);
    };
    auto decGroup = [](const RTTGroup* tgt) {
        if (!tgt)
            return;
        ASSERT(tgt->virtualRefCount() > 0);
        tgt->setVirtualRefCount(tgt->virtualRefCount() - 1);
    };

    auto decEdgesFromRTT = [&](const RTT& rtt) {
        decGroup(rtt.canonicalGroup());
        rtt.forEachPayloadRTTRef(decRTT);
        for (unsigned i = 0; i <= rtt.displaySizeExcludingThis(); ++i)
            decRTT(rtt.displayEntry(i));
    };

    // Phase 2: trial-decrement every internal structural edge.
    // Unlike original Bacon-Rajan, we do not need tri-color graph traversal since
    // all RTTs and groups are reachable from m_canonicalRecursionGroups and m_canonicalSingletonGroups.

    // Visit each group and RTT, perform trial-decrement for each edge.
    // This edge includes edges from m_canonicalRecursionGroups / m_canonicalSingletonGroups.
    for (const auto& entry : info.m_canonicalRecursionGroups) {
        const RTTGroup& group = entry.group.get();
        decGroup(&group);
        for (const auto& rtt : group.rtts()) {
            decRTT(rtt.ptr());
            decEdgesFromRTT(rtt.get());
        }
    }
    for (const auto& entry : info.m_canonicalSingletonGroups) {
        decRTT(entry.rtt.ptr());
        decEdgesFromRTT(entry.rtt.get());
    }

    // Phase 3: transitive-closure restore from any object still > 0.
    Vector<const RTT*, 16> rttWorklist;
    Vector<const RTTGroup*, 16> groupWorklist;

    // After trial-decrement, if group / RTT are still non-zero count,
    // this indicates that they are actually referenced outside of this registry.
    forEachCandidateRTT([&](const RTT& rtt) {
        if (rtt.virtualRefCount() > 0)
            rttWorklist.append(&rtt);
    });
    forEachCandidateGroup([&](const RTTGroup& group) {
        if (group.virtualRefCount() > 0)
            groupWorklist.append(&group);
    });

    // Then restore virtualRefCount non-zero which are reachable from the above really live RTTs and groups.
    auto incRTT = [&](const RTT* tgt) {
        if (!tgt)
            return;
        bool wasZero = !tgt->virtualRefCount();
        tgt->setVirtualRefCount(tgt->virtualRefCount() + 1);
        if (wasZero)
            rttWorklist.append(tgt);
    };
    auto incGroup = [&](const RTTGroup* tgt) {
        if (!tgt)
            return;
        bool wasZero = !tgt->virtualRefCount();
        tgt->setVirtualRefCount(tgt->virtualRefCount() + 1);
        if (wasZero)
            groupWorklist.append(tgt);
    };

    while (!rttWorklist.isEmpty() || !groupWorklist.isEmpty()) {
        while (!groupWorklist.isEmpty()) {
            const RTTGroup* group = groupWorklist.takeLast();
            for (const auto& rtt : group->rtts())
                incRTT(rtt.ptr());
        }
        while (!rttWorklist.isEmpty()) {
            const RTT* rtt = rttWorklist.takeLast();
            incGroup(rtt->canonicalGroup());
            rtt->forEachPayloadRTTRef(incRTT);
            for (unsigned i = 0; i <= rtt->displaySizeExcludingThis(); ++i)
                incRTT(rtt->displayEntry(i));
        }
    }

    // Phase 4: sweep objects whose virtualRefCount stayed 0.
    size_t groupsBeforeSweep = info.m_canonicalRecursionGroups.size();
    size_t singletonsBeforeSweep = info.m_canonicalSingletonGroups.size();
    info.m_canonicalRecursionGroups.removeIf([&](const CanonicalRecursionGroupEntry& entry) {
        const RTTGroup& group = entry.group.get();
        if (group.virtualRefCount() > 0)
            return false;
        // Clear intra-group display / payload / m_group refs so the natural
        // destructor cascade can drive each member's refcount to 0 once the
        // table entry drops.
        for (const auto& member : group.rtts())
            breakCyclesForReclamation(member.get());
        return true;
    });

    info.m_canonicalSingletonGroups.removeIf([&](const CanonicalSingletonEntry& entry) {
        if (entry.rtt->virtualRefCount() > 0)
            return false;
        breakCyclesForReclamation(entry.rtt.get());
        return true;
    });

    if (Options::verboseWasmTypeCleanup()) [[unlikely]] {
        size_t groupsAfter = info.m_canonicalRecursionGroups.size();
        size_t singletonsAfter = info.m_canonicalSingletonGroups.size();
        dataLogLn("[Wasm::TypeInformation::tryCleanup] groups scanned=", groupsBeforeSweep,
            " reclaimed=", groupsBeforeSweep - groupsAfter,
            " live=", groupsAfter,
            " | singletons scanned=", singletonsBeforeSweep,
            " reclaimed=", singletonsBeforeSweep - singletonsAfter,
            " live=", singletonsAfter);
    }
}

size_t TypeInformation::canonicalTypeCount()
{
    auto& info = singleton();
    Locker locker { info.m_lock };
    return info.m_canonicalRecursionGroups.size() + info.m_canonicalSingletonGroups.size();
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

void RTT::ensureArgumINTBytecode(const CallInformation& callCC) const
{
    ASSERT(kind() == RTTKind::Function);

    constexpr static int NUM_ARGUMINT_GPRS = 8;
    constexpr static int NUM_ARGUMINT_FPRS = 8;

    ASSERT_UNUSED(NUM_ARGUMINT_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_ARGUMINT_GPRS);
    ASSERT_UNUSED(NUM_ARGUMINT_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_ARGUMINT_FPRS);

    m_argumINTBytecode.ensure([&] {
        auto numArgs = argumentCount();
        auto candidate = IPIntSharedBytecode::createWithSizeFromGenerator(numArgs + 1,
            [&](size_t index) -> uint8_t {
                if (index == numArgs)
                    return static_cast<uint8_t>(IPInt::ArgumINTBytecode::End);

                const ArgumentLocation& argLoc = callCC.params[index];
                const ValueLocation& loc = argLoc.location;

                if (loc.isGPR()) {
#if USE(JSVALUE64)
                    ASSERT_UNUSED(NUM_ARGUMINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().gpr()) < NUM_ARGUMINT_GPRS);
                    return static_cast<uint8_t>(IPInt::ArgumINTBytecode::ArgGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr());
#elif USE(JSVALUE32_64)
                    ASSERT_UNUSED(NUM_ARGUMINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_ARGUMINT_GPRS);
                    ASSERT_UNUSED(NUM_ARGUMINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().tagGPR()) < NUM_ARGUMINT_GPRS);
                    return static_cast<uint8_t>(IPInt::ArgumINTBytecode::ArgGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord)) / 2;
#endif
                }

                if (loc.isFPR()) {
                    ASSERT_UNUSED(NUM_ARGUMINT_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_ARGUMINT_FPRS);
                    return static_cast<uint8_t>(IPInt::ArgumINTBytecode::ArgFPR) + FPRInfo::toArgumentIndex(loc.fpr());
                }

                RELEASE_ASSERT(loc.isStack());
                switch (argLoc.width) {
                case Width::Width64:
                    return static_cast<uint8_t>(IPInt::ArgumINTBytecode::Stack);
                case Width::Width128:
                    return static_cast<uint8_t>(IPInt::ArgumINTBytecode::StackVector);
                default:
                    RELEASE_ASSERT_NOT_REACHED("No argumINT bytecode for result width");
                }
            });

        ASSERT(candidate->size() == numArgs + 1u);
        ASSERT(candidate->last() == static_cast<uint8_t>(IPInt::ArgumINTBytecode::End));
        return candidate;
    });
}

void RTT::ensureUINTBytecode(const CallInformation& returnCC) const
{
    ASSERT(kind() == RTTKind::Function);

    // uINT: the interpreter smaller than mINT
    constexpr static int NUM_UINT_GPRS = 8;
    constexpr static int NUM_UINT_FPRS = 8;
    ASSERT_UNUSED(NUM_UINT_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_UINT_GPRS);
    ASSERT_UNUSED(NUM_UINT_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_UINT_FPRS);

    m_uINTBytecode.ensure([&] {
        // Offset past the last stack-typed return, in signature order.
        uint32_t topOfReturnStackFPOffset = 0;
        for (const auto& argLoc : returnCC.results) {
            if (argLoc.location.isStack())
                topOfReturnStackFPOffset = argLoc.location.offsetFromFP() + bytesForWidth(argLoc.width);
        }

        auto encode = [&](unsigned index) -> uint8_t {
            const ArgumentLocation& argLoc = returnCC.results[index];
            const ValueLocation& loc = argLoc.location;

            if (loc.isGPR()) {
#if USE(JSVALUE64)
                ASSERT_UNUSED(NUM_UINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().gpr()) < NUM_UINT_GPRS);
                return static_cast<uint8_t>(IPInt::UINTBytecode::RetGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr());
#elif USE(JSVALUE32_64)
                ASSERT_UNUSED(NUM_UINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_UINT_GPRS);
                ASSERT_UNUSED(NUM_UINT_GPRS, GPRInfo::toArgumentIndex(loc.jsr().tagGPR()) < NUM_UINT_GPRS);
                return static_cast<uint8_t>(IPInt::UINTBytecode::RetGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord));
#endif
            }

            if (loc.isFPR()) {
                ASSERT_UNUSED(NUM_UINT_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_UINT_FPRS);
                return static_cast<uint8_t>(IPInt::UINTBytecode::RetFPR) + FPRInfo::toArgumentIndex(loc.fpr());
            }

            RELEASE_ASSERT(loc.isStack());
            switch (argLoc.width) {
            case Width::Width64:
                return static_cast<uint8_t>(IPInt::UINTBytecode::Stack);
            case Width::Width128:
                return static_cast<uint8_t>(IPInt::UINTBytecode::StackVector);
            default:
                RELEASE_ASSERT_NOT_REACHED("No uINT bytecode for result width");
            }
        };

        // Layout: [topOfReturnStackFPOffset : u32][encode(last)..encode(first)][End].
        // Results are consumed in reverse by the uINT dispatcher, so emit in reverse.
        unsigned size = returnCC.results.size();
        auto headerBytes = std::bit_cast<std::array<uint8_t, sizeof(uint32_t)>>(topOfReturnStackFPOffset);
        auto candidate = IPIntSharedBytecode::createWithSizeFromGenerator(sizeof(uint32_t) + size + 1,
            [&](size_t index) -> uint8_t {
                if (index < sizeof(uint32_t))
                    return headerBytes[index];
                size_t i = index - sizeof(uint32_t);
                if (i == size)
                    return static_cast<uint8_t>(IPInt::UINTBytecode::End);
                return encode(size - 1 - i);
            });

        ASSERT(candidate->size() == sizeof(uint32_t) + size + 1u);
        ASSERT(candidate->last() == static_cast<uint8_t>(IPInt::UINTBytecode::End));
        return candidate;
    });
}

template<bool isTailCall>
static Vector<uint8_t, 16> buildCallArgumentBytecode(const CallInformation& callConvention)
{
    constexpr static int NUM_MINT_CALL_GPRS = 8;
    constexpr static int NUM_MINT_CALL_FPRS = 8;
    ASSERT_UNUSED(NUM_MINT_CALL_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_MINT_CALL_GPRS);
    ASSERT_UNUSED(NUM_MINT_CALL_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_MINT_CALL_FPRS);

    auto toBytecodeUint8 = [](IPInt::CallArgumentBytecode bytecode) {
        constexpr uint8_t tailBytecodeOffset = static_cast<uint8_t>(IPInt::CallArgumentBytecode::TailCallArgDecSP) - static_cast<uint8_t>(IPInt::CallArgumentBytecode::CallArgDecSP);
        uint8_t bytecodeUint8 = static_cast<uint8_t>(bytecode);
        ASSERT(static_cast<uint8_t>(IPInt::CallArgumentBytecode::CallArgDecSP) <= bytecodeUint8
            && bytecodeUint8 <= static_cast<uint8_t>(IPInt::CallArgumentBytecode::CallArgDecSPStoreVector8));
        if constexpr (isTailCall)
            bytecodeUint8 += tailBytecodeOffset;
        return bytecodeUint8;
    };

    Vector<uint8_t, 16> results;
    results.append(static_cast<uint8_t>(isTailCall ? IPInt::CallArgumentBytecode::TailCall : IPInt::CallArgumentBytecode::Call));

    intptr_t spOffset = callConvention.headerIncludingThisSizeInBytes;
    auto isAligned16 = [&spOffset]() ALWAYS_INLINE_LAMBDA {
        return !(spOffset & 0xf);
    };

    ASSERT(isAligned16());
    results.appendUsingFunctor(callConvention.params.size(),
        [&](unsigned index) -> uint8_t {
            const ArgumentLocation& argLoc = callConvention.params[index];
            const ValueLocation& loc = argLoc.location;

            if (loc.isGPR()) {
#if USE(JSVALUE64)
                ASSERT_UNUSED(NUM_MINT_CALL_GPRS, GPRInfo::toArgumentIndex(loc.jsr().gpr()) < NUM_MINT_CALL_GPRS);
                return static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr());
#elif USE(JSVALUE32_64)
                ASSERT_UNUSED(NUM_MINT_CALL_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_MINT_CALL_GPRS);
                ASSERT_UNUSED(NUM_MINT_CALL_GPRS, GPRInfo::toArgumentIndex(loc.jsr().tagGPR()) < NUM_MINT_CALL_GPRS);
                return static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord));
#endif
            }

            if (loc.isFPR()) {
                ASSERT_UNUSED(NUM_MINT_CALL_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_MINT_CALL_FPRS);
                return static_cast<uint8_t>(IPInt::CallArgumentBytecode::ArgumentFPR) + FPRInfo::toArgumentIndex(loc.fpr());
            }
            RELEASE_ASSERT(loc.isStackArgument());
            ASSERT(loc.offsetFromSP() == spOffset);
            IPInt::CallArgumentBytecode bytecode;
            switch (argLoc.width) {
            case Width64:
                bytecode = isAligned16() ? IPInt::CallArgumentBytecode::CallArgStore0 : IPInt::CallArgumentBytecode::CallArgDecSPStore8;
                spOffset += 8;
                break;
            case Width128:
                bytecode = isAligned16() ? IPInt::CallArgumentBytecode::CallArgDecSPStoreVector0 : IPInt::CallArgumentBytecode::CallArgDecSPStoreVector8;
                spOffset += 16;
                break;
            default:
                RELEASE_ASSERT_NOT_REACHED("No bytecode for stack argument location width");
            }
            return toBytecodeUint8(bytecode);
        });

    if (!isAligned16()) {
        spOffset += 8;
        results.append(toBytecodeUint8(IPInt::CallArgumentBytecode::CallArgDecSP));
    }
    intptr_t frameSize = roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes);
    ASSERT(frameSize >= spOffset);
    ASSERT(isAligned16());
    for (; spOffset < frameSize; spOffset += 16)
        results.append(toBytecodeUint8(IPInt::CallArgumentBytecode::CallArgDecSP));
    ASSERT(spOffset == frameSize);

    results.reverse();
    return results;
}

static intptr_t buildCallResultBytecode(Vector<uint8_t, 16>& results, const CallInformation& callConvention)
{
    constexpr static int NUM_MINT_RET_GPRS = 8;
    constexpr static int NUM_MINT_RET_FPRS = 8;
    ASSERT_UNUSED(NUM_MINT_RET_GPRS, wasmCallingConvention().jsrArgs.size() <= NUM_MINT_RET_GPRS);
    ASSERT_UNUSED(NUM_MINT_RET_FPRS, wasmCallingConvention().fprArgs.size() <= NUM_MINT_RET_FPRS);

    intptr_t firstStackResultSPOffset = 0;
    bool hasSeenStackResult = false;
    intptr_t spOffset = 0;

    results.appendUsingFunctor(callConvention.results.size(),
        [&](unsigned index) -> uint8_t {
            const ArgumentLocation& argLoc = callConvention.results[index];
            const ValueLocation& loc = argLoc.location;

            if (loc.isGPR()) {
                ASSERT_UNUSED(NUM_MINT_RET_GPRS, GPRInfo::toArgumentIndex(loc.jsr().payloadGPR()) < NUM_MINT_RET_GPRS);
#if USE(JSVALUE64)
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr());
#elif USE(JSVALUE32_64)
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultGPR) + GPRInfo::toArgumentIndex(loc.jsr().gpr(WhichValueWord::PayloadWord));
#endif
            }

            if (loc.isFPR()) {
                ASSERT_UNUSED(NUM_MINT_RET_FPRS, FPRInfo::toArgumentIndex(loc.fpr()) < NUM_MINT_RET_FPRS);
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultFPR) + FPRInfo::toArgumentIndex(loc.fpr());
            }
            RELEASE_ASSERT(loc.isStackArgument());

            if (!hasSeenStackResult) {
                hasSeenStackResult = true;
                spOffset = loc.offsetFromSP();
                firstStackResultSPOffset = spOffset;
            }
            ASSERT(loc.offsetFromSP() == spOffset);
            switch (argLoc.width) {
            case Width::Width64:
                spOffset += 8;
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultStack);
            case Width::Width128:
                spOffset += 16;
                return static_cast<uint8_t>(IPInt::CallResultBytecode::ResultStackVector);
            default:
                ASSERT_NOT_REACHED("No bytecode for stack result location width");
                return 0;
            }
        });
    results.append(static_cast<uint8_t>(IPInt::CallResultBytecode::End));
    return firstStackResultSPOffset;
}

void RTT::ensureCallBytecode() const
{
    ASSERT(kind() == RTTKind::Function);

    m_callBytecode.ensure([&] {
        // Build [arg bytecode][Call][CallReturnMetadata][result bytecode][End] -- the same
        // layout the generator used to emit inline into m_metadata. MC walks the whole
        // thing during one call: arg dispatch leaves MC at CallReturnMetadata (inside the
        // buffer), and ret dispatch continues from there.
        auto callConvention = wasmCallingConvention().callInformationFor(*this, CallRole::Caller);
        Vector<uint8_t, 16> bytes = buildCallArgumentBytecode</* isTailCall */ false>(callConvention);

        Checked<uint32_t> frameSize = WTF::roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes);
        IPInt::CallReturnMetadata returnMeta {
            .stackFrameSize = frameSize,
            .firstStackResultSPOffset = 0,
        };

        Vector<uint8_t, 16> resultBytes;
        Checked<uint32_t> firstStackResultSPOffset = buildCallResultBytecode(resultBytes, callConvention);
        returnMeta.firstStackResultSPOffset = firstStackResultSPOffset;

        auto toSpan = [&](auto& value) {
            auto start = std::bit_cast<const uint8_t*>(&value);
            return std::span { start, start + sizeof(value) };
        };
        bytes.append(toSpan(returnMeta));
        bytes.append(resultBytes.span());

        return IPIntSharedBytecode::createFromVector(WTF::move(bytes));
    });
}

void RTT::ensureTailCallBytecode() const
{
    ASSERT(kind() == RTTKind::Function);

    m_tailCallBytecode.ensure([&] {
        // [arg bytecode][TailCall terminator][stackArgumentsAndResultsInBytes (u32)].
        // The trailing u32 is read by .ipint_perform_tail_call via `loadi [MC]`
        // after mINT dispatch hits TailCall, so MC naturally lands on it.
        auto callConvention = wasmCallingConvention().callInformationFor(*this, CallRole::Caller);
        auto bytes = buildCallArgumentBytecode</* isTailCall */ true>(callConvention);

        uint32_t stackArgumentsAndResultsInBytes = roundUpToMultipleOf<stackAlignmentBytes()>(callConvention.headerAndArgumentStackSizeInBytes) - callConvention.headerIncludingThisSizeInBytes;
        ASSERT(!(stackArgumentsAndResultsInBytes % 16));
        bytes.append(std::span { std::bit_cast<const uint8_t*>(&stackArgumentsAndResultsInBytes), sizeof(stackArgumentsAndResultsInBytes) });

        return IPIntSharedBytecode::createFromVector(WTF::move(bytes));
    });
}

} } // namespace JSC::Wasm

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
