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

#include "config.h"
#include "Writer.h"

namespace WGSL::Writer {

void MetalWriter::writeStageAttribute(const AST::StageAttribute& attribute)
{
    switch (attribute.stage()) {
    case AST::StageAttribute::Stage::Compute:
        m_msl.append("kernel");
        return;

    case AST::StageAttribute::Stage::Vertex:
        m_msl.append("vertex");
        return;

    case AST::StageAttribute::Stage::Fragment:
        m_msl.append("fragment");
        return;
    }
}

void MetalWriter::writeAttribute(const AST::Attribute& attribute)
{
    if (attribute.isStage()) {
        writeStageAttribute(downcast<AST::StageAttribute>(attribute));
        return;
    }

    ASSERT_NOT_REACHED();
}

void MetalWriter::writeAttributes(const AST::Attributes& attributes)
{
    for (const auto& attribute : attributes)
        writeAttribute(attribute);
}

void MetalWriter::writeNamedTypeDecl(const AST::NamedType& type)
{
    // Have to handle builtin types
    if (type.name() == "bool"_s)
        m_msl.append("bool");
    else if (type.name() == "i32"_s)
        m_msl.append("int32_t");
    else if (type.name() == "u32"_s)
        m_msl.append("uint32_t");
    else if (type.name() == "f32"_s)
        m_msl.append("float");
    else
        m_msl.append(type.name());
}

void MetalWriter::writeVecTypeDecl(const AST::VecType& type)
{
    writeTypeDecl(&type.elementType());
    m_msl.append(type.size());
}

void MetalWriter::writeArrayTypeDecl(const AST::ArrayType& type)
{
    writeTypeDecl(&type.elementType());

    m_msl.append("[");
    if (type.maybeSizeExpression())
        writeExpression(*type.maybeSizeExpression());
    m_msl.append("]");
}

void MetalWriter::writeTypeDecl(const AST::TypeDecl* maybeType)
{
    if (!maybeType) {
        m_msl.append("void");
        return;
    }

    const auto& type = *maybeType;

    if (type.isNamed())
        writeNamedTypeDecl(downcast<AST::NamedType>(type));
    else if (type.isVec())
        writeVecTypeDecl(downcast<AST::VecType>(type));
    else if (type.isArray())
        writeArrayTypeDecl(downcast<AST::ArrayType>(type));
    else
        ASSERT_NOT_REACHED();
}

void MetalWriter::writeFunction(const AST::FunctionDecl& function)
{
    writeAttributes(function.attributes());
    m_msl.append(" ");

    writeTypeDecl(function.maybeReturnType());
    m_msl.append("\n");

    m_msl.append(function.name());
    m_msl.append("(");
    // TODO write arguments
    m_msl.append(")");

    m_msl.append("\n{");

    {
        IndentationScope scope(m_indent);
        newline();

        for (const auto& stmt : function.body().statements())
            writeStatement(stmt);
    }

    m_msl.append("}");
}

void MetalWriter::newline() {
    m_msl.append("\n"_s);
    m_msl.append(m_indent);
}

} // namespace WGSL::Writer
