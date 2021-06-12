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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLCodeLocation.h"
#include "WHLSLEntryPointType.h"
#include <wtf/FastMalloc.h>

namespace WebCore {

namespace WHLSL {

class Intrinsics;

namespace AST {

class FunctionDefinition;
class UnnamedType;

class BaseSemantic {
    WTF_MAKE_FAST_ALLOCATED;
public:
    BaseSemantic(CodeLocation location)
        : m_codeLocation(location)
    {
    }

    virtual ~BaseSemantic() = default;

    BaseSemantic(const BaseSemantic&) = delete;
    BaseSemantic(BaseSemantic&&) = default;

    virtual bool isAcceptableType(const UnnamedType&, const Intrinsics&) const = 0;

    enum class ShaderItemDirection : uint8_t {
        Input,
        Output
    };
    virtual bool isAcceptableForShaderItemDirection(ShaderItemDirection, const std::optional<EntryPointType>&) const = 0;

private:
    CodeLocation m_codeLocation;
};

} // namespace AST

}

}

#endif // ENABLE(WHLSL_COMPILER)
