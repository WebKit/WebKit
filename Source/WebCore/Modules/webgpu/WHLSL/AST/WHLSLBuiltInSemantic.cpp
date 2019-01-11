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
#include "WHLSLBuiltInSemantic.h"

#if ENABLE(WEBGPU)

#include "WHLSLFunctionDefinition.h"
#include "WHLSLTypeReference.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

bool BuiltInSemantic::isAcceptableType(const UnnamedType&, const Intrinsics&) const
{
    // FIXME: Implement this
    return true;
}

bool BuiltInSemantic::isAcceptableForShaderItemDirection(ShaderItemDirection direction, const FunctionDefinition& functionDefinition) const
{
    switch (*functionDefinition.entryPointType()) {
    case FunctionDeclaration::EntryPointType::Vertex:
        switch (direction) {
        case ShaderItemDirection::Input:
            switch (m_variable) {
            case Variable::SVInstanceID:
            case Variable::SVVertexID:
                return true;
            default:
                return false;
            }
        case ShaderItemDirection::Output:
            switch (m_variable) {
            case Variable::PSize:
            case Variable::SVPosition:
                return true;
            default:
                return false;
            }
        }
    case FunctionDeclaration::EntryPointType::Fragment:
        switch (direction) {
        case ShaderItemDirection::Input:
            switch (m_variable) {
            case Variable::SVIsFrontFace:
            case Variable::SVPosition:
            case Variable::SVSampleIndex:
            case Variable::SVInnerCoverage:
                return true;
            default:
                return false;
            }
        case ShaderItemDirection::Output:
            switch (m_variable) {
            case Variable::SVTarget:
            case Variable::SVDepth:
            case Variable::SVCoverage:
                return true;
            default:
                return false;
            }
        }
    case FunctionDeclaration::EntryPointType::Compute:
        switch (direction) {
        case ShaderItemDirection::Input:
            switch (m_variable) {
            case Variable::SVDispatchThreadID:
            case Variable::SVGroupID:
            case Variable::SVGroupIndex:
            case Variable::SVGroupThreadID:
                return true;
            default:
                return false;
            }
        case ShaderItemDirection::Output:
            return false;
        }
    }
}

} // namespace AST

}

}

#endif
