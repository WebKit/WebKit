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
#include "WHLSLCheckTextureReferences.h"

#include "WHLSLInferTypes.h"

#if ENABLE(WEBGPU)

namespace WebCore {

namespace WHLSL {

class TextureReferencesChecker : public Visitor {
public:
    TextureReferencesChecker() = default;

    virtual ~TextureReferencesChecker() = default;

private:
    void visit(AST::PointerType&) override;
    void visit(AST::ArrayReferenceType&) override;
    void visit(AST::ArrayType&) override;
    void visit(AST::Expression&) override;

    bool containsTextureOrSampler(AST::UnnamedType&);
};

class Searcher : public Visitor {
public:
    Searcher() = default;

    virtual ~Searcher() = default;

    bool found() const { return m_found; }

private:
    void visit(AST::NativeTypeDeclaration&);

    bool m_found { false };
};

void Searcher::visit(AST::NativeTypeDeclaration& nativeTypeDeclaration)
{
    if (nativeTypeDeclaration.isOpaqueType())
        m_found = true;
}

bool TextureReferencesChecker::containsTextureOrSampler(AST::UnnamedType& unnamedType)
{
    Searcher searcher;
    searcher.checkErrorAndVisit(unnamedType);
    return searcher.found();
}

void TextureReferencesChecker::visit(AST::PointerType& pointerType)
{
    Visitor::visit(pointerType);
    if (error())
        return;
    if (containsTextureOrSampler(pointerType.elementType()))
        setError();
}

void TextureReferencesChecker::visit(AST::ArrayReferenceType& arrayReferenceType)
{
    Visitor::visit(arrayReferenceType);
    if (error())
        return;
    if (containsTextureOrSampler(arrayReferenceType.elementType()))
        setError();
}

void TextureReferencesChecker::visit(AST::ArrayType& arrayType)
{
    Visitor::visit(arrayType);
    if (error())
        return;
    if (containsTextureOrSampler(arrayType.type()))
        setError();
}

void TextureReferencesChecker::visit(AST::Expression& expression)
{
    Visitor::visit(expression);
    checkErrorAndVisit(expression.resolvedType());
}

bool checkTextureReferences(Program& program)
{
    TextureReferencesChecker textureReferencesChecker;
    textureReferencesChecker.checkErrorAndVisit(program);
    return !textureReferencesChecker.error();
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
