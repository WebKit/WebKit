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

#include "WHLSLExpression.h"
#include "WHLSLFunctionDeclaration.h"
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class NamedType;

class CallExpression final : public Expression {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CallExpression(CodeLocation location, String&& name, Vector<UniqueRef<Expression>>&& arguments)
        : Expression(location, Kind::Call)
        , m_name(WTFMove(name))
        , m_arguments(WTFMove(arguments))
    {
    }

    CallExpression(const CallExpression&) = delete;
    CallExpression(CallExpression&&) = default;

    Vector<UniqueRef<Expression>>& arguments() { return m_arguments; }

    String& name() { return m_name; }

    ~CallExpression() = default;

    void setCastData(NamedType& namedType)
    {
        m_castReturnType = &namedType;
    }

    bool isCast() { return m_castReturnType; }
    NamedType* castReturnType() { return m_castReturnType; }
    FunctionDeclaration& function()
    {
        ASSERT(m_function);
        return *m_function;
    }

    void setFunction(FunctionDeclaration& functionDeclaration)
    {
        ASSERT(!m_function);
        m_function = &functionDeclaration;
    }

private:
    String m_name;
    Vector<UniqueRef<Expression>> m_arguments;
    FunctionDeclaration* m_function { nullptr };
    NamedType* m_castReturnType { nullptr };
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(CallExpression)

SPECIALIZE_TYPE_TRAITS_WHLSL_EXPRESSION(CallExpression, isCallExpression())

#endif // ENABLE(WHLSL_COMPILER)
