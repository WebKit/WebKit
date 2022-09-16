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
#include "Declaration.h"
#include "FunctionDeclaration.h"
#include "GlobalDirective.h"
#include "StructDeclaration.h"
#include "VariableDeclaration.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace WGSL::AST {

class ShaderModule final : ASTNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShaderModule(SourceSpan span, Vector<UniqueRef<GlobalDirective>>&& directives, Vector<UniqueRef<Declaration>>&& decls)
        : ASTNode(span)
        , m_directives(WTFMove(directives))
    {
        for (auto& decl : decls) {
            if (is<StructDeclaration>(decl.get())) {
                // We want to go from UniqueRef<BaseClass> to UniqueRef<DerivedClass>, but that is not supported by downcast.
                // So instead we do UniqueRef -> unique_ptr -> raw_pointer -(downcast)-> raw_pointer -> unique_ptr -> UniqueRef...
                Declaration* rawPtr = decl.moveToUniquePtr().release();
                StructDeclaration* downcastedRawPtr = downcast<StructDeclaration>(rawPtr);
                m_structs.append(makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<StructDeclaration>(downcastedRawPtr)));
            } else if (is<VariableDeclaration>(decl.get())) {
                Declaration* rawPtr = decl.moveToUniquePtr().release();
                VariableDeclaration* downcastedRawPtr = downcast<VariableDeclaration>(rawPtr);
                m_globalVars.append(makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<VariableDeclaration>(downcastedRawPtr)));
            } else {
                Declaration* rawPtr = decl.moveToUniquePtr().release();
                FunctionDeclaration* func = downcast<FunctionDeclaration>(rawPtr);
                m_functions.append(makeUniqueRefFromNonNullUniquePtr(std::unique_ptr<FunctionDeclaration>(func)));
            }
        }
    }

    Vector<UniqueRef<GlobalDirective>>& directives() { return m_directives; }
    Vector<UniqueRef<StructDeclaration>>& structs() { return m_structs; }
    Vector<UniqueRef<VariableDeclaration>>& globalVars() { return m_globalVars; }
    Vector<UniqueRef<FunctionDeclaration>>& functions() { return m_functions; }

private:
    Vector<UniqueRef<GlobalDirective>> m_directives;

    Vector<UniqueRef<StructDeclaration>> m_structs;
    Vector<UniqueRef<VariableDeclaration>> m_globalVars;
    Vector<UniqueRef<FunctionDeclaration>> m_functions;
};

} // namespace WGSL::AST
