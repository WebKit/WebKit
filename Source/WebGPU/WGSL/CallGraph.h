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
#include "ASTStageAttribute.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WGSL {

namespace AST {
class CallableExpression;
class ShaderModule;
class FunctionDecl;
};

class CallGraph {
    friend class CallGraphBuilder;

public:
    struct Callee {
        AST::FunctionDecl* m_target;
        Vector<AST::CallableExpression*> m_callSites;
    };

    struct EntryPoint {
        AST::FunctionDecl& m_function;
        AST::StageAttribute::Stage m_stage;
    };

    AST::ShaderModule& ast() { return m_ast; }
    const Vector<EntryPoint>& entrypoints() { return m_entrypoints; }

private:
    CallGraph(AST::ShaderModule&);

    AST::ShaderModule& m_ast;
    Vector<EntryPoint> m_entrypoints;
    HashMap<String, AST::FunctionDecl*> m_functionsByName;
    HashMap<AST::FunctionDecl*, Vector<Callee>> m_callees;
};

CallGraph buildCallGraph(AST::ShaderModule&);

} // namespace WGSL
