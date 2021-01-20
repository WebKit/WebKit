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
#include "WHLSLCheckReferenceTypes.h"

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLAST.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

class ReferenceTypeChecker final : public Visitor {
public:
    ReferenceTypeChecker() = default;
    virtual ~ReferenceTypeChecker() = default;

private:
    ALWAYS_INLINE void checkType(AST::UnnamedType& type, CodeLocation codeLocation, const char* error)
    {
        auto& unifiedType = type.unifyNode();
        if (is<AST::ReferenceType>(unifiedType)) {
            setError(Error(error, codeLocation));
            return;
        }

        if (is<AST::UnnamedType>(unifiedType))
            checkErrorAndVisit(downcast<AST::UnnamedType>(unifiedType));
    }

    void visit(AST::ReferenceType& referenceType) override
    {
        checkType(referenceType.elementType(), referenceType.codeLocation(), "reference type cannot have another reference type as an inner type");
    }

    void visit(AST::ArrayType& arrayType) override
    {
        checkType(arrayType.type(), arrayType.codeLocation(), "array type cannot have a reference type as its inner type");
    }

    void visit(AST::StructureElement& structureElement) override
    {
        checkType(structureElement.type(), structureElement.codeLocation(), "cannot have a structure field which is a reference type");
    }

    void visit(AST::Expression& expression) override
    {
        Visitor::visit(expression);
        checkErrorAndVisit(expression.resolvedType());
    }
};

Expected<void, Error> checkReferenceTypes(Program& program)
{
    // This pass does these things:
    // 1. Disallow structs to have fields which are references. This prevents us from having to
    // figure out how to default initialize such a struct. We could relax this in the future if we
    // did an analysis on which structs contain reference fields, and ensure such struct variables
    // always have initializers. This would also require us to create constructors for structs which
    // initialize each field.
    // 2. We also do the same for arrays.
    // 3. References can only be one level deep. So no pointers to pointers. No references to
    // references, etc. This is to support logical mode.
    ReferenceTypeChecker referenceTypeChecker;
    referenceTypeChecker.checkErrorAndVisit(program);
    return referenceTypeChecker.result();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
