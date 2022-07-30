/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#include "ASTNode.h"
#include "FunctionDecl.h"
#include "GlobalDecl.h"
#include "GlobalDirective.h"
#include "StructureDecl.h"
#include "VariableDecl.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WGSL::AST {

class ShaderModule final : ASTNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShaderModule(SourceSpan span,
                 Vector<GlobalDirective>&& directives,
                 Vector<StructDecl>&& structs,
                 Vector<VariableDecl>&& globalVars,
                 Vector<FunctionDecl>&& functions)
        : ASTNode(span)
        , m_directives(WTFMove(directives))
        , m_structs(WTFMove(structs))
        , m_globalVars(WTFMove(globalVars))
        , m_functions(WTFMove(functions))
    {
    }

    Vector<GlobalDirective>& directives() { return m_directives; }
    Vector<StructDecl>& structs() { return m_structs; }
    Vector<VariableDecl>& globalVars() { return m_globalVars; }
    Vector<FunctionDecl>& functions() { return m_functions; }

private:
    Vector<GlobalDirective> m_directives;

    Vector<StructDecl> m_structs;
    Vector<VariableDecl> m_globalVars;
    Vector<FunctionDecl> m_functions;
};

} // namespace WGSL::AST
