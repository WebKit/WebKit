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

namespace JSC::Wasm {

// Subtype/RecursionGroup/Projection are parser-internal classes.
//
// TypeIndex (uintptr_t) is overloaded to carry one of:
//   - Abstract heap type (small negative number representing a TypeKind).
//   - Bare RTT pointer (untagged, low bits all zero).
//   - Bare Subtype pointer (tag bit subtypeTagBit set, used in
//     RecursionGroup::types() to discriminate Subtype members from concrete
//     RTT members).
//   - Bare Projection pointer (tag bit projectionTagBit set, used in
//     Subtype::superTypes() for intra-rec-group supertype refs and in
//     Type::index for placeholder ref types in canonical RTT payloads --
//     the latter is the same bit;).
//
// projectionTagBit and subtypeTagBit are independent (different positions);
// each context knows which (if any) tag may be set on the indices it sees.
static constexpr TypeIndex projectionTagBit = 1; // bit 0
static constexpr TypeIndex subtypeTagBit = 2;    // bit 1

inline const Projection* untagProjection(TypeIndex idx) { return std::bit_cast<const Projection*>(idx & ~projectionTagBit); }
inline const Subtype* untagSubtype(TypeIndex idx) { return std::bit_cast<const Subtype*>(idx & ~subtypeTagBit); }

inline TypeIndex tagAsSubtype(const Subtype* s) { return std::bit_cast<TypeIndex>(s) | subtypeTagBit; }
inline TypeIndex tagAsProjection(const Projection* p) { return std::bit_cast<TypeIndex>(p) | projectionTagBit; }

inline bool isPlaceholderRef(TypeIndex idx) { return (idx & projectionTagBit) != 0; }

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

    TypeIndex index() const { return std::bit_cast<TypeIndex>(this); }
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
    mutable RefPtr<const RTT> m_rtt;
    const Ref<const RTT> m_underlyingRTT;
    Vector<TypeIndex> m_superTypes;
    bool m_final;
};

// Parser-internal sum type for "the result of parsing one type entry".
// The variant alternatives correspond to what the parser can produce:
//
//   - Ref<const RTT>: Function/Struct/Array kinds (already canonical).
//   - const Subtype*: explicit (sub ...) declaration.
//   - const RecursionGroup*: (rec ...) declaration.
//   - const Projection*: shorthand-recursive types are wrapped in a singleton
//     RecursionGroup + Projection pair during parsing.
//
// Subtype/RecursionGroup/Projection are parser-local -- owned by
// TypeSectionState, which outlives every ParsedDef -- so raw pointers are
// safe. RTTs are still refcounted and shared across modules, hence Ref.
class ParsedDef {
public:
    using Variant = WTF::Variant<Ref<const RTT>, const Subtype*, const RecursionGroup*, const Projection*>;

    ParsedDef() = default;
    ParsedDef(Ref<const RTT>&& v) : m_v(Variant { WTF::move(v) }) { }
    ParsedDef(const Subtype* v) : m_v(Variant { v }) { }
    ParsedDef(const RecursionGroup* v) : m_v(Variant { v }) { }
    ParsedDef(const Projection* v) : m_v(Variant { v }) { }

    bool isRTT() const { return m_v && std::holds_alternative<Ref<const RTT>>(*m_v); }
    bool isSubtype() const { return m_v && std::holds_alternative<const Subtype*>(*m_v); }
    bool isRecursionGroup() const { return m_v && std::holds_alternative<const RecursionGroup*>(*m_v); }
    bool isProjection() const { return m_v && std::holds_alternative<const Projection*>(*m_v); }

    const RTT& asRTT() const { return std::get<Ref<const RTT>>(*m_v).get(); }
    const Subtype* asSubtype() const { return std::get<const Subtype*>(*m_v); }
    const RecursionGroup* asRecursionGroup() const { return std::get<const RecursionGroup*>(*m_v); }
    const Projection* asProjection() const { return std::get<const Projection*>(*m_v); }

    bool hasRecursiveReference() const;
    // Encoded TypeIndex (with the subtypeTagBit / projectionTagBit
    // conventions) suitable for use as a recursion group's typeIndex or as
    // the "index" of this parsed type in canonicalization keys.
    TypeIndex index() const;
    Ref<const RTT> canonicalRTT() const;

    bool operator!() const { return !m_v.has_value(); }
    explicit operator bool() const { return m_v.has_value(); }

private:
    std::optional<Variant> m_v;
};

class RecursionGroup final {
    WTF_MAKE_NONCOPYABLE(RecursionGroup);
    WTF_MAKE_NONMOVABLE(RecursionGroup);
public:
    // Members in `types` are TypeIndex values: either bare RTT* (untagged)
    // for concrete-kind members, or tagged Subtype* (subtypeTagBit) for
    // Subtype members. The caller is responsible for setting the tag.
    explicit RecursionGroup(Vector<TypeIndex>&& types)
        : m_types(WTF::move(types))
    {
    }

    TypeIndex index() const { return std::bit_cast<TypeIndex>(this); }
    RecursionGroupCount typeCount() const { return m_types.size(); }
    TypeIndex type(RecursionGroupCount i) const { return m_types[i]; }
    std::span<const TypeIndex> types() const LIFETIME_BOUND { return m_types.span(); }

    String toString() const;
    void dump(WTF::PrintStream& out) const;

private:
    Vector<TypeIndex> m_types;
};

// A projection into a recursion group. m_recursionGroup is null for
// placeholders (intra-rec-group refs created at parse time before the actual
// group exists); after substitution it points to the real RecursionGroup.
class Projection final {
    WTF_MAKE_NONCOPYABLE(Projection);
    WTF_MAKE_NONMOVABLE(Projection);
public:
    // recursionGroup TypeIndex: pass 0 for placeholders, otherwise pass the
    // RecursionGroup*'s index (untagged).
    Projection(TypeIndex recursionGroup, ProjectionIndex projectionIndex)
        : m_recursionGroup(recursionGroup)
        , m_projectionIndex(projectionIndex)
    {
    }

    TypeIndex index() const { return std::bit_cast<TypeIndex>(this); }
    TypeIndex recursionGroup() const { return m_recursionGroup; }
    ProjectionIndex projectionIndex() const { return m_projectionIndex; }
    const RTT* rtt() const { return m_rtt.get(); }

    String toString() const;
    void dump(WTF::PrintStream& out) const;

    static constexpr TypeIndex PlaceholderGroup = 0;
    bool isPlaceholder() const { return recursionGroup() == PlaceholderGroup; }

    void setRTT(Ref<const RTT> rtt) const
    {
        m_rtt = WTF::move(rtt);
    }

private:
    mutable RefPtr<const RTT> m_rtt;
    TypeIndex m_recursionGroup;
    ProjectionIndex m_projectionIndex;
};

struct SubtypeHash {
    static unsigned hash(const Subtype*);
    static bool equal(const Subtype*, const Subtype*);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct RecursionGroupHash {
    static unsigned hash(const RecursionGroup*);
    static bool equal(const RecursionGroup*, const RecursionGroup*);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct ProjectionHash {
    static unsigned hash(const Projection*);
    static bool equal(const Projection*, const Projection*);
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

unsigned computeSubtypeHash(std::span<const TypeIndex> superTypes, TypeIndex underlyingRTT, bool isFinal);
unsigned computeRecursionGroupHash(std::span<const TypeIndex> types);
unsigned computeProjectionHash(TypeIndex recursionGroup, ProjectionIndex);

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

    // Create and dedup within this section. Pointer identity is stable for the
    // state's lifetime; callers may keep raw pointers as long as the state
    // outlives them.
    //
    // createSubtype / createRecursionGroup take Vector<TypeIndex>&& so the
    // caller's locally-built Vector is moved into the new object on a cache
    // miss (or dropped on a hit) -- no copy either way.
    const Subtype* createSubtype(Vector<TypeIndex>&& superTypes, Ref<const RTT> underlyingRTT, bool isFinal);
    const Projection* createProjection(TypeIndex recursionGroup, ProjectionIndex);
    const Projection* createProjectionDirect(TypeIndex recursionGroup, ProjectionIndex);

    const RecursionGroup* createRecursionGroup(Vector<TypeIndex>&& types);
    const Projection* createPlaceholderProjection(ProjectionIndex);

    // Intra-rec-group reference substitution. Placeholder Projections in
    // `type` / `parent` (tagged with the placeholder bit) get rewritten to
    // real Projection refs into the recursion group named by `projectee`.
    Type substitute(Type, TypeIndex projectee);
    TypeIndex substituteParent(TypeIndex parent, TypeIndex projectee);

    // Lazily build the candidate canonical RTT for a parser-local Subtype or
    // Projection and cache it via setRTT.
    void registerCanonicalRTT(const Subtype&);
    void registerCanonicalRTT(const Projection&);

private:
    Ref<const RTT> createCanonicalRTT(const Subtype&);
    Ref<const RTT> createCanonicalRTT(const Projection&);

    SegmentedVector<Subtype, 64> m_subtypeStorage;
    SegmentedVector<Projection, 64> m_projectionStorage;
    SegmentedVector<RecursionGroup, 4> m_recursionGroupStorage;

    UncheckedKeyHashSet<const Subtype*, SubtypeHash> m_subtypeDedup;
    UncheckedKeyHashSet<const Projection*, ProjectionHash> m_projectionDedup;
    UncheckedKeyHashSet<const RecursionGroup*, RecursionGroupHash> m_recursionGroupDedup;
};

} // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
