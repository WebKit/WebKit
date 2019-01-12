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

#include "WHLSLVisitor.h"
#include <wtf/HashSet.h>

namespace WebCore {

namespace WHLSL {

class RecursiveTypeChecker : public Visitor {
public:
    ~RecursiveTypeChecker() = default;

    void visit(AST::TypeDefinition& typeDefinition) override
    {
        auto addResult = m_types.add(&typeDefinition);
        if (!addResult.isNewEntry) {
            setError();
            return;
        }

        Visitor::visit(typeDefinition);

        auto success = m_types.remove(&typeDefinition);
        ASSERT_UNUSED(success, success);
    }

    void visit(AST::StructureDefinition& structureDefinition) override
    {
        auto addResult = m_types.add(&structureDefinition);
        if (!addResult.isNewEntry) {
            setError();
            return;
        }

        Visitor::visit(structureDefinition);

        auto success = m_types.remove(&structureDefinition);
        ASSERT_UNUSED(success, success);
    }

    void visit(AST::TypeReference& typeReference) override
    {
        auto addResult = m_types.add(&typeReference);
        if (!addResult.isNewEntry) {
            setError();
            return;
        }

        for (auto& typeArgument : typeReference.typeArguments())
            checkErrorAndVisit(typeArgument);
        checkErrorAndVisit(*typeReference.resolvedType());

        auto success = m_types.remove(&typeReference);
        ASSERT_UNUSED(success, success);
    }

    void visit(AST::ReferenceType&) override
    {
    }

private:
    HashSet<AST::Type*> m_types;
};

bool checkRecursiveTypes(Program& program)
{
    RecursiveTypeChecker recursiveTypeChecker;
    recursiveTypeChecker.checkErrorAndVisit(program);
    return recursiveTypeChecker.error();
}

}

}

#endif
