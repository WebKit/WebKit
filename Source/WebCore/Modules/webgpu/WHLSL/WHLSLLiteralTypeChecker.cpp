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
#include "WHLSLLiteralTypeChecker.h"

#if ENABLE(WEBGPU)

#include "WHLSLIntegerLiteralType.h"
#include "WHLSLNativeTypeDeclaration.h"
#include "WHLSLNullLiteralType.h"
#include "WHLSLProgram.h"
#include "WHLSLTypeReference.h"
#include "WHLSLVisitor.h"

namespace WebCore {

namespace WHLSL {

#if !ASSERT_DISABLED
static AST::NativeTypeDeclaration* getNativeTypeDeclaration(AST::ResolvableType& resolvableType)
{
    if (!is<AST::TypeReference>(resolvableType.resolvedType()))
        return nullptr;
    auto& typeReference = downcast<AST::TypeReference>(resolvableType.resolvedType());
    if (!is<AST::NativeTypeDeclaration>(typeReference.resolvedType()))
        return nullptr;
    return &downcast<AST::NativeTypeDeclaration>(typeReference.resolvedType());
}

class LiteralTypeChecker : public Visitor {
public:
private:
    void visit(AST::FloatLiteralType& floatLiteralType) override
    {
        auto* nativeTypeDeclaration = getNativeTypeDeclaration(floatLiteralType);
        ASSERT(nativeTypeDeclaration);
        ASSERT(nativeTypeDeclaration->canRepresentFloat()(floatLiteralType.value()));
    }

    void visit(AST::IntegerLiteralType& integerLiteralType) override
    {
        auto* nativeTypeDeclaration = getNativeTypeDeclaration(integerLiteralType);
        ASSERT(nativeTypeDeclaration);
        ASSERT(nativeTypeDeclaration->canRepresentInteger()(integerLiteralType.value()));
    }

    void visit(AST::UnsignedIntegerLiteralType& unsignedIntegerLiteralType) override
    {
        auto* nativeTypeDeclaration = getNativeTypeDeclaration(unsignedIntegerLiteralType);
        ASSERT(nativeTypeDeclaration);
        ASSERT(nativeTypeDeclaration->canRepresentUnsignedInteger()(unsignedIntegerLiteralType.value()));
    }

    void visit(AST::NullLiteralType& nullLiteralType) override
    {
        ASSERT(nullLiteralType.maybeResolvedType());
    }
};
#endif

void checkLiteralTypes(Program& program)
{
#if ASSERT_DISABLED
    UNUSED_PARAM(program);
#else
    LiteralTypeChecker().Visitor::visit(program);
#endif
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
