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
#include "WHLSLFunctionStageChecker.h"

#if ENABLE(WEBGPU)

#include "WHLSLCallExpression.h"
#include "WHLSLEntryPointType.h"
#include "WHLSLIntrinsics.h"
#include "WHLSLProgram.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

class FunctionStageChecker : public Visitor {
public:
    FunctionStageChecker(AST::EntryPointType entryPointType, const Intrinsics& intrinsics)
        : m_entryPointType(entryPointType)
        , m_intrinsics(intrinsics)
    {
    }

public:
    void visit(AST::CallExpression& callExpression) override
    {
        if ((&callExpression.function() == m_intrinsics.ddx() || &callExpression.function() == m_intrinsics.ddy()) && m_entryPointType != AST::EntryPointType::Fragment) {
            setError(Error("Cannot use ddx or ddy in a non-fragment shader.", callExpression.codeLocation()));
            return;
        }
        if ((&callExpression.function() == m_intrinsics.allMemoryBarrier() || &callExpression.function() == m_intrinsics.deviceMemoryBarrier() || &callExpression.function() == m_intrinsics.groupMemoryBarrier())
            && m_entryPointType != AST::EntryPointType::Compute) {
            setError(Error("Cannot use memory barrier in a non-compute shader.", callExpression.codeLocation()));
            return;
        }
        Visitor::visit(callExpression.function());
    }

    AST::EntryPointType m_entryPointType;
    const Intrinsics& m_intrinsics;
};

Expected<void, Error> checkFunctionStages(Program& program)
{
    for (auto& functionDefinition : program.functionDefinitions()) {
        if (!functionDefinition->entryPointType())
            continue;
        FunctionStageChecker functionStageChecker(*functionDefinition->entryPointType(), program.intrinsics());
        functionStageChecker.Visitor::visit(functionDefinition);
        if (functionStageChecker.hasError())
            return makeUnexpected(functionStageChecker.result().error());
    }
    return { };
}

}

}

#endif
