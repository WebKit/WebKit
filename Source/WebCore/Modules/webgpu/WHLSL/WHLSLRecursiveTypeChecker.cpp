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

#include "config.h"
#include "WHLSLRecursiveTypeChecker.h"

#if ENABLE(WEBGPU)

#include "WHLSLScopedSetAdder.h"
#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVisitor.h"
#include <wtf/HashSet.h>

namespace WebCore {

namespace WHLSL {

class RecursiveTypeChecker : public Visitor {
public:
    virtual ~RecursiveTypeChecker() = default;

    void visit(AST::TypeDefinition&) override;
    void visit(AST::StructureDefinition&) override;
    void visit(AST::TypeReference&) override;
    void visit(AST::ReferenceType&) override;

private:
    using Adder = ScopedSetAdder<AST::Type*>;
    HashSet<AST::Type*> m_types;
};

void RecursiveTypeChecker::visit(AST::TypeDefinition& typeDefinition)
{
    Adder adder(m_types, &typeDefinition);
    if (!adder.isNewEntry()) {
        setError();
        return;
    }

    Visitor::visit(typeDefinition);
}

void RecursiveTypeChecker::visit(AST::StructureDefinition& structureDefinition)
{
    Adder adder(m_types, &structureDefinition);
    if (!adder.isNewEntry()) {
        setError();
        return;
    }

    Visitor::visit(structureDefinition);
}

void RecursiveTypeChecker::visit(AST::TypeReference& typeReference)
{
    Adder adder(m_types, &typeReference);
    if (!adder.isNewEntry()) {
        setError();
        return;
    }

    for (auto& typeArgument : typeReference.typeArguments())
        checkErrorAndVisit(typeArgument);
    if (typeReference.maybeResolvedType()) // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198161 Shouldn't we know by now whether the type has been resolved or not?
        checkErrorAndVisit(typeReference.resolvedType());
}

void RecursiveTypeChecker::visit(AST::ReferenceType&)
{
}

bool checkRecursiveTypes(Program& program)
{
    RecursiveTypeChecker recursiveTypeChecker;
    recursiveTypeChecker.checkErrorAndVisit(program);
    return !recursiveTypeChecker.error();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
