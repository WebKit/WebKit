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

#include "CallData.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "Nodes.h"
#include "SamplingTool.h"

namespace JSC {

    class CodeBlock;
    class Debugger;
    class EvalCodeBlock;
    class FunctionCodeBlock;
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
            : m_numParametersForCall(numParameters)
            , m_numParametersForConstruct(numParameters)
        {
        }

        virtual ~ExecutableBase() {}

        bool isHostFunction() const
        {
            ASSERT((m_numParametersForCall == NUM_PARAMETERS_IS_HOST) == (m_numParametersForConstruct == NUM_PARAMETERS_IS_HOST));
            return m_numParametersForCall == NUM_PARAMETERS_IS_HOST;
        }

    protected:
        int m_numParametersForCall;
        int m_numParametersForConstruct;

#if ENABLE(JIT)
    public:
        JITCode& generatedJITCodeForCall()
        {
            ASSERT(m_jitCodeForCall);
            return m_jitCodeForCall;
        }

        JITCode& generatedJITCodeForConstruct()
        {
            ASSERT(m_jitCodeForConstruct);
            return m_jitCodeForConstruct;
        }

    protected:
        JITCode m_jitCodeForCall;
        JITCode m_jitCodeForConstruct;
#endif
    };

#if ENABLE(JIT)
    class NativeExecutable : public ExecutableBase {
        friend class JIT;
    public:
        static PassRefPtr<NativeExecutable> create(MacroAssemblerCodePtr callThunk, NativeFunction function, MacroAssemblerCodePtr constructThunk, NativeFunction constructor)
        {
            return adoptRef(new NativeExecutable(JITCode::HostFunction(callThunk), function, JITCode::HostFunction(constructThunk), constructor));
        }

        ~NativeExecutable();

        NativeFunction function() { return m_function; }

    private:
        NativeExecutable(JITCode callThunk, NativeFunction function, JITCode constructThunk, NativeFunction constructor)
            : ExecutableBase(NUM_PARAMETERS_IS_HOST)
            , m_function(function)
            , m_constructor(constructor)
        {
            m_jitCodeForCall = callThunk;
            m_jitCodeForConstruct = constructThunk;
        }

        NativeFunction m_function;
        // Probably should be a NativeConstructor, but this will currently require rewriting the JIT
        // trampoline. It may be easier to make NativeFunction be passed 'this' as a part of the ArgList.
        NativeFunction m_constructor;
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
        ScriptExecutable(JSGlobalData* globalData, const SourceCode& source)
            : ExecutableBase(NUM_PARAMETERS_NOT_COMPILED)
            , m_source(source)
            , m_features(0)
        {
#if ENABLE(CODEBLOCK_SAMPLING)
            if (SamplingTool* sampler = globalData->interpreter->sampler())
                sampler->notifyOfScope(this);
#else
            UNUSED_PARAM(globalData);
#endif
        }

        ScriptExecutable(ExecState* exec, const SourceCode& source)
            : ExecutableBase(NUM_PARAMETERS_NOT_COMPILED)
            , m_source(source)
            , m_features(0)
        {
#if ENABLE(CODEBLOCK_SAMPLING)
            if (SamplingTool* sampler = exec->globalData().interpreter->sampler())
                sampler->notifyOfScope(this);
#else
            UNUSED_PARAM(exec);
#endif
        }

        const SourceCode& source() { return m_source; }
        intptr_t sourceID() const { return m_source.provider()->asID(); }
        const UString& sourceURL() const { return m_source.provider()->url(); }
        int lineNo() const { return m_firstLine; }
        int lastLine() const { return m_lastLine; }

        bool usesEval() const { return m_features & EvalFeature; }
        bool usesArguments() const { return m_features & ArgumentsFeature; }
        bool needsActivation() const { return m_features & (EvalFeature | ClosureFeature | WithFeature | CatchFeature); }

        virtual ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*) = 0;

    protected:
        void recordParse(CodeFeatures features, int firstLine, int lastLine)
        {
            m_features = features;
            m_firstLine = firstLine;
            m_lastLine = lastLine;
        }

        SourceCode m_source;
        CodeFeatures m_features;
        int m_firstLine;
        int m_lastLine;
    };

    class EvalExecutable : public ScriptExecutable {
    public:

        ~EvalExecutable();

        EvalCodeBlock& bytecode(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            if (!m_evalCodeBlock) {
                JSObject* error = compile(exec, scopeChainNode);
                ASSERT_UNUSED(!error, error);
            }
            return *m_evalCodeBlock;
        }

        JSObject* compile(ExecState*, ScopeChainNode*);

        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);
        static PassRefPtr<EvalExecutable> create(ExecState* exec, const SourceCode& source) { return adoptRef(new EvalExecutable(exec, source)); }

    private:
        EvalExecutable(ExecState* exec, const SourceCode& source)
            : ScriptExecutable(exec, source)
            , m_evalCodeBlock(0)
        {
        }
        EvalCodeBlock* m_evalCodeBlock;

#if ENABLE(JIT)
    public:
        JITCode& jitCode(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCodeForCall)
                generateJITCode(exec, scopeChainNode);
            return m_jitCodeForCall;
        }

    private:
        void generateJITCode(ExecState*, ScopeChainNode*);
#endif
    };

    class ProgramExecutable : public ScriptExecutable {
    public:
        static PassRefPtr<ProgramExecutable> create(ExecState* exec, const SourceCode& source)
        {
            return adoptRef(new ProgramExecutable(exec, source));
        }

        ~ProgramExecutable();

        ProgramCodeBlock& bytecode(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            if (!m_programCodeBlock) {
                JSObject* error = compile(exec, scopeChainNode);
                ASSERT_UNUSED(!error, error);
            }
            return *m_programCodeBlock;
        }

        JSObject* checkSyntax(ExecState*);
        JSObject* compile(ExecState*, ScopeChainNode*);

        // CodeBlocks for program code are transient and therefore do not gain from from throwing out there exception information.
        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*) { ASSERT_NOT_REACHED(); return 0; }

    private:
        ProgramExecutable(ExecState* exec, const SourceCode& source)
            : ScriptExecutable(exec, source)
            , m_programCodeBlock(0)
        {
        }
        ProgramCodeBlock* m_programCodeBlock;

#if ENABLE(JIT)
    public:
        JITCode& jitCode(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCodeForCall)
                generateJITCode(exec, scopeChainNode);
            return m_jitCodeForCall;
        }

    private:
        void generateJITCode(ExecState*, ScopeChainNode*);
#endif
    };

    class FunctionExecutable : public ScriptExecutable {
        friend class JIT;
    public:
        static PassRefPtr<FunctionExecutable> create(ExecState* exec, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, int firstLine, int lastLine)
        {
            return adoptRef(new FunctionExecutable(exec, name, source, forceUsesArguments, parameters, firstLine, lastLine));
        }

        static PassRefPtr<FunctionExecutable> create(JSGlobalData* globalData, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, int firstLine, int lastLine)
        {
            return adoptRef(new FunctionExecutable(globalData, name, source, forceUsesArguments, parameters, firstLine, lastLine));
        }

        ~FunctionExecutable();

        JSFunction* make(ExecState* exec, ScopeChainNode* scopeChain)
        {
            return new (exec) JSFunction(exec, this, scopeChain);
        }
        
        // Returns either call or construct bytecode. This can be appropriate
        // for answering questions that that don't vary between call and construct --
        // for example, argumentsRegister().
        FunctionCodeBlock& generatedByteCode()
        {
            if (m_codeBlockForCall)
                return *m_codeBlockForCall;
            ASSERT(m_codeBlockForConstruct);
            return *m_codeBlockForConstruct;
        }

        FunctionCodeBlock& bytecodeForCall(ExecState* exec, ScopeChainNode* scopeChainNode) 
        {
            ASSERT(scopeChainNode);
            if (!m_codeBlockForCall)
                compileForCall(exec, scopeChainNode);
            return *m_codeBlockForCall;
        }

        bool isGeneratedForCall() const
        {
            return m_codeBlockForCall;
        }

        FunctionCodeBlock& generatedBytecodeForCall()
        {
            ASSERT(m_codeBlockForCall);
            return *m_codeBlockForCall;
        }

        FunctionCodeBlock& bytecodeForConstruct(ExecState* exec, ScopeChainNode* scopeChainNode) 
        {
            ASSERT(scopeChainNode);
            if (!m_codeBlockForConstruct)
                compileForConstruct(exec, scopeChainNode);
            return *m_codeBlockForConstruct;
        }

        bool isGeneratedForConstruct() const
        {
            return m_codeBlockForConstruct;
        }

        FunctionCodeBlock& generatedBytecodeForConstruct()
        {
            ASSERT(m_codeBlockForConstruct);
            return *m_codeBlockForConstruct;
        }

        const Identifier& name() { return m_name; }
        size_t parameterCount() const { return m_parameters->size(); }
        unsigned variableCount() const { return m_numVariables; }
        UString paramString() const;
        SharedSymbolTable* symbolTable() const { return m_symbolTable; }

        void recompile(ExecState*);
        ExceptionInfo* reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);
        void markAggregate(MarkStack& markStack);
        static PassRefPtr<FunctionExecutable> fromGlobalCode(const Identifier&, ExecState*, Debugger*, const SourceCode&, int* errLine = 0, UString* errMsg = 0);

    private:
        FunctionExecutable(JSGlobalData* globalData, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, int firstLine, int lastLine)
            : ScriptExecutable(globalData, source)
            , m_numVariables(0)
            , m_forceUsesArguments(forceUsesArguments)
            , m_parameters(parameters)
            , m_codeBlockForCall(0)
            , m_codeBlockForConstruct(0)
            , m_name(name)
            , m_symbolTable(0)
        {
            m_firstLine = firstLine;
            m_lastLine = lastLine;
        }

        FunctionExecutable(ExecState* exec, const Identifier& name, const SourceCode& source, bool forceUsesArguments, FunctionParameters* parameters, int firstLine, int lastLine)
            : ScriptExecutable(exec, source)
            , m_numVariables(0)
            , m_forceUsesArguments(forceUsesArguments)
            , m_parameters(parameters)
            , m_codeBlockForCall(0)
            , m_codeBlockForConstruct(0)
            , m_name(name)
            , m_symbolTable(0)
        {
            m_firstLine = firstLine;
            m_lastLine = lastLine;
        }

        void compileForCall(ExecState*, ScopeChainNode*);
        void compileForConstruct(ExecState*, ScopeChainNode*);

        unsigned m_numVariables : 31;
        bool m_forceUsesArguments : 1;

        RefPtr<FunctionParameters> m_parameters;
        FunctionCodeBlock* m_codeBlockForCall;
        FunctionCodeBlock* m_codeBlockForConstruct;
        Identifier m_name;
        SharedSymbolTable* m_symbolTable;

#if ENABLE(JIT)
    public:
        JITCode& jitCodeForCall(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCodeForCall)
                generateJITCodeForCall(exec, scopeChainNode);
            return m_jitCodeForCall;
        }

        JITCode& jitCodeForConstruct(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            if (!m_jitCodeForConstruct)
                generateJITCodeForConstruct(exec, scopeChainNode);
            return m_jitCodeForConstruct;
        }

    private:
        void generateJITCodeForCall(ExecState*, ScopeChainNode*);
        void generateJITCodeForConstruct(ExecState*, ScopeChainNode*);
#endif
    };

    inline FunctionExecutable* JSFunction::jsExecutable() const
    {
        ASSERT(!isHostFunctionNonInline());
        return static_cast<FunctionExecutable*>(m_executable.get());
    }

    inline bool JSFunction::isHostFunction() const
    {
        ASSERT(m_executable);
        return m_executable->isHostFunction();
    }

#if ENABLE(JIT)
    inline NativeFunction JSFunction::nativeFunction()
    {
        ASSERT(isHostFunction());
        return static_cast<NativeExecutable*>(m_executable.get())->function();
    }
#endif
}

#endif
