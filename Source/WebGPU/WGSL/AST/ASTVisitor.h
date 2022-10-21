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
    virtual void visit(ShaderModule&);
    virtual void visit(GlobalDirective&);

    // Attribute
    virtual void visit(Attribute&);
    virtual void visit(BindingAttribute&);
    virtual void visit(BuiltinAttribute&);
    virtual void visit(GroupAttribute&);
    virtual void visit(LocationAttribute&);
    virtual void visit(StageAttribute&);

    // Declaration
    virtual void visit(Decl&);
    virtual void visit(FunctionDecl&);
    virtual void visit(StructDecl&);
    virtual void visit(VariableDecl&);

    // Expression
    virtual void visit(Expression&);
    virtual void visit(AbstractFloatLiteral&);
    virtual void visit(AbstractIntLiteral&);
    virtual void visit(ArrayAccess&);
    virtual void visit(BoolLiteral&);
    virtual void visit(CallableExpression&);
    virtual void visit(Float32Literal&);
    virtual void visit(IdentifierExpression&);
    virtual void visit(Int32Literal&);
    virtual void visit(StructureAccess&);
    virtual void visit(Uint32Literal&);
    virtual void visit(UnaryExpression&);

    // Statement
    virtual void visit(Statement&);
    virtual void visit(AssignmentStatement&);
    virtual void visit(CompoundStatement&);
    virtual void visit(ReturnStatement&);
    virtual void visit(VariableStatement&);

    // Types
    virtual void visit(TypeDecl&);
    virtual void visit(ArrayType&);
    virtual void visit(NamedType&);
    virtual void visit(ParameterizedType&);

    virtual void visit(Parameter&);
    virtual void visit(StructMember&);
    virtual void visit(VariableQualifier&);
    
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
