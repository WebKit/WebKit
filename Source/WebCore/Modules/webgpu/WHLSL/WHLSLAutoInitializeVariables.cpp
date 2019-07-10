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

#include "config.h"
#include "WHLSLAutoInitializeVariables.h"

#if ENABLE(WEBGPU)

#include "WHLSLAST.h"
#include "WHLSLASTDumper.h"
#include "WHLSLNameContext.h"
#include "WHLSLResolveOverloadImpl.h"
#include "WHLSLResolvingType.h"
#include "WHLSLVisitor.h"
#include <wtf/StringPrintStream.h>

namespace WebCore {

namespace WHLSL {

class AutoInitialize : public Visitor {
    using Base = Visitor;
public:
    AutoInitialize(NameContext& nameContext)
        : m_nameContext(nameContext)
        , m_castFunctions(*m_nameContext.getFunctions("operator cast"_str))
    { }

private:
    void visit(AST::FunctionDeclaration&)
    {
        // Skip argument declarations.
    }

    void visit(AST::VariableDeclaration& variableDeclaration)
    {
        if (variableDeclaration.initializer())
            return;

        AST::UnnamedType* type = variableDeclaration.type();
        RELEASE_ASSERT(type);

#ifndef NDEBUG
        StringPrintStream printStream;
        printStream.print(TypeDumper(*type));
        String functionName = printStream.toString();
#else
        String functionName = "<zero-init>"_s;
#endif
        auto callExpression = std::make_unique<AST::CallExpression>(variableDeclaration.codeLocation(), WTFMove(functionName), Vector<UniqueRef<AST::Expression>>());
        callExpression->setType(type->clone());
        callExpression->setTypeAnnotation(AST::RightValue());
        Vector<std::reference_wrapper<ResolvingType>> argumentTypes;
        auto* function = resolveFunctionOverload(m_castFunctions, argumentTypes, type);
        if (!function) {
            setError();
            return;
        }
        callExpression->setFunction(*function);

        variableDeclaration.setInitializer(WTFMove(callExpression));
    }

    NameContext& m_nameContext;
    Vector<std::reference_wrapper<AST::FunctionDeclaration>, 1>& m_castFunctions;
};

bool autoInitializeVariables(Program& program)
{
    AutoInitialize autoInitialize(program.nameContext());
    autoInitialize.Visitor::visit(program);
    return !autoInitialize.error();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
