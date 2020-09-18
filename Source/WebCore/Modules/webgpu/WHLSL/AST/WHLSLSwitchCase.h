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

#include "WHLSLBlock.h"
#include "WHLSLCodeLocation.h"
#include "WHLSLConstantExpression.h"
#include "WHLSLStatement.h"
#include <wtf/FastMalloc.h>
#include <wtf/Optional.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class SwitchCase final : public Statement {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SwitchCase(CodeLocation location, Optional<ConstantExpression>&& value, Block&& block)
        : Statement(location, Kind::SwitchCase)
        , m_value(WTFMove(value))
        , m_block(WTFMove(block))
    {
    }

    ~SwitchCase() = default;

    SwitchCase(const SwitchCase&) = delete;
    SwitchCase(SwitchCase&&) = default;

    Optional<ConstantExpression>& value() { return m_value; }
    Block& block() { return m_block; }

private:
    Optional<ConstantExpression> m_value;
    Block m_block;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(SwitchCase)

SPECIALIZE_TYPE_TRAITS_WHLSL_STATEMENT(SwitchCase, isSwitchCase())

#endif // ENABLE(WHLSL_COMPILER)
