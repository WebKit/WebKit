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

#include "WHLSLBaseSemantic.h"
#include "WHLSLCodeLocation.h"
#include <wtf/FastMalloc.h>
#include <wtf/Optional.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class BuiltInSemantic : public BaseSemantic {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class Variable : uint8_t {
        SVInstanceID,
        SVVertexID,
        PSize,
        SVPosition,
        SVIsFrontFace,
        SVSampleIndex,
        SVInnerCoverage,
        SVTarget,
        SVDepth,
        SVCoverage,
        SVDispatchThreadID,
        SVGroupID,
        SVGroupIndex,
        SVGroupThreadID
    };

    BuiltInSemantic(CodeLocation location, Variable variable, Optional<unsigned>&& targetIndex = WTF::nullopt)
        : BaseSemantic(location)
        , m_variable(variable)
        , m_targetIndex(WTFMove(targetIndex))
    {
    }

    virtual ~BuiltInSemantic() = default;

    BuiltInSemantic(const BuiltInSemantic&) = delete;
    BuiltInSemantic(BuiltInSemantic&&) = default;

    Variable variable() const { return m_variable; }
    Optional<unsigned>& targetIndex() { return m_targetIndex; }

    bool operator==(const BuiltInSemantic& other) const
    {
        return m_variable == other.m_variable && m_targetIndex == other.m_targetIndex;
    }

    bool operator!=(const BuiltInSemantic& other) const
    {
        return !(*this == other);
    }

    StringView toString() const
    {
        switch (m_variable) {
        case Variable::SVInstanceID:
            return "SVInstanceID";
        case Variable::SVVertexID:
            return "SVVertexID";
        case Variable::PSize:
            return "PSize";
        case Variable::SVPosition:
            return "SVPosition";
        case Variable::SVIsFrontFace:
            return "SVIsFrontFace";
        case Variable::SVSampleIndex:
            return "SVSampleIndex";
        case Variable::SVInnerCoverage:
            return "SVInnerCoverage";
        case Variable::SVTarget:
            return "SVTarget";
        case Variable::SVDepth:
            return "SVDepth";
        case Variable::SVCoverage:
            return "SVCoverage";
        case Variable::SVDispatchThreadID:
            return "SVDispatchThreadID";
        case Variable::SVGroupID:
            return "SVGroupID";
        case Variable::SVGroupIndex:
            return "SVGroupIndex";
        case Variable::SVGroupThreadID:
            return "SVGroupThreadID";
        }
    }

    bool isAcceptableType(const UnnamedType&, const Intrinsics&) const override;
    bool isAcceptableForShaderItemDirection(ShaderItemDirection, const Optional<EntryPointType>&) const override;

private:
    Variable m_variable;
    Optional<unsigned> m_targetIndex;
};

} // namespace AST

}

}

#endif
