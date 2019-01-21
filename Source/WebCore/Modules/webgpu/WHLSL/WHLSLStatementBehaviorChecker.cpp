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
#include "WHLSLStatementBehaviorChecker.h"

#if ENABLE(WEBGPU)

#include "WHLSLDoWhileLoop.h"
#include "WHLSLForLoop.h"
#include "WHLSLIfStatement.h"
#include "WHLSLInferTypes.h"
#include "WHLSLProgram.h"
#include "WHLSLSwitchStatement.h"
#include "WHLSLWhileLoop.h"
#include <cstdint>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace WHLSL {

class StatementBehaviorChecker : public Visitor {
public:
    enum class Behavior : uint8_t {
        Return = 1 << 0,
        Break = 1 << 1,
        Continue = 1 << 2,
        Fallthrough = 1 << 3,
        Nothing = 1 << 4
    };

    OptionSet<Behavior> takeFunctionBehavior()
    {
        ASSERT(m_stack.size() == 1);
        return m_stack.takeLast();
    }

private:
    void visit(AST::Break&) override
    {
        m_stack.append({ Behavior::Break });
    }

    void visit(AST::Fallthrough&) override
    {
        m_stack.append({ Behavior::Fallthrough });
    }

    void visit(AST::Continue&) override
    {
        m_stack.append({ Behavior::Continue });
    }

    void visit(AST::Return&) override
    {
        m_stack.append({ Behavior::Return });
    }

    void visit(AST::Trap&) override
    {
        m_stack.append({ Behavior::Return });
    }

    void visit(AST::DoWhileLoop& doWhileLoop) override
    {
        checkErrorAndVisit(doWhileLoop.body());
        if (error())
            return;
        auto b = m_stack.takeLast();
        b.remove(Behavior::Break);
        b.remove(Behavior::Continue);
        b.add(Behavior::Nothing);
        m_stack.append(b);
    }

    void visit(AST::ForLoop& forLoop) override
    {
        // The initialization always has a behavior of Nothing, which we already add unconditionally below,
        // so we can just ignore the initialization.

        checkErrorAndVisit(forLoop.body());
        if (error())
            return;
        auto b = m_stack.takeLast();
        b.remove(Behavior::Break);
        b.remove(Behavior::Continue);
        b.add(Behavior::Nothing);
        m_stack.append(b);
    }

    void visit(AST::WhileLoop& whileLoop) override
    {
        checkErrorAndVisit(whileLoop.body());
        if (error())
            return;
        auto b = m_stack.takeLast();
        b.remove(Behavior::Break);
        b.remove(Behavior::Continue);
        b.add(Behavior::Nothing);
        m_stack.append(b);
    }

    void visit(AST::SwitchCase& switchCase) override
    {
        Visitor::visit(switchCase);
    }

    void visit(AST::SwitchStatement& switchStatement) override
    {
        if (switchStatement.switchCases().isEmpty()) {
            m_stack.append({ Behavior::Nothing });
            return;
        }

        OptionSet<Behavior> reduction = { };
        for (auto& switchCase : switchStatement.switchCases()) {
            checkErrorAndVisit(switchCase);
            if (error())
                return;
            auto b = m_stack.takeLast();
            reduction = reduction | b;
        }
        if (reduction.contains(Behavior::Nothing)) {
            setError();
            return;
        }
        reduction.remove(Behavior::Break);
        reduction.remove(Behavior::Fallthrough);
        reduction.add(Behavior::Nothing);
        m_stack.append(reduction);
    }

    void visit(AST::IfStatement& ifStatement) override
    {
        checkErrorAndVisit(ifStatement.body());
        if (error())
            return;
        auto b = m_stack.takeLast();
        OptionSet<Behavior> bPrime;
        if (ifStatement.elseBody()) {
            checkErrorAndVisit(*ifStatement.elseBody());
            if (error())
                return;
            bPrime = m_stack.takeLast();
        } else
            bPrime = { Behavior::Nothing };
        m_stack.append(b | bPrime);
    }

    void visit(AST::EffectfulExpressionStatement&) override
    {
        m_stack.append({ Behavior::Nothing });
    }

    void visit(AST::Block& block) override
    {
        if (block.statements().isEmpty()) {
            m_stack.append({ Behavior::Nothing });
            return;
        }

        OptionSet<Behavior> reduction = { };
        for (size_t i = 0; i < block.statements().size() - 1; ++i) {
            checkErrorAndVisit(block.statements()[i]);
            if (error())
                return;
            auto b = m_stack.takeLast();
            if (!b.contains(Behavior::Nothing)) {
                setError();
                return;
            }
            b.remove(Behavior::Nothing);
            if (b.contains(Behavior::Fallthrough)) {
                setError();
                return;
            }
            reduction = reduction | b;
        }
        checkErrorAndVisit(block.statements()[block.statements().size() - 1]);
        if (error())
            return;
        auto b = m_stack.takeLast();
        m_stack.append(reduction | b);
    }

    void visit(AST::VariableDeclarationsStatement&) override
    {
        m_stack.append({ Behavior::Nothing });
    }

    Vector<OptionSet<Behavior>> m_stack;
};

bool checkStatementBehavior(Program& program)
{
    StatementBehaviorChecker statementBehaviorChecker;
    for (auto& functionDefinition : program.functionDefinitions()) {
        statementBehaviorChecker.Visitor::visit(functionDefinition);
        if (statementBehaviorChecker.error())
            return false;
        auto behavior = statementBehaviorChecker.takeFunctionBehavior();
        if (matches(functionDefinition->type(), program.intrinsics().voidType())) {
            behavior.remove(StatementBehaviorChecker::Behavior::Return);
            behavior.remove(StatementBehaviorChecker::Behavior::Nothing);
            if (behavior != OptionSet<StatementBehaviorChecker::Behavior>())
                return false;
        } else {
            if (behavior != StatementBehaviorChecker::Behavior::Return)
                return false;
        }
    }
    return true;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
