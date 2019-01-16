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
#include "WHLSLLoopChecker.h"

#if ENABLE(WEBGPU)

#include "WHLSLBreak.h"
#include "WHLSLContinue.h"
#include "WHLSLDoWhileLoop.h"
#include "WHLSLFallthrough.h"
#include "WHLSLForLoop.h"
#include "WHLSLSwitchStatement.h"
#include "WHLSLVisitor.h"
#include "WHLSLWhileLoop.h"
#include <wtf/SetForScope.h>

namespace WebCore {

namespace WHLSL {

// This makes sure that every occurance of "continue," "break," and "fallthrough" appear within the relevant control flow structure.
class LoopChecker : public Visitor {
public:
    LoopChecker() = default;

    virtual ~LoopChecker() = default;

private:
    void visit(AST::FunctionDefinition& functionDefinition) override
    {
        ASSERT(!m_loopDepth);
        ASSERT(!m_switchDepth);
        Visitor::visit(functionDefinition);
    }

    void visit(AST::WhileLoop& whileLoop) override
    {
        checkErrorAndVisit(whileLoop.conditional());

        SetForScope<unsigned> change(m_loopDepth, m_loopDepth + 1);
        checkErrorAndVisit(whileLoop.body());
        ASSERT(m_loopDepth);
    }

    void visit(AST::DoWhileLoop& doWhileLoop) override
    {
        {
            SetForScope<unsigned> change(m_loopDepth, m_loopDepth + 1);
            checkErrorAndVisit(doWhileLoop.body());
            ASSERT(m_loopDepth);
        }

        checkErrorAndVisit(doWhileLoop.conditional());
    }

    void visit(AST::ForLoop& forLoop) override
    {
        WTF::visit(WTF::makeVisitor([&](AST::VariableDeclarationsStatement& variableDeclarationsStatement) {
            checkErrorAndVisit(variableDeclarationsStatement);
        }, [&](UniqueRef<AST::Expression>& expression) {
            checkErrorAndVisit(expression);
        }), forLoop.initialization());
        if (forLoop.condition())
            checkErrorAndVisit(*forLoop.condition());
        if (forLoop.increment())
            checkErrorAndVisit(*forLoop.increment());

        SetForScope<unsigned> change(m_loopDepth, m_loopDepth + 1);
        checkErrorAndVisit(forLoop.body());
        ASSERT(m_loopDepth);
    }

    void visit(AST::SwitchStatement& switchStatement) override
    {
        checkErrorAndVisit(switchStatement.value());

        SetForScope<unsigned> change(m_switchDepth, m_switchDepth + 1);
        for (auto& switchCase : switchStatement.switchCases())
            checkErrorAndVisit(switchCase);
        ASSERT(m_switchDepth);
    }

    void visit(AST::Break& breakStatement) override
    {
        if (!m_loopDepth && !m_switchDepth) {
            setError();
            return;
        }
        Visitor::visit(breakStatement);
    }

    void visit(AST::Continue& continueStatement) override
    {
        if (!m_loopDepth) {
            setError();
            return;
        }
        Visitor::visit(continueStatement);
    }

    void visit(AST::Fallthrough& fallthroughStatement) override
    {
        if (!m_switchDepth) {
            setError();
            return;
        }
        Visitor::visit(fallthroughStatement);
    }

    unsigned m_loopDepth { 0 };
    unsigned m_switchDepth { 0 };
};

bool checkLoops(Program& program)
{
    LoopChecker loopChecker;
    loopChecker.Visitor::visit(program);
    return !loopChecker.error();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
