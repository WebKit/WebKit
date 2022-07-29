/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "Struct.h"

namespace WGSL::Semantics::Types {

static std::optional<Error> checkStructMemberType(SourceSpan span, const Type& type)
{
    if (type.isScalar() || type.isVector() || type.isMatrix() || type.isAtomic())
        return { };

    // TODO: support runtime-sized array
    if (type.isArray()) {
        if (!type.hasCreationFixedFootprint()) {
            auto err = makeString(
                "struct member type '",
                type.string(),
                "' is an array, but does not have creation-fixed footprint"
            );

            return Error { WTFMove(err), type.span() };
        }

        return { };
    }

    if (type.isStruct()) {
        if (!type.hasCreationFixedFootprint()) {
            auto err = makeString(
                "struct member type '",
                type.string(),
                "' is a struct, but does not have creation-fixed footprint"
            );

            return Error { WTFMove(err), type.span() };
        }

        return { };
    }

    auto err = makeString(
        "'",
        type.string(),
        "' can't be used as struct member type"
    );

    return Error { WTFMove(err), span };
}

StructMember::StructMember(SourceSpan span, UniqueRef<Type> type)
    : m_span(span)
    , m_type(WTFMove(type))
{
}

Expected<StructMember, Error> StructMember::tryCreate(SourceSpan span, UniqueRef<Type> type)
{
    if (auto err = checkStructMemberType(span, type))
        return makeUnexpected(*err);

    return StructMember { span, WTFMove(type) };
}

Struct::Struct(SourceSpan span, String name, Vector<StructMember>&& members)
    : Type(Type::Kind::Struct, span)
    , m_name(WTFMove(name))
    , m_members(WTFMove(members))
{
}

UniqueRef<Type> Struct::create(SourceSpan span, String name, Vector<StructMember>&& members)
{
    // return makeUniqueRef<Struct>(span, WTFMove(name), WTFMove(members));
    return { UniqueRef(*new Struct(span, WTFMove(name), WTFMove(members))) };
}

bool Struct::operator==(const Type &other) const
{
    if (!other.isStruct())
        return false;

    const auto& structOther = downcast<Struct>(other);

    // TODO: assume that all structs have unique name, so comparing
    // name is enough. Check if assumption is true.
    return m_name == structOther.m_name;
}

String Struct::string() const
{
    return makeString("struct ", m_name);
}

} // namespace WGSL::Semantics::Types
