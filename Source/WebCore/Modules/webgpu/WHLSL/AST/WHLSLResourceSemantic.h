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
#include "WHLSLLexer.h"

namespace WebCore {

namespace WHLSL {

namespace AST {

class ResourceSemantic : public BaseSemantic {
public:
    enum class Mode : uint8_t {
        UnorderedAccessView,
        Texture,
        Buffer,
        Sampler
    };

    ResourceSemantic(Lexer::Token&& origin, Mode mode, unsigned index, unsigned space)
        : BaseSemantic(WTFMove(origin))
        , m_mode(mode)
        , m_index(index)
        , m_space(space)
    {
    }

    virtual ~ResourceSemantic() = default;

    ResourceSemantic(const ResourceSemantic&) = delete;
    ResourceSemantic(ResourceSemantic&&) = default;

    Mode mode() const { return m_mode; }
    unsigned index() const { return m_index; }
    unsigned space() const { return m_space; }

    bool operator==(const ResourceSemantic& other) const
    {
        return m_mode == other.m_mode && m_index == other.m_index && m_space == other.m_space;
    }

    bool operator!=(const ResourceSemantic& other) const
    {
        return !(*this == other);
    }

    bool isAcceptableType(const UnnamedType&, const Intrinsics&) const override;
    bool isAcceptableForShaderItemDirection(ShaderItemDirection, const Optional<EntryPointType>&) const override;

private:
    Mode m_mode;
    unsigned m_index;
    unsigned m_space;
};

} // namespace AST

}

}

#endif
