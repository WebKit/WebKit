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

#include <JavaScriptCore/ScriptExecutable.h>

namespace JSC {

class GlobalExecutable : public ScriptExecutable {
public:
    using Base = ScriptExecutable;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    DECLARE_INFO;

    // A source with no text has no position to derive, so the guard returns the unset value.
    unsigned lastLine() const
    {
        if (!hasSourceText()) [[unlikely]]
            return static_cast<unsigned>(-1);
        return derivedEndPosition().line;
    }

    unsigned endColumn() const
    {
        if (!hasSourceText()) [[unlikely]]
            return UINT_MAX;
        return derivedEndPosition().column;
    }

    LineColumn derivedEndPosition() const
    {
        SUPPRESS_UNCOUNTED_ARG // source() holds the owning ref for the whole call
        return source().provider()->documentLineColumnForOffset(source().endOffset());
    }

    DECLARE_VISIT_CHILDREN;
    DECLARE_VISIT_OUTPUT_CONSTRAINTS;

    void reconcileWeakReferencesAtGCEnd(VM&, CollectionScope);

protected:
    friend class ScriptExecutable;
    GlobalExecutable(Structure* structure, VM& vm, const SourceCode& sourceCode, LexicallyScopedFeatures lexicallyScopedFeatures, DerivedContextType derivedContextType, bool isInArrowFunctionContext, bool isInsideOrdinaryFunction, EvalContextType evalContextType, Intrinsic intrinsic)
        : Base(structure, vm, sourceCode, lexicallyScopedFeatures, derivedContextType, isInArrowFunctionContext, isInsideOrdinaryFunction, evalContextType, intrinsic)
    {
    }

    CodeBlock* codeBlock() const
    {
        return m_codeBlock.get();
    }

    UnlinkedCodeBlock* unlinkedCodeBlock() const
    {
        return m_unlinkedCodeBlock.get();
    }

    CodeBlock* replaceCodeBlockWith(VM&, CodeBlock*);


    WriteBarrier<CodeBlock> m_codeBlock;
    WriteBarrier<UnlinkedCodeBlock> m_unlinkedCodeBlock;
};

} // namespace JSC
