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

#include "WHLSLStructureDefinition.h"
#include "WHLSLTypeDefinition.h"
#include "WHLSLProgram.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVisitor.h"
#include <wtf/HashSet.h>

namespace WebCore {

namespace WHLSL {

class RecursiveTypeChecker : public Visitor {
public:
    void visit(AST::TypeDefinition&) override;
    void visit(AST::StructureDefinition&) override;
    void visit(AST::TypeReference&) override;
    void visit(AST::ReferenceType&) override;

private:
    HashSet<AST::Type*> m_startedVisiting;
    HashSet<AST::Type*> m_finishedVisiting;
};

#define START_VISITING(t) \
do { \
    if (m_finishedVisiting.contains(t)) \
        return; \
    auto resultStartedVisiting = m_startedVisiting.add(t); \
    if (!resultStartedVisiting.isNewEntry) { \
        setError(Error("Cannot declare recursive types.", (t)->codeLocation())); \
        return; \
    } \
} while (false);

#define END_VISITING(t) \
do { \
    auto resultFinishedVisiting = m_finishedVisiting.add(t); \
    ASSERT_UNUSED(resultFinishedVisiting, resultFinishedVisiting.isNewEntry); \
} while (false);

void RecursiveTypeChecker::visit(AST::TypeDefinition& typeDefinition)
{
    START_VISITING(&typeDefinition);

    Visitor::visit(typeDefinition);

    END_VISITING(&typeDefinition);
}

void RecursiveTypeChecker::visit(AST::StructureDefinition& structureDefinition)
{
    START_VISITING(&structureDefinition);

    Visitor::visit(structureDefinition);

    END_VISITING(&structureDefinition);
}

void RecursiveTypeChecker::visit(AST::TypeReference& typeReference)
{
    START_VISITING(&typeReference);

    for (auto& typeArgument : typeReference.typeArguments())
        checkErrorAndVisit(typeArgument);
    if (typeReference.maybeResolvedType()) // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198161 Shouldn't we know by now whether the type has been resolved or not?
        checkErrorAndVisit(typeReference.resolvedType());

    END_VISITING(&typeReference);
}

void RecursiveTypeChecker::visit(AST::ReferenceType&)
{
}

Expected<void, Error> checkRecursiveTypes(Program& program)
{
    RecursiveTypeChecker recursiveTypeChecker;
    for (auto& typeDefinition : program.typeDefinitions())
        recursiveTypeChecker.checkErrorAndVisit(typeDefinition);
    for (auto& structureDefinition : program.structureDefinitions())
        recursiveTypeChecker.checkErrorAndVisit(structureDefinition);
    return recursiveTypeChecker.result();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
