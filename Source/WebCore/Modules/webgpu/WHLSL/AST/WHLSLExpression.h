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

#if ENABLE(WEBGPU)

#include "WHLSLAddressSpace.h"
#include "WHLSLCodeLocation.h"
#include "WHLSLDefaultDelete.h"
#include "WHLSLUnnamedType.h"
#include <wtf/FastMalloc.h>
#include <wtf/Optional.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class Expression {
    WTF_MAKE_FAST_ALLOCATED;

protected:
    ~Expression() = default;

public:

    enum class Kind : uint8_t {
        Assignment,
        BooleanLiteral,
        Call,
        Comma,
        Dereference,
        Dot,
        GlobalVariableReference,
        FloatLiteral,
        Index,
        IntegerLiteral,
        Logical,
        LogicalNot,
        MakeArrayReference,
        MakePointer,
        ReadModifyWrite,
        Ternary,
        UnsignedIntegerLiteral,
        VariableReference,
        EnumerationMemberLiteral,
    };

    Expression(CodeLocation codeLocation, Kind kind)
        : m_codeLocation(codeLocation)
        , m_kind(kind)
    {
    }

    static void destroy(Expression&);
    static void destruct(Expression&);

    Expression(const Expression&) = delete;
    Expression(Expression&&) = default;

    Expression& operator=(const Expression&) = delete;
    Expression& operator=(Expression&&) = default;

    UnnamedType* maybeResolvedType() { return m_type ? &*m_type : nullptr; }

    UnnamedType& resolvedType()
    {
        ASSERT(m_type);
        return *m_type;
    }

    void setType(Ref<UnnamedType> type)
    {
        ASSERT(!m_type);
        m_type = WTFMove(type);
    }

    const TypeAnnotation* maybeTypeAnnotation() const { return m_typeAnnotation ? &*m_typeAnnotation : nullptr; }

    const TypeAnnotation& typeAnnotation() const
    {
        ASSERT(m_typeAnnotation);
        return *m_typeAnnotation;
    }

    void setTypeAnnotation(TypeAnnotation&& typeAnnotation)
    {
        ASSERT(!m_typeAnnotation);
        m_typeAnnotation = WTFMove(typeAnnotation);
    }

    void copyTypeTo(Expression& other) const
    {
        if (auto* resolvedType = const_cast<Expression*>(this)->maybeResolvedType())
            other.setType(*resolvedType);
    }

    bool mayBeEffectful() const;

    Kind kind() const  { return m_kind; }
    bool isAssignmentExpression() const { return kind() == Kind::Assignment; }
    bool isBooleanLiteral() const { return kind() == Kind::BooleanLiteral; }
    bool isCallExpression() const { return kind() == Kind::Call; }
    bool isCommaExpression() const { return kind() == Kind::Comma; }
    bool isDereferenceExpression() const { return kind() == Kind::Dereference; }
    bool isDotExpression() const { return kind() == Kind::Dot; }
    bool isGlobalVariableReference() const { return kind() == Kind::GlobalVariableReference; }
    bool isFloatLiteral() const { return kind() == Kind::FloatLiteral; }
    bool isIndexExpression() const { return kind() == Kind::Index; }
    bool isIntegerLiteral() const { return kind() == Kind::IntegerLiteral; }
    bool isLogicalExpression() const { return kind() == Kind::Logical; }
    bool isLogicalNotExpression() const { return kind() == Kind::LogicalNot; }
    bool isMakeArrayReferenceExpression() const { return kind() == Kind::MakeArrayReference; }
    bool isMakePointerExpression() const { return kind() == Kind::MakePointer; }
    bool isPropertyAccessExpression() const { return isDotExpression() || isIndexExpression(); }
    bool isReadModifyWriteExpression() const { return kind() == Kind::ReadModifyWrite; }
    bool isTernaryExpression() const { return kind() == Kind::Ternary; }
    bool isUnsignedIntegerLiteral() const { return kind() == Kind::UnsignedIntegerLiteral; }
    bool isVariableReference() const { return kind() == Kind::VariableReference; }
    bool isEnumerationMemberLiteral() const { return kind() == Kind::EnumerationMemberLiteral; }

    CodeLocation codeLocation() const { return m_codeLocation; }
    void updateCodeLocation(CodeLocation location) { m_codeLocation = location; }

private:
    CodeLocation m_codeLocation;
    RefPtr<UnnamedType> m_type;
    Optional<TypeAnnotation> m_typeAnnotation;
    Kind m_kind;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(Expression)

#define SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WHLSL::AST::ToValueTypeName) \
    static bool isType(const WebCore::WHLSL::AST::Expression& expression) { return expression.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
