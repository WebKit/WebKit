/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLCodeLocation.h"
#include "WHLSLNamedType.h"
#include "WHLSLTypeArgument.h"
#include "WHLSLUnnamedType.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/UniqueRef.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class NamedType;

class TypeReference final : public UnnamedType {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(TypeReference);
    TypeReference(CodeLocation location, String&& name, TypeArguments&& typeArguments)
        : UnnamedType(location, Kind::TypeReference)
        , m_name(WTFMove(name))
        , m_typeArguments(WTFMove(typeArguments))
    {
    }
public:
    static Ref<TypeReference> create(CodeLocation location, String&& name, TypeArguments&& typeArguments)
    {
        return adoptRef(* new TypeReference(location, WTFMove(name), WTFMove(typeArguments)));
    }

    ~TypeReference() = default;

    static Ref<TypeReference> wrap(CodeLocation, NamedType& resolvedType);

    String& name() { return m_name; }
    TypeArguments& typeArguments() { return m_typeArguments; }
    NamedType* maybeResolvedType() const { return m_resolvedType; }
    NamedType& resolvedType() const
    {
        ASSERT(m_resolvedType);
        return *m_resolvedType;
    }

    void setResolvedType(NamedType& resolvedType)
    {
        m_resolvedType = &resolvedType;
    }

    unsigned hash() const
    {
        // Currently, we only use this function after the name resolver runs.
        // Relying on having a resolved type simplifies this implementation.
        ASSERT(m_resolvedType);
        return WTF::PtrHash<const Type*>::hash(&unifyNode());
    }

    bool operator==(const TypeReference& other) const
    {
        ASSERT(m_resolvedType);
        return &unifyNode() == &other.unifyNode();
    }

    String toString() const
    {
        ASSERT(m_resolvedType);
        return m_resolvedType->name();
    }

private:
    friend class Type;
    Type& unifyNodeImpl()
    {
        ASSERT(m_resolvedType);
        return m_resolvedType->unifyNode();
    }

    String m_name;
    TypeArguments m_typeArguments;
    NamedType* m_resolvedType { nullptr };
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(TypeReference)

SPECIALIZE_TYPE_TRAITS_WHLSL_TYPE(TypeReference, isTypeReference())

#endif // ENABLE(WHLSL_COMPILER)
