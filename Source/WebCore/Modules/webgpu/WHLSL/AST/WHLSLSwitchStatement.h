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

#include "WHLSLExpression.h"
#include "WHLSLStatement.h"
#include "WHLSLSwitchCase.h"
#include <wtf/FastMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

namespace WHLSL {

namespace AST {

class SwitchStatement final : public Statement {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SwitchStatement(CodeLocation location, UniqueRef<Expression>&& value, Vector<SwitchCase>&& switchCases)
        : Statement(location, Kind::Switch)
        , m_value(WTFMove(value))
        , m_switchCases(WTFMove(switchCases))
    {
    }

    ~SwitchStatement() = default;

    SwitchStatement(const SwitchStatement&) = delete;
    SwitchStatement(SwitchStatement&&) = default;

    Expression& value() { return m_value; }
    Vector<SwitchCase>& switchCases() { return m_switchCases; }

private:
    UniqueRef<Expression> m_value;
    Vector<SwitchCase> m_switchCases;
};

} // namespace AST

}

}

DEFINE_DEFAULT_DELETE(SwitchStatement)

SPECIALIZE_TYPE_TRAITS_WHLSL_STATEMENT(SwitchStatement, isSwitchStatement())

#endif // ENABLE(WHLSL_COMPILER)
