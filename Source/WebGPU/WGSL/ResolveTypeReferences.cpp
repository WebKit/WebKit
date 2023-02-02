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

#include "config.h"
#include "ResolveTypeReferences.h"

#include "AST.h"
#include "ASTVisitor.h"
#include <wtf/HashMap.h>
#include <wtf/Ref.h>

namespace WGSL {

class ResolveTypeReferences : public AST::Visitor {
public:
    ResolveTypeReferences()
        : AST::Visitor()
    {
    }

    void visit(AST::Structure&) override;
    void visit(AST::NamedTypeName&) override;

private:
    HashMap<String, Ref<AST::TypeName>> m_typeContext;
};


void ResolveTypeReferences::visit(AST::Structure& structure)
{
    m_typeContext.add(structure.name(), adoptRef(*new AST::StructTypeName(structure.span(), structure)));
}

void ResolveTypeReferences::visit(AST::NamedTypeName& namedType)
{
    auto it = m_typeContext.find(namedType.name());
    if (it != m_typeContext.end())
        namedType.resolveTypeReference(it->value.copyRef());
}

void resolveTypeReferences(AST::ShaderModule& shaderModule)
{
    ResolveTypeReferences().AST::Visitor::visit(shaderModule);
}

} // namespace WGSL
