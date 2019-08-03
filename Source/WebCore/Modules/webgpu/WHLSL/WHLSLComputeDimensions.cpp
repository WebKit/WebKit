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
#include "WHLSLComputeDimensions.h"

#if ENABLE(WEBGPU)

#include "WHLSLFunctionDeclaration.h"
#include "WHLSLPrepare.h"
#include "WHLSLProgram.h"
#include "WHLSLVisitor.h"
#include <wtf/Optional.h>

namespace WebCore {

namespace WHLSL {

Optional<ComputeDimensions> computeDimensions(Program& program, AST::FunctionDefinition& entryPoint)
{
    Optional<ComputeDimensions> computeDimensions;

    for (auto& functionDefinition : program.functionDefinitions()) {
        bool foundNumThreadsFunctionAttribute = false;
        for (auto& functionAttribute : functionDefinition->attributeBlock()) {
            auto success = WTF::visit(WTF::makeVisitor([&](AST::NumThreadsFunctionAttribute& numThreadsFunctionAttribute) {
                if (foundNumThreadsFunctionAttribute)
                    return false;
                foundNumThreadsFunctionAttribute = true;
                if (&functionDefinition == &entryPoint) {
                    ASSERT(!computeDimensions);
                    computeDimensions = {{ numThreadsFunctionAttribute.width(), numThreadsFunctionAttribute.height(), numThreadsFunctionAttribute.depth() }};
                }
                return true;
            }), functionAttribute);
            if (!success) {
                // Cannot declare multiple numthread attributes on a single function.
                return WTF::nullopt;
            }
        }
    }

    return computeDimensions;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
