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

    class CodeBlock;
    class EvalCodeBlock;
    class ProgramCodeBlock;
    class ScopeChainNode;

    struct ExceptionInfo;

    class ExecutableBase {
        friend class JIT;
    public:
        virtual ~ExecutableBase() {}

        ExecutableBase(const SourceCode& source)
            : m_source(source)
        {
        }

        const SourceCode& source() { return m_source; }
        intptr_t sourceID() const { return m_node->sourceID(); }
        const UString& sourceURL() const { return m_node->sourceURL(); }
        int lineNo() const { return m_node->lineNo(); }
        int lastLine() const { return m_node->lastLine(); }

        bool usesEval() const { return m_node->usesEval(); }
        bool usesArguments() const { return m_node->usesArguments(); }
        bool needsActivation() const { return m_node->needsActivation(); }

        virtual ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*) = 0;

        ScopeNode* astNode() { return m_node.get(); }

    protected:
        RefPtr<ScopeNode> m_node;
        SourceCode m_source;

    private:
        // For use making native thunk.
        friend class FunctionExecutable;
        ExecutableBase()
        {
        }

#if ENABLE(JIT)
    public:
        JITCode& generatedJITCode()
        {
            ASSERT(m_jitCode);
            return m_jitCode;
        }

        ExecutablePool* getExecutablePool()
        {
            return m_jitCode.getExecutablePool();
        }

    protected:
        JITCode m_jitCode;
#endif
    };

    class EvalExecutable : public ExecutableBase {
    public:
        EvalExecutable(const SourceCode& source)
            : ExecutableBase(source)
            , m_evalCodeBlock(0)
        {
        }

        ~EvalExecutable();

        JSObject* parse(ExecState* exec, bool allowDebug = true);

        EvalCodeBlock& bytecode(ScopeChainNode* scopeChainNode)
        {
            if (!m_evalCodeBlock)
                generateBytecode(scopeChainNode);
            return *m_evalCodeBlock;
        }

        DeclarationStacks::VarStack& varStack() { return m_node->varStack(); }

        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);

    private:
        EvalNode* evalNode() { return static_cast<EvalNode*>(m_node.get()); }

        void generateBytecode(ScopeChainNode*);

        EvalCodeBlock* m_evalCodeBlock;

#if ENABLE(JIT)
    public:
        JITCode& jitCode(ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCode)
                generateJITCode(scopeChainNode);
            return m_jitCode;
        }

    private:
        void generateJITCode(ScopeChainNode*);
#endif
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

    class ProgramExecutable : public ExecutableBase {
    public:
        ProgramExecutable(const SourceCode& source)
            : ExecutableBase(source)
            , m_programCodeBlock(0)
        {
        }
        
        ~ProgramExecutable();

        JSObject* parse(ExecState* exec, bool allowDebug = true);

        // CodeBlocks for program code are transient and therefore to not gain from from throwing out there exception information.
        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*) { ASSERT_NOT_REACHED(); return 0; }

        ProgramCodeBlock& bytecode(ScopeChainNode* scopeChainNode)
        {
            if (!m_programCodeBlock)
                generateBytecode(scopeChainNode);
            return *m_programCodeBlock;
        }

    private:
        ProgramNode* programNode() { return static_cast<ProgramNode*>(m_node.get()); }

        void generateBytecode(ScopeChainNode*);

        ProgramCodeBlock* m_programCodeBlock;

#if ENABLE(JIT)
    public:
        JITCode& jitCode(ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCode)
                generateJITCode(scopeChainNode);
            return m_jitCode;
        }

    private:
        void generateJITCode(ScopeChainNode*);
#endif
    };

    class FunctionExecutable : public ExecutableBase, public RefCounted<FunctionExecutable> {
        friend class JIT;
    public:
        FunctionExecutable(const Identifier& name, FunctionBodyNode* body)
            : ExecutableBase(body->source())
            , m_codeBlock(0)
            , m_name(name)
        {
            m_node = body;
        }

        ~FunctionExecutable();

        const Identifier& name() { return m_name; }

        JSFunction* make(ExecState* exec, ScopeChainNode* scopeChain);

        CodeBlock& bytecode(ScopeChainNode* scopeChainNode) 
        {
            ASSERT(scopeChainNode);
            if (!m_codeBlock)
                generateBytecode(scopeChainNode);
            return *m_codeBlock;
        }
        CodeBlock& generatedBytecode()
        {
            ASSERT(m_codeBlock);
            return *m_codeBlock;
        }

        bool usesEval() const { return body()->usesEval(); }
        bool usesArguments() const { return body()->usesArguments(); }
        size_t parameterCount() const { return body()->parameterCount(); }
        UString paramString() const { return body()->paramString(); }

        bool isHostFunction() const;
        bool isGenerated() const
        {
            return m_codeBlock;
        }

        void recompile(ExecState* exec);

        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);

        void markAggregate(MarkStack& markStack);

    private:
        FunctionBodyNode* body() const { return static_cast<FunctionBodyNode*>(m_node.get()); }

        void generateBytecode(ScopeChainNode*);

        CodeBlock* m_codeBlock;
        const Identifier& m_name;

#if ENABLE(JIT)
    public:
        JITCode& jitCode(ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCode)
                generateJITCode(scopeChainNode);
            return m_jitCode;
        }

        static PassRefPtr<FunctionExecutable> createNativeThunk(ExecState* exec)
        {
            return adoptRef(new FunctionExecutable(exec));
        }

    private:
        FunctionExecutable(ExecState* exec);
        void generateJITCode(ScopeChainNode*);
#endif
    };

};

#endif
