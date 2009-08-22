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

#include "JSFunction.h"
#include "Nodes.h"

namespace JSC {

    class CodeBlock;
    class EvalCodeBlock;
    class ProgramCodeBlock;
    class ScopeChainNode;

    struct ExceptionInfo;

    class ExecutableBase : public RefCounted<ExecutableBase> {
        friend class JIT;

    protected:
        static const int NUM_PARAMETERS_IS_HOST = 0;
        static const int NUM_PARAMETERS_NOT_COMPILED = -1;
    
    public:
        ExecutableBase(int numParameters)
            : m_numParameters(numParameters)
        {
        }

        virtual ~ExecutableBase() {}

        bool isHostFunction() const { return m_numParameters == NUM_PARAMETERS_IS_HOST; }

    protected:
        int m_numParameters;

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

#if ENABLE(JIT)
    class NativeExecutable : public ExecutableBase {
    public:
        NativeExecutable(ExecState* exec)
            : ExecutableBase(NUM_PARAMETERS_IS_HOST)
        {
            m_jitCode = JITCode(JITCode::HostFunction(exec->globalData().jitStubs.ctiNativeCallThunk()));
        }

        ~NativeExecutable();
    };
#endif

    class VPtrHackExecutable : public ExecutableBase {
    public:
        VPtrHackExecutable()
            : ExecutableBase(NUM_PARAMETERS_IS_HOST)
        {
        }

        ~VPtrHackExecutable();
    };

    class ScriptExecutable : public ExecutableBase {
    public:
        ScriptExecutable(const SourceCode& source)
            : ExecutableBase(NUM_PARAMETERS_NOT_COMPILED)
            , m_source(source)
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

    protected:
        SourceCode m_source;
        RefPtr<ScopeNode> m_node;
    };

    class EvalExecutable : public ScriptExecutable {
    public:
        EvalExecutable(const SourceCode& source)
            : ScriptExecutable(source)
            , m_evalCodeBlock(0)
        {
        }

        ~EvalExecutable();

        JSObject* parse(ExecState*, bool allowDebug = true);

        EvalCodeBlock& bytecode(ScopeChainNode* scopeChainNode)
        {
            if (!m_evalCodeBlock)
                generateBytecode(scopeChainNode);
            return *m_evalCodeBlock;
        }

        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);

        static PassRefPtr<EvalExecutable> create(const SourceCode& source) { return adoptRef(new EvalExecutable(source)); }

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

    class ProgramExecutable : public ScriptExecutable {
    public:
        ProgramExecutable(const SourceCode& source)
            : ScriptExecutable(source)
            , m_programCodeBlock(0)
        {
        }
        
        ~ProgramExecutable();

        JSObject* parse(ExecState*, bool allowDebug = true);

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

    class FunctionExecutable : public ScriptExecutable {
        friend class JIT;
    public:
        FunctionExecutable(const Identifier& name, FunctionBodyNode* body)
            : ScriptExecutable(body->source())
            , m_codeBlock(0)
            , m_name(name)
            , m_numVariables(0)
        {
            m_node = body;
        }

        ~FunctionExecutable();

        const Identifier& name() { return m_name; }

        JSFunction* make(ExecState*, ScopeChainNode* scopeChain);

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
        size_t variableCount() const { return m_numVariables; }
        UString paramString() const { return body()->paramString(); }

        bool isGenerated() const
        {
            return m_codeBlock;
        }

        void recompile(ExecState*);

        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);

        void markAggregate(MarkStack& markStack);

    private:
        FunctionBodyNode* body() const { return static_cast<FunctionBodyNode*>(m_node.get()); }

        void generateBytecode(ScopeChainNode*);

        CodeBlock* m_codeBlock;
        const Identifier& m_name;
        size_t m_numVariables;

#if ENABLE(JIT)
    public:
        JITCode& jitCode(ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCode)
                generateJITCode(scopeChainNode);
            return m_jitCode;
        }

    private:
        FunctionExecutable(ExecState*);
        void generateJITCode(ScopeChainNode*);
#endif
    };

    inline FunctionExecutable* JSFunction::jsExecutable() const
    {
        ASSERT(!isHostFunctionNonInline());
        return static_cast<FunctionExecutable*>(m_executable.get());
    }

    inline JSFunction* FunctionExecutable::make(ExecState* exec, ScopeChainNode* scopeChain)
    {
        return new (exec) JSFunction(exec, this, scopeChain);
    }

    inline bool JSFunction::isHostFunction() const
    {
        ASSERT(m_executable);
        return m_executable->isHostFunction();
    }

}

#endif
