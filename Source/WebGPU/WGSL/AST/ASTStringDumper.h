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

#include "ASTVisitor.h"
#include <wtf/StringPrintStream.h>

namespace WGSL::AST {

class StringDumper final : public Visitor {
    friend struct Indent;
public:
    using Visitor::visit;

    ~StringDumper() = default;

    String toString();

    // Visitor
    void visit(ShaderModule&) override;
    void visit(GlobalDirective&) override;

    // Attribute
    void visit(BindingAttribute&) override;
    void visit(BuiltinAttribute&) override;
    void visit(GroupAttribute&) override;
    void visit(LocationAttribute&) override;
    void visit(StageAttribute&) override;

    // Declaration
    void visit(FunctionDecl&) override;
    void visit(StructDecl&) override;
    void visit(VariableDecl&) override;

    // Expression
    void visit(AbstractFloatLiteral&) override;
    void visit(AbstractIntLiteral&) override;
    void visit(ArrayAccess&) override;
    void visit(BoolLiteral&) override;
    void visit(CallableExpression&) override;
    void visit(Float32Literal&) override;
    void visit(IdentifierExpression&) override;
    void visit(Int32Literal&) override;
    void visit(StructureAccess&) override;
    void visit(Uint32Literal&) override;
    void visit(UnaryExpression&) override;

    // Statement
    void visit(AssignmentStatement&) override;
    void visit(CompoundStatement&) override;
    void visit(ReturnStatement&) override;
    void visit(VariableStatement&) override;

    // Types
    void visit(ArrayType&) override;
    void visit(NamedType&) override;
    void visit(ParameterizedType&) override;

    void visit(Parameter&) override;
    void visit(StructMember&) override;
    void visit(VariableQualifier&) override;

private:

    template<typename T, typename J>
    void visitVector(T&, J);

    StringPrintStream m_out;
    String m_indent;
};

template<typename T> void dumpNode(PrintStream&, T&);

MAKE_PRINT_ADAPTOR(ShaderModuleDumper, ShaderModule&, dumpNode);

void dumpAST(ShaderModule&);

} // namespace WGSL::AST
