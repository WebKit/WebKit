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
#include "WHLSLPruneUnreachableStandardLibraryFunctions.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLAST.h"
#include "WHLSLProgram.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

class ReachableStdlibFunctions final : public Visitor {
public:
    void visit(AST::FunctionDefinition& function) override
    {
        auto addResult = m_reachableFunctions.add(&function);
        if (addResult.isNewEntry)
            Visitor::visit(function);
    }

    void visit(AST::CallExpression& callExpression) override
    {
        Visitor::visit(callExpression);
        if (is<AST::FunctionDefinition>(callExpression.function()))
            visit(downcast<AST::FunctionDefinition>(callExpression.function()));
    }

    HashSet<AST::FunctionDefinition*> takeReachableFunctions() { return WTFMove(m_reachableFunctions); }

private:
    HashSet<AST::FunctionDefinition*> m_reachableFunctions;
};

void pruneUnreachableStandardLibraryFunctions(Program& program)
{
    ReachableStdlibFunctions reachableStdlibFunctions;
    Vector<UniqueRef<AST::FunctionDefinition>> functionDefinitions = WTFMove(program.functionDefinitions());
    for (auto& function : functionDefinitions) {
        if (function->parsingMode() != ParsingMode::StandardLibrary)
            reachableStdlibFunctions.visit(function.get());
        else
            RELEASE_ASSERT(!function->entryPointType());
    }

    auto reachableFunctions = reachableStdlibFunctions.takeReachableFunctions();
    Vector<UniqueRef<AST::FunctionDefinition>> newFunctionDefinitions;
    for (UniqueRef<AST::FunctionDefinition>& entry : functionDefinitions) {
        if (reachableFunctions.contains(&entry.get()))
            newFunctionDefinitions.append(WTFMove(entry));
    }

    program.functionDefinitions() = WTFMove(newFunctionDefinitions);
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
