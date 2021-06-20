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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExecutableToCodeBlockEdge.h"
#include "ScriptExecutable.h"

namespace JSC {

class GlobalExecutable : public ScriptExecutable {
public:
    using Base = ScriptExecutable;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    DECLARE_INFO;

    unsigned lastLine() const { return m_lastLine; }
    unsigned endColumn() const { return m_endColumn; }

    void recordParse(CodeFeatures features, LexicalScopeFeatures lexicalScopeFeatures, bool hasCapturedVariables, int lastLine, unsigned endColumn)
    {
        Base::recordParse(features, lexicalScopeFeatures, hasCapturedVariables);
        m_lastLine = lastLine;
        m_endColumn = endColumn;
        ASSERT(endColumn != UINT_MAX);
    }

protected:
    GlobalExecutable(Structure* structure, VM& vm, const SourceCode& sourceCode, bool isInStrictContext, DerivedContextType derivedContextType, bool isInArrowFunctionContext, bool isInsideOrdinaryFunction, EvalContextType evalContextType, Intrinsic intrinsic)
        : Base(structure, vm, sourceCode, isInStrictContext ? StrictModeLexicalFeature : NoLexicalFeatures, derivedContextType, isInArrowFunctionContext, isInsideOrdinaryFunction, evalContextType, intrinsic)
    {
    }

    int m_lastLine { -1 };
    unsigned m_endColumn { UINT_MAX };
};

} // namespace JSC
