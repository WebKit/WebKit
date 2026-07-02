/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/WasmTypeDefinition.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/SegmentedVector.h>

namespace JSC { namespace Wasm {

// Subtype/RecursionGroup/Projection are parser-internal classes.
//
// TypeIndex (uintptr_t) is overloaded to carry one of:
//   - Abstract heap type (small negative number representing a TypeKind).
//   - Bare RTT pointer (untagged, low bits all zero).
//   - Bare Subtype pointer (tag bit subtypeTagBit set, used in
//     RecursionGroup::types() to discriminate Subtype members from concrete
//     RTT members).
//   - Bare Projection pointer (tag bit projectionTagBit set). Two contexts
//     use this tag with the same bit: (a) Subtype::superTypes() entries for
//     intra-rec-group supertype refs, and (b) Type::index values carrying a
//     placeholder ref during parsing, before rewriteInternalRefs replaces
//     them with bare canonical RTT*.
//
// projectionTagBit and subtypeTagBit are independent (different positions);
// each context knows which (if any) tag may be set on the indices it sees.
class Subtype;
class Projection;

static constexpr TypeIndex projectionTagBit = 1; // bit 0
static constexpr TypeIndex subtypeTagBit = 2;    // bit 1

inline const Projection* untagProjection(TypeIndex idx) { return std::bit_cast<const Projection*>(idx & ~projectionTagBit); }
inline const Subtype* untagSubtype(TypeIndex idx) { return std::bit_cast<const Subtype*>(idx & ~subtypeTagBit); }

inline TypeIndex tagAsSubtype(const Subtype* s) { return std::bit_cast<TypeIndex>(s) | subtypeTagBit; }
inline TypeIndex tagAsProjection(const Projection* p) { return std::bit_cast<TypeIndex>(p) | projectionTagBit; }

inline bool isPlaceholderRef(TypeIndex idx) { return idx & projectionTagBit; }

class Subtype final {
    WTF_MAKE_NONCOPYABLE(Subtype);
    WTF_MAKE_NONMOVABLE(Subtype);
public:
    Subtype(Vector<TypeIndex>&& superTypes, Ref<const RTT> underlyingRTT, bool isFinal)
        : m_underlyingRTT(WTF::move(underlyingRTT))
        , m_superTypes(WTF::move(superTypes))
        , m_final(isFinal)
    {
    }

    SupertypeCount supertypeCount() const { return m_superTypes.size(); }
    bool isFinalType() const { return m_final; }
    TypeIndex firstSuperType() const { return m_superTypes[0]; }
    TypeIndex superType(SupertypeCount i) const { return m_superTypes[i]; }
    const RTT& underlyingRTT() const LIFETIME_BOUND { return m_underlyingRTT; }
    std::span<const TypeIndex> superTypes() const LIFETIME_BOUND { return m_superTypes.span(); }
    const RTT* rtt() const { return m_rtt.get(); }
    bool hasRecursiveReference() const;

    String toString() const;
    void dump(WTF::PrintStream& out) const;

    void setRTT(Ref<const RTT> rtt) const
    {
        m_rtt = WTF::move(rtt);
    }

private:
    // m_rtt is a lazy cache set by setRTT(); parser-local, not thread-safe.
    mutable RefPtr<const RTT> m_rtt;
    const Ref<const RTT> m_underlyingRTT;
    Vector<TypeIndex> m_superTypes;
    bool m_final { false };
};

// Parser-internal sum type for "the result of parsing one type entry".
// The variant alternatives correspond to what the parser can produce:
//
//   - std::monostate: default-constructed / unset.
//   - Ref<const RTT>: Function/Struct/Array kinds (already canonical).
//   - const Subtype*: explicit (sub ...) declaration.
//
// Subtypes are parser-local -- owned by
// TypeSectionState, which outlives every ParsedDef -- so raw pointers are
// safe. RTTs are still refcounted and shared across modules, hence Ref.
class ParsedDef {
public:
    using Variant = WTF::Variant<std::monostate, Ref<const RTT>, const Subtype*>;

    ParsedDef() = default;
    ParsedDef(Ref<const RTT>&& v) : m_value(WTF::move(v)) { }
    ParsedDef(const Subtype* v) : m_value(v) { }

    bool isRTT() const { return std::holds_alternative<Ref<const RTT>>(m_value); }
    bool isSubtype() const { return std::holds_alternative<const Subtype*>(m_value); }

    const RTT& asRTT() const { return std::get<Ref<const RTT>>(m_value).get(); }
    const Subtype* asSubtype() const { return std::get<const Subtype*>(m_value); }

    bool hasRecursiveReference() const;
    // Encoded TypeIndex (with the subtypeTagBit / projectionTagBit
    // conventions) suitable for use as a recursion group's typeIndex or as
    // the "index" of this parsed type in canonicalization keys.
    TypeIndex index() const;
    Ref<const RTT> canonicalRTT() const;

    bool operator!() const { return std::holds_alternative<std::monostate>(m_value); }
    explicit operator bool() const { return !std::holds_alternative<std::monostate>(m_value); }

private:
    Variant m_value;
};

class RecursionGroup final {
    WTF_MAKE_NONCOPYABLE(RecursionGroup);
    WTF_MAKE_NONMOVABLE(RecursionGroup);
public:
    // Members in `types` are TypeIndex values: either bare RTT* (untagged)
    // for concrete-kind members, or tagged Subtype* (subtypeTagBit) for
    // Subtype members. The caller is responsible for setting the tag.
    RecursionGroup() = default;
    explicit RecursionGroup(Vector<TypeIndex>&& types)
        : m_types(WTF::move(types))
    {
    }

    RecursionGroupCount typeCount() const { return m_types.size(); }
    TypeIndex type(RecursionGroupCount i) const { return m_types[i]; }
    std::span<const TypeIndex> types() const LIFETIME_BOUND { return m_types.span(); }

    String toString() const;
    void dump(WTF::PrintStream& out) const;

    static const RecursionGroup* emptySingleton();

private:
    Vector<TypeIndex> m_types;
};

// A projection into a recursion group. Placeholder Projections carry
// placeholderGroup (== nullptr) as their recursionGroup, created at parse time
// before the actual RecursionGroup exists. Substitution doesn't mutate a
// placeholder -- TypeSectionState::substitute allocates a new Projection with
// the resolved RecursionGroup* and leaves the placeholder as is.
class Projection final {
    WTF_MAKE_NONCOPYABLE(Projection);
    WTF_MAKE_NONMOVABLE(Projection);
public:
    // Pass placeholderGroup (nullptr) for placeholders, otherwise pass the
    // parent RecursionGroup*.
    Projection(const RecursionGroup* recursionGroup, ProjectionIndex projectionIndex)
        : m_recursionGroup(recursionGroup)
        , m_projectionIndex(projectionIndex)
    {
    }

    const RecursionGroup* recursionGroup() const { return m_recursionGroup; }
    ProjectionIndex projectionIndex() const { return m_projectionIndex; }
    const RTT* rtt() const { return m_rtt.get(); }

    String toString() const;
    void dump(WTF::PrintStream& out) const;

    static constexpr RecursionGroup* placeholderGroup = nullptr;
    bool isPlaceholder() const { return !recursionGroup(); }

    void setRTT(Ref<const RTT> rtt) const
    {
        m_rtt = WTF::move(rtt);
    }

private:
    // m_rtt is a lazy cache set by setRTT(); parser-local, not thread-safe.
    mutable RefPtr<const RTT> m_rtt;
    const RecursionGroup* m_recursionGroup;
    ProjectionIndex m_projectionIndex;
};

struct ProjectionHash {
    static unsigned hash(const Projection*);
    static bool equal(const Projection*, const Projection*);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

// Tracks whether the parser is currently inside a recursion group and, if so,
// the [start, end) range of type indices that belong to it. Lives on
// TypeSectionState because it's only meaningful while a type section is being
// parsed; parseValueType / parseHeapType consult it via the parser's
// m_typeSectionState pointer.
struct RecursionGroupInformation {
    bool inRecursionGroup { false };
    uint32_t start { 0 };
    uint32_t end { 0 };
};

class TypeSectionState {
    WTF_MAKE_NONCOPYABLE(TypeSectionState);
    WTF_MAKE_NONMOVABLE(TypeSectionState);
public:
    TypeSectionState() = default;
    ~TypeSectionState() = default;

    RecursionGroupInformation recursionGroupInformation;

    // Projection dedup is required for correctness, not just perf: the
    // parser pre-creates one Projection per rec-group member in Phase 1 and
    // populates each member's cached RTT in Phase 2. During Phase 2, the
    // recursive-subtype unrolling path looks up sibling Projections via
    // substituteParent -> createProjection(group, idx). That lookup must
    // return the *same* Projection* that Phase 2 will setRTT on; otherwise
    // createCanonicalRTT(const Subtype&) reads a null cached rtt for an
    // intra-group supertype and asserts. (Wasm spec guarantees supertype
    // index < self index, so the rtt is always set by the time it's needed
    // -- but only when dedup keeps the pointer identity intact.)
    const Subtype* createSubtype(Vector<TypeIndex>&& superTypes, Ref<const RTT> underlyingRTT, bool isFinal);
    const Projection* createProjection(const RecursionGroup*, ProjectionIndex);

    const RecursionGroup* createRecursionGroup(Vector<TypeIndex>&& types);
    const Projection* createPlaceholderProjection(ProjectionIndex);

    // Lazily build the candidate canonical RTT for a parser-local Subtype or
    // Projection and cache it via setRTT.
    void registerCanonicalRTT(const Subtype&);
    void registerCanonicalRTT(const Projection&);

private:
    // Intra-rec-group reference substitution. Placeholder Projections in
    // `type` / `parent` (tagged with the placeholder bit) get rewritten to
    // real Projection refs into the recursion group named by `projectee`.
    Type substitute(Type, const RecursionGroup* projectee);
    TypeIndex substituteParent(TypeIndex parent, const RecursionGroup* projectee);

    Ref<const RTT> createCanonicalRTT(const Subtype&);
    Ref<const RTT> createCanonicalRTT(const Projection&);

    SegmentedVector<Subtype, 64> m_subtypeStorage;
    SegmentedVector<Projection, 64> m_projectionStorage;
    SegmentedVector<RecursionGroup, 16> m_recursionGroupStorage;
    UncheckedKeyHashSet<const Projection*, ProjectionHash> m_projectionDedup;
};

// The low bits of Subtype*/Projection* pointers are reused as discriminator
// tags (see projectionTagBit / subtypeTagBit above). Enforce alignment so a
// future layout tweak can't silently collide with the tag bits.
static_assert(alignof(Subtype) >= 4, "Subtype* low bits reused as tag");
static_assert(alignof(Projection) >= 4, "Projection* low bits reused as tag");

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
