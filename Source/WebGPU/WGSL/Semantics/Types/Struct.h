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

#pragma once

#include "Type.h"

#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>

namespace WGSL::Semantics::Types {

class StructMember {
    static Expected<StructMember, Error> tryCreate(SourceSpan, UniqueRef<Type>);

    SourceSpan span() const { return m_span; }
    const Type& type() const { return m_type; }

private:
    StructMember(SourceSpan, UniqueRef<Type>);

    // TODO: member attributes.
    SourceSpan m_span;
    UniqueRef<Type> m_type;
};

class Struct : public Type {
public:
    static UniqueRef<Type> create(SourceSpan, String name, Vector<StructMember>&& m_members);

    bool operator==(const Type &other) const override;
    String string() const override;

    const String& name() const { return m_name; }
    const Vector<StructMember>& members() const { return m_members; }

private:
    Struct(SourceSpan, String name, Vector<StructMember>&& members);

    String m_name;
    Vector<StructMember> m_members;
};

} // namespace WGSL::Semantics::Types

SPECIALIZE_TYPE_TRAITS_WGSL_SEMANTICS_TYPES(Struct, isStruct())
