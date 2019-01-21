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

#pragma once

#if ENABLE(WEBGPU)

#include "WHLSLVisitor.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class EnumerationDefinition;
class EnumerationMember;
class NamedType;
class NativeTypeDeclaration;
class StructureDefinition;
class TypeDefinition;
class UnnamedType;

}

class Program;

namespace Metal {

class BaseTypeNameNode;

class TypeNamer : private Visitor {
public:
    TypeNamer(Program&);
    virtual ~TypeNamer();

    String metalTypes();

    // Must be called after calling metalTypes().
    String mangledNameForType(AST::NativeTypeDeclaration&);
    String mangledNameForType(AST::UnnamedType&);
    String mangledNameForType(AST::NamedType&);
    String mangledNameForEnumerationMember(AST::EnumerationMember&);
    String mangledNameForStructureElement(AST::StructureElement&);

    String generateNextTypeName()
    {
        return makeString("type", m_typeCount++);
    }

    String generateNextStructureElementName()
    {
        return makeString("structureElement", m_structureElementCount++);
    }

private:
    void visit(AST::UnnamedType&) override;
    void visit(AST::EnumerationDefinition&) override;
    void visit(AST::NativeTypeDeclaration&) override;
    void visit(AST::StructureDefinition&) override;
    void visit(AST::TypeDefinition&) override;

    String generateNextEnumerationMemberName()
    {
        return makeString("enumerationMember", m_enumerationMemberCount++);
    }

    void emitNamedTypeDefinition(AST::NamedType&, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<BaseTypeNameNode*>& emittedUnnamedTypes, StringBuilder&);
    void emitUnnamedTypeDefinition(BaseTypeNameNode&, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<BaseTypeNameNode*>& emittedUnnamedTypes, StringBuilder&);
    String metalTypeDeclarations();
    String metalTypeDefinitions();

    UniqueRef<BaseTypeNameNode> createNameNode(AST::UnnamedType&, BaseTypeNameNode* parent);
    size_t insert(AST::UnnamedType&, Vector<UniqueRef<BaseTypeNameNode>>&);

    Program& m_program;
    Vector<UniqueRef<BaseTypeNameNode>> m_trie;
    HashMap<AST::UnnamedType*, BaseTypeNameNode*> m_unnamedTypeMapping;
    HashMap<AST::NamedType*, String> m_namedTypeMapping;
    HashMap<AST::NamedType*, Vector<std::reference_wrapper<BaseTypeNameNode>>> m_dependencyGraph;
    HashMap<AST::EnumerationMember*, String> m_enumerationMemberMapping;
    HashMap<AST::StructureElement*, String> m_structureElementMapping;
    unsigned m_typeCount { 0 };
    unsigned m_enumerationMemberCount { 0 };
    unsigned m_structureElementCount { 0 };
};

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
