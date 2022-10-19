/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include "ASTForward.h"
#include "CompilationMessage.h"

#include <wtf/Expected.h>

namespace WGSL::AST {

class Visitor {
public:
    virtual ~Visitor() = default;

    // Shader Module
    virtual void visit(AST::ShaderModule&);
    virtual void visit(AST::GlobalDirective&);

    // Attribute
    virtual void visit(AST::Attribute&);
    virtual void visit(AST::BindingAttribute&);
    virtual void visit(AST::BuiltinAttribute&);
    virtual void visit(AST::GroupAttribute&);
    virtual void visit(AST::LocationAttribute&);
    virtual void visit(AST::StageAttribute&);

    // Declaration
    virtual void visit(AST::Decl&);
    virtual void visit(AST::FunctionDecl&);
    virtual void visit(AST::StructDecl&);
    virtual void visit(AST::VariableDecl&);

    // Expression
    virtual void visit(AST::Expression&);
    virtual void visit(AST::AbstractFloatLiteral&);
    virtual void visit(AST::AbstractIntLiteral&);
    virtual void visit(AST::ArrayAccess&);
    virtual void visit(AST::BoolLiteral&);
    virtual void visit(AST::CallableExpression&);
    virtual void visit(AST::Float32Literal&);
    virtual void visit(AST::IdentifierExpression&);
    virtual void visit(AST::Int32Literal&);
    virtual void visit(AST::StructureAccess&);
    virtual void visit(AST::Uint32Literal&);
    virtual void visit(AST::UnaryExpression&);

    // Statement
    virtual void visit(AST::Statement&);
    virtual void visit(AST::AssignmentStatement&);
    virtual void visit(AST::CompoundStatement&);
    virtual void visit(AST::ReturnStatement&);
    virtual void visit(AST::VariableStatement&);

    // Types
    virtual void visit(AST::TypeDecl&);
    virtual void visit(AST::ArrayType&);
    virtual void visit(AST::NamedType&);
    virtual void visit(AST::ParameterizedType&);

    virtual void visit(AST::Parameter&);
    virtual void visit(AST::StructMember&);
    virtual void visit(AST::VariableQualifier&);
    
    bool hasError() const;
    Expected<void, Error> result();

    template<typename T> void checkErrorAndVisit(T&);
    template<typename T> void maybeCheckErrorAndVisit(T*);

protected:
    void setError(Error error)
    {
        ASSERT(!hasError());
        m_expectedError = makeUnexpected(error);
    }

private:
    Expected<void, Error> m_expectedError;
};

} // namespace WGSL::AST
