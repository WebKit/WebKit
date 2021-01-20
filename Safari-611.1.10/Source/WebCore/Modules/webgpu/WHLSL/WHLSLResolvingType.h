/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "WHLSLUnnamedType.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/UniqueRef.h>
#include <wtf/Variant.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class ResolvableType;

}

// There are cases where the type of one AST node should match the type of another AST node.
// One example of this is the comma expression - the type of the comma expression should match the type of its last item.
// If this type happens to be a resolvable type, it will get resolved later. When that happens,
// both of the AST nodes have to be resolved to the result type. This class represents a
// reference counted wrapper around a resolvable type so both entries in the type map can point
// to the same resolvable type, so resolving it once resolves both the entries in the map.
// This class could probably be represented as
// "class ResolvableTypeReference : public std::reference_wrapper<AST::ResolvableType>, public RefCounted<ResolvableTypeReference {}"
// but I didn't want to be too clever.
class ResolvableTypeReference : public RefCounted<ResolvableTypeReference> {
public:
    ResolvableTypeReference(AST::ResolvableType& resolvableType)
        : m_resolvableType(&resolvableType)
    {
    }

    ResolvableTypeReference(const ResolvableTypeReference&) = delete;
    ResolvableTypeReference(ResolvableTypeReference&&) = delete;
    ResolvableTypeReference& operator=(const ResolvableTypeReference&) = delete;
    ResolvableTypeReference& operator=(ResolvableTypeReference&&) = delete;

    AST::ResolvableType& resolvableType() { return *m_resolvableType; }

private:
    AST::ResolvableType* m_resolvableType;
};

// This is a thin wrapper around a Variant.
// It exists so we can make sure that the default constructor does the right thing.
// FIXME: https://bugs.webkit.org/show_bug.cgi?id=198158 This wrapper might not be necessary.
class ResolvingType {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ResolvingType()
        : m_inner(RefPtr<ResolvableTypeReference>())
    {
    }

    ResolvingType(Ref<AST::UnnamedType> v)
        : m_inner(WTFMove(v))
    {
    }

    ResolvingType(RefPtr<ResolvableTypeReference> v)
        : m_inner(WTFMove(v))
    {
    }

    ResolvingType(const ResolvingType&) = delete;
    ResolvingType(ResolvingType&& other)
        : m_inner(WTFMove(other.m_inner))
    {
    }

    ResolvingType& operator=(const ResolvingType&) = delete;
    ResolvingType& operator=(ResolvingType&& other)
    {
        m_inner = WTFMove(other.m_inner);
        return *this;
    }

    AST::UnnamedType* getUnnamedType()
    {
        if (WTF::holds_alternative<Ref<AST::UnnamedType>>(m_inner))
            return WTF::get<Ref<AST::UnnamedType>>(m_inner).ptr();
        return nullptr;
    }

    template <typename Visitor> auto visit(const Visitor& visitor) -> decltype(WTF::visit(visitor, std::declval<Variant<Ref<AST::UnnamedType>, RefPtr<ResolvableTypeReference>>&>()))
    {
        return WTF::visit(visitor, m_inner);
    }

private:
    Variant<Ref<AST::UnnamedType>, RefPtr<ResolvableTypeReference>> m_inner;
};

}

}

#endif // ENABLE(WHLSL_COMPILER)
