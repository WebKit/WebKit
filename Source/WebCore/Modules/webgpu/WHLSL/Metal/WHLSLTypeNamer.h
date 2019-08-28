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

#include "WHLSLMangledNames.h"
#include "WHLSLUnnamedTypeHash.h"
#include "WHLSLVisitor.h"
#include <wtf/HashMap.h>
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

    void emitMetalTypes(StringBuilder&);

    // Must be called after calling emitMetalTypes().
    String mangledNameForType(AST::NativeTypeDeclaration&);
    MangledTypeName mangledNameForType(AST::UnnamedType&);
    MangledOrNativeTypeName mangledNameForType(AST::NamedType&);
    MangledEnumerationMemberName mangledNameForEnumerationMember(AST::EnumerationMember&);
    MangledStructureElementName mangledNameForStructureElement(AST::StructureElement&);

    MangledTypeName generateNextTypeName() { return { m_typeCount++ }; }
    MangledStructureElementName generateNextStructureElementName() { return { m_structureElementCount++ }; }

private:
    void visit(AST::UnnamedType&) override;
    void visit(AST::EnumerationDefinition&) override;
    void visit(AST::NativeTypeDeclaration&) override;
    void visit(AST::StructureDefinition&) override;
    void visit(AST::TypeDefinition&) override;
    void visit(AST::Expression&) override;
    void visit(AST::CallExpression&) override;

    MangledEnumerationMemberName generateNextEnumerationMemberName() { return { m_enumerationMemberCount++ }; }

    void emitNamedTypeDefinition(StringBuilder&, AST::NamedType&, Vector<std::reference_wrapper<AST::UnnamedType>>&, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<UnnamedTypeKey>& emittedUnnamedTypes);
    void emitUnnamedTypeDefinition(StringBuilder&, AST::UnnamedType&, MangledTypeName, HashSet<AST::NamedType*>& emittedNamedTypes, HashSet<UnnamedTypeKey>& emittedUnnamedTypes);
    void emitMetalTypeDeclarations(StringBuilder&);
    void emitMetalTypeDefinitions(StringBuilder&);

    void generateUniquedTypeName(AST::UnnamedType&);

    Program& m_program;
    HashMap<UnnamedTypeKey, MangledTypeName> m_unnamedTypeMapping;
    HashMap<AST::NamedType*, MangledTypeName> m_namedTypeMapping;
    HashMap<AST::NamedType*, Vector<std::reference_wrapper<AST::UnnamedType>>> m_dependencyGraph;
    HashMap<AST::EnumerationMember*, MangledEnumerationMemberName> m_enumerationMemberMapping;
    HashMap<AST::StructureElement*, MangledStructureElementName> m_structureElementMapping;
    unsigned m_typeCount { 0 };
    unsigned m_enumerationMemberCount { 0 };
    unsigned m_structureElementCount { 0 };
};

} // namespace Metal

} // namespace WHLSL

} // namespace WebCore

#endif
