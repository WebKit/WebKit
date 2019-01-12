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

#include "WHLSLResolvingType.h"
#include "WHLSLTypeArgument.h"
#include <memory>

namespace WebCore {

namespace WHLSL {

namespace AST {

class FunctionDeclaration;
class NamedType;
class ResolvableType;
class UnnamedType;

}

bool matches(const AST::UnnamedType&, const AST::UnnamedType&);
bool matches(const AST::NamedType&, const AST::NamedType&);
bool matches(const AST::UnnamedType&, const AST::NamedType&);
// FIXME: Is anyone actually using the return type here?
Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(AST::UnnamedType&, AST::ResolvableType&);
Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(AST::NamedType&, AST::ResolvableType&);
Optional<UniqueRef<AST::UnnamedType>> matchAndCommit(AST::ResolvableType&, AST::ResolvableType&);
Optional<UniqueRef<AST::UnnamedType>> commit(AST::ResolvableType&);
bool inferTypesForTypeArguments(AST::NamedType& possibleType, AST::TypeArguments&);
bool inferTypesForCall(AST::FunctionDeclaration& possibleFunction, Vector<std::reference_wrapper<ResolvingType>>& argumentTypes, Optional<std::reference_wrapper<AST::NamedType>>& castReturnType);

}

}

#endif
