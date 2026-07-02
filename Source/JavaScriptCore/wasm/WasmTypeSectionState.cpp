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

#include "config.h"
#include "WasmTypeSectionState.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmFormat.h"
#include "WasmTypeDefinitionInlines.h"

namespace JSC { namespace Wasm {

static unsigned computeProjectionHash(const RecursionGroup* recursionGroup, ProjectionIndex projectionIndex)
{
    unsigned accumulator = 0xbeae6d4e;
    accumulator = WTF::pairIntHash(accumulator, WTF::PtrHash<const RecursionGroup*>::hash(recursionGroup));
    accumulator = WTF::pairIntHash(accumulator, WTF::IntHash<ProjectionIndex>::hash(projectionIndex));
    return accumulator;
}

unsigned ProjectionHash::hash(const Projection* p)
{
    if (!p)
        return 0;
    return computeProjectionHash(p->recursionGroup(), p->projectionIndex());
}

bool ProjectionHash::equal(const Projection* a, const Projection* b)
{
    if (!a || !b)
        return a == b;
    return a->recursionGroup() == b->recursionGroup() && a->projectionIndex() == b->projectionIndex();
}

bool ParsedDef::hasRecursiveReference() const
{
    return WTF::switchOn(m_value,
        [](std::monostate) -> bool { RELEASE_ASSERT_NOT_REACHED(); },
        [](const Ref<const RTT>& rtt) { return rtt->hasRecursiveReference(); },
        [](const Subtype* s) { return s->hasRecursiveReference(); });
}

TypeIndex ParsedDef::index() const
{
    return WTF::switchOn(m_value,
        [](std::monostate) -> TypeIndex { RELEASE_ASSERT_NOT_REACHED(); },
        [](const Ref<const RTT>& rtt) -> TypeIndex { return rtt->asTypeIndex(); },
        [](const Subtype* s) -> TypeIndex { return tagAsSubtype(s); });
}

Ref<const RTT> ParsedDef::canonicalRTT() const
{
    return WTF::switchOn(m_value,
        [](std::monostate) -> Ref<const RTT> { RELEASE_ASSERT_NOT_REACHED(); },
        [](const Ref<const RTT>& rtt) -> Ref<const RTT> { return rtt; },
        [](const Subtype* s) -> Ref<const RTT> { RELEASE_ASSERT(s->rtt()); return Ref<const RTT> { *s->rtt() }; });
}

const RecursionGroup* RecursionGroup::emptySingleton()
{
    static LazyNeverDestroyed<RecursionGroup> result;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        result.construct();
    });
    return &result.get();
}

String RecursionGroup::toString() const
{
    return WTF::toString(*this);
}

void RecursionGroup::dump(PrintStream& out) const
{
    out.print("("_s);
    CommaPrinter comma;
    for (RecursionGroupCount typeIndex = 0; typeIndex < typeCount(); ++typeIndex) {
        out.print(comma);
        TypeIndex t = type(typeIndex);
        if (t & subtypeTagBit)
            untagSubtype(t)->dump(out);
        else
            std::bit_cast<const RTT*>(t)->dump(out);
    }
    out.print(")"_s);
}

String Projection::toString() const
{
    return WTF::toString(*this);
}

void Projection::dump(PrintStream& out) const
{
    out.print("("_s);
    CommaPrinter comma;
    if (isPlaceholder())
        out.print("<current-rec-group>"_s);
    else
        recursionGroup()->dump(out);
    out.print("."_s, projectionIndex());
    out.print(")"_s);
}

String Subtype::toString() const
{
    return WTF::toString(*this);
}

void Subtype::dump(PrintStream& out) const
{
    out.print("("_s);
    if (supertypeCount() > 0) {
        TypeIndex parent = firstSuperType();
        if (isPlaceholderRef(parent))
            untagProjection(parent)->dump(out);
        else if (RefPtr rtt = std::bit_cast<const RTT*>(parent))
            rtt->dump(out);
        out.print(", "_s);
    }
    underlyingRTT().dump(out);
    out.print(")"_s);
}

bool Subtype::hasRecursiveReference() const
{
    if (supertypeCount() > 0) {
        if (isPlaceholderRef(firstSuperType()))
            return true;
    }
    return m_underlyingRTT->hasRecursiveReference();
}

struct ProjectionLookupKey {
    const RecursionGroup* recursionGroup;
    ProjectionIndex projectionIndex;
};

struct ProjectionLookupTranslator {
    static unsigned hash(const ProjectionLookupKey& params)
    {
        return computeProjectionHash(params.recursionGroup, params.projectionIndex);
    }
    static bool equal(const Projection* p, const ProjectionLookupKey& params)
    {
        if (!p)
            return false;
        return p->recursionGroup() == params.recursionGroup
            && p->projectionIndex() == params.projectionIndex;
    }
};

const RecursionGroup* TypeSectionState::createRecursionGroup(Vector<TypeIndex>&& types)
{
    return &m_recursionGroupStorage.alloc(WTF::move(types));
}

const Projection* TypeSectionState::createProjection(const RecursionGroup* recursionGroup, ProjectionIndex projectionIndex)
{
    auto result = m_projectionDedup.ensure<ProjectionLookupTranslator>(
        ProjectionLookupKey { recursionGroup, projectionIndex },
        [&] -> const Projection* {
            return &m_projectionStorage.alloc(recursionGroup, projectionIndex);
        });
    return *result.iterator;
}

const Subtype* TypeSectionState::createSubtype(Vector<TypeIndex>&& superTypes, Ref<const RTT> underlyingRTT, bool isFinal)
{
    return &m_subtypeStorage.alloc(WTF::move(superTypes), WTF::move(underlyingRTT), isFinal);
}

const Projection* TypeSectionState::createPlaceholderProjection(ProjectionIndex projectionIndex)
{
    return createProjection(Projection::placeholderGroup, projectionIndex);
}

Type TypeSectionState::substitute(Type type, const RecursionGroup* projectee)
{
    if (isRefWithTypeIndex(type) && isPlaceholderRef(type.index)) {
        const Projection* projection = untagProjection(type.index);
        if (projection->isPlaceholder()) {
            auto* newProjection = createProjection(projectee, projection->projectionIndex());
            TypeKind kind = type.isNullable() ? TypeKind::RefNull : TypeKind::Ref;
            return Type { kind, tagAsProjection(newProjection) };
        }
    }
    return type;
}

TypeIndex TypeSectionState::substituteParent(TypeIndex parent, const RecursionGroup* projectee)
{
    if (isPlaceholderRef(parent)) {
        const Projection* projection = untagProjection(parent);
        if (projection->isPlaceholder())
            return tagAsProjection(createProjection(projectee, projection->projectionIndex()));
    }
    return parent;
}

void TypeSectionState::registerCanonicalRTT(const Subtype& subtype)
{
    if (subtype.rtt())
        return;
    auto rtt = createCanonicalRTT(subtype);
    subtype.setRTT(WTF::move(rtt));
}

void TypeSectionState::registerCanonicalRTT(const Projection& projection)
{
    if (projection.rtt())
        return;
    auto rtt = createCanonicalRTT(projection);
    projection.setRTT(WTF::move(rtt));
}

Ref<const RTT> TypeSectionState::createCanonicalRTT(const Subtype& subtype)
{
    bool isFinalType = subtype.isFinalType();
    Ref expandedRTT = subtype.underlyingRTT();
    RTTKind kind = expandedRTT->kind();

    RefPtr<const RTT> superRTT = nullptr;
    if (subtype.supertypeCount() > 0) {
        TypeIndex parent = subtype.firstSuperType();
        if (isPlaceholderRef(parent))
            superRTT = untagProjection(parent)->rtt();
        else
            superRTT = std::bit_cast<const RTT*>(parent);
        ASSERT(superRTT);
    }

    RefPtr<RTT> protector;
    switch (kind) {
    case RTTKind::Function: {
        RTTFunctionPayload payload {
            expandedRTT->argumentCount(), expandedRTT->returnCount(),
            expandedRTT->functionPayload().signatureSpan(),
            expandedRTT->argumentsOrResultsIncludeI64(),
            expandedRTT->argumentsOrResultsIncludeV128(),
            expandedRTT->argumentsOrResultsIncludeExnref(),
            expandedRTT->hasRecursiveReference()
        };
        protector = superRTT
            ? RTT::tryCreateFunction(*superRTT, isFinalType, WTF::move(payload))
            : RTT::tryCreateFunction(isFinalType, WTF::move(payload));
        break;
    }
    case RTTKind::Struct: {
        RTTStructPayload payload {
            FixedVector<StructFieldEntry>(expandedRTT->structPayload().fieldsSpan()),
            expandedRTT->instancePayloadSize(),
            expandedRTT->hasRefFieldTypes(),
            expandedRTT->hasRecursiveReference()
        };
        protector = superRTT
            ? RTT::tryCreateStruct(*superRTT, isFinalType, WTF::move(payload))
            : RTT::tryCreateStruct(isFinalType, WTF::move(payload));
        break;
    }
    case RTTKind::Array: {
        RTTArrayPayload payload {
            expandedRTT->elementType(),
            expandedRTT->arrayPayload().elementTypeAnchor(),
            expandedRTT->hasRecursiveReference()
        };
        protector = superRTT
            ? RTT::tryCreateArray(*superRTT, isFinalType, WTF::move(payload))
            : RTT::tryCreateArray(isFinalType, WTF::move(payload));
        break;
    }
    }
    RELEASE_ASSERT(protector);
    return protector.releaseNonNull();
}

Ref<const RTT> TypeSectionState::createCanonicalRTT(const Projection& projection)
{
    ASSERT(!projection.isPlaceholder());
    const RecursionGroup* recursionGroup = projection.recursionGroup();
    TypeIndex memberIndex = recursionGroup->type(projection.projectionIndex());

    auto rebuildWithSubstitution = [&](const RTT& source) -> Ref<const RTT> {
        switch (source.kind()) {
        case RTTKind::Function: {
            SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE return TypeInformation::typeDefinitionForFunctionFromProviders(
                source.returnCount(),
                [&](size_t i) { return substitute(source.returnType(i), recursionGroup); },
                source.argumentCount(),
                [&](size_t i) { return substitute(source.argumentType(i), recursionGroup); });
        }
        case RTTKind::Struct: {
            SUPPRESS_UNCOUNTED_LAMBDA_CAPTURE return TypeInformation::typeDefinitionForStructFromProvider(
                source.fieldCount(),
                [&](size_t i) {
                    FieldType field = source.field(i);
                    StorageType substituted = field.type.is<PackedType>() ? field.type : StorageType(substitute(field.type.as<Type>(), recursionGroup));
                    return FieldType { substituted, field.mutability };
                });
        }
        case RTTKind::Array: {
            FieldType field = source.elementType();
            StorageType substituted = field.type.is<PackedType>() ? field.type : StorageType(substitute(field.type.as<Type>(), recursionGroup));
            return TypeInformation::typeDefinitionForArray(FieldType { substituted, field.mutability });
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    if (memberIndex & subtypeTagBit) {
        const Subtype* subtype = untagSubtype(memberIndex);
        if (subtype->hasRecursiveReference()) {
            // A subtype with recursive refs can carry them in two independent
            // places: the supertype chain (rewritten via substituteParent) and
            // the underlying structural shape (rewritten via
            // rebuildWithSubstitution). Rebuild both, then wrap in a fresh
            // Subtype -- the dedup set will coalesce identical rebuilds.
            Vector<TypeIndex> supertypes(subtype->supertypeCount(), [&](size_t i) {
                return substituteParent(subtype->superType(i), recursionGroup);
            });
            Ref<const RTT> newUnderlyingRTT = rebuildWithSubstitution(subtype->underlyingRTT());
            auto* unrolled = createSubtype(WTF::move(supertypes), WTF::move(newUnderlyingRTT), subtype->isFinalType());
            return createCanonicalRTT(*unrolled);
        }
        return createCanonicalRTT(*subtype);
    }
    RefPtr rtt = std::bit_cast<const RTT*>(memberIndex);
    if (rtt->hasRecursiveReference())
        return rebuildWithSubstitution(*rtt);
    return rtt.releaseNonNull();
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
