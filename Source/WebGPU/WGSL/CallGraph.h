/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

// FIXME: move Stage out of StageAttribute so we don't need to include this
#include "ASTForward.h"
#include "ASTStageAttribute.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WGSL {

class ShaderModule;
struct PipelineLayout;
struct PrepareResult;

namespace Reflection {
struct EntryPointInformation;
}

class CallGraph {
    friend class CallGraphBuilder;

public:
    struct Callee {
        AST::Function* target;
        Vector<AST::CallExpression*> callSites;
    };

    struct EntryPoint {
        AST::Function& function;
        ShaderStage stage;
        Reflection::EntryPointInformation& information;
    };

    ShaderModule& ast() const { return m_ast; }
    const Vector<EntryPoint>& entrypoints() const { return m_entrypoints; }
    const Vector<Callee>& callees(AST::Function& function) const { return m_calleeMap.find(&function)->value; }

private:
    CallGraph(ShaderModule&);

    ShaderModule& m_ast;
    Vector<EntryPoint> m_entrypoints;
    HashMap<String, AST::Function*> m_functionsByName;
    HashMap<AST::Function*, Vector<Callee>> m_calleeMap;
};

CallGraph buildCallGraph(ShaderModule&, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts, HashMap<String, Reflection::EntryPointInformation>&);

} // namespace WGSL
