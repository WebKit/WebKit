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
#include "WHLSLRecursionChecker.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLCallExpression.h"
#include "WHLSLFunctionDefinition.h"
#include "WHLSLProgram.h"
#include "WHLSLVisitor.h"
#include <wtf/HashSet.h>

namespace WebCore {

namespace WHLSL {

// Makes sure there is no function recursion.
class RecursionChecker final : public Visitor {
private:
    void visit(Program& program) override
    {
        for (auto& functionDefinition : program.functionDefinitions())
            checkErrorAndVisit(functionDefinition);
    }

    void visit(AST::FunctionDefinition& functionDefinition) override
    {
        if (m_finishedVisiting.contains(&functionDefinition))
            return;

        auto addResult = m_startedVisiting.add(&functionDefinition);
        if (!addResult.isNewEntry) {
            setError(Error("Cannot use recursion in the call graph.", functionDefinition.codeLocation()));
            return;
        }

        if (functionDefinition.parsingMode() != ParsingMode::StandardLibrary)
            Visitor::visit(functionDefinition);

        {
            auto addResult = m_finishedVisiting.add(&functionDefinition);
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }
    }

    void visit(AST::CallExpression& callExpression) override
    {
        Visitor::visit(callExpression);
        if (is<AST::FunctionDefinition>(callExpression.function()))
            checkErrorAndVisit(downcast<AST::FunctionDefinition>(callExpression.function()));
    }

    HashSet<AST::FunctionDefinition*> m_startedVisiting;
    HashSet<AST::FunctionDefinition*> m_finishedVisiting;
};

Expected<void, Error> checkRecursion(Program& program)
{
    RecursionChecker recursionChecker;
    recursionChecker.Visitor::visit(program);
    return recursionChecker.result();
}

}

}

#endif // ENABLE(WHLSL_COMPILER)
