/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef Executable_h
#define Executable_h

#include "Nodes.h"

namespace JSC {

    template<class ASTNodeType, class CodeBlockType>
    class TemplateExecutable {
    public:
        TemplateExecutable(const SourceCode& source)
            : m_source(source)
        {
        }

        void markAggregate(MarkStack& markStack)
        {
            m_node->markAggregate(markStack);
        }

        const UString& sourceURL() const { return m_node->sourceURL(); }
        int lineNo() const { return m_node->lineNo(); }
        CodeBlockType& bytecode(ScopeChainNode* scopeChainNode) { return m_node->bytecode(scopeChainNode); }

#if ENABLE(JIT)
        JITCode& jitCode(ScopeChainNode* scopeChainNode) { return m_node->jitCode(scopeChainNode); }
#endif

    protected:
        RefPtr<ASTNodeType> m_node;
        SourceCode m_source;
    };

    class EvalExecutable : public TemplateExecutable<EvalNode, EvalCodeBlock> {
    public:
        EvalExecutable(const SourceCode& source)
            : TemplateExecutable<EvalNode, EvalCodeBlock>(source)
        {
        }

        JSObject* parse(ExecState* exec, bool allowDebug = true);
    };

    class CacheableEvalExecutable : public EvalExecutable, public RefCounted<CacheableEvalExecutable> {
    public:
        static PassRefPtr<CacheableEvalExecutable> create(const SourceCode& source) { return adoptRef(new CacheableEvalExecutable(source)); }

    private:
        CacheableEvalExecutable(const SourceCode& source)
            : EvalExecutable(source)
        {
        }
    };

    class ProgramExecutable : public TemplateExecutable<ProgramNode, ProgramCodeBlock> {
    public:
        ProgramExecutable(const SourceCode& source)
            : TemplateExecutable<ProgramNode, ProgramCodeBlock>(source)
        {
        }

        JSObject* parse(ExecState* exec, bool allowDebug = true);
    };

};

#endif
