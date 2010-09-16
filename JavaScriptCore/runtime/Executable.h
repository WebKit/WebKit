/*
 * Copyright (C) 2009, 2010 Apple Inc. All rights reserved.
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
#include <wtf/PassOwnPtr.h>

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
        MacroAssemblerCodePtr m_jitCodeForCallWithArityCheck;
        MacroAssemblerCodePtr m_jitCodeForConstructWithArityCheck;
#endif
    };

#if ENABLE(JIT)
    class NativeExecutable : public ExecutableBase {
        friend class JIT;
    public:
        static PassRefPtr<NativeExecutable> create(MacroAssemblerCodePtr callThunk, NativeFunction function, MacroAssemblerCodePtr constructThunk, NativeFunction constructor)
        {
            if (!callThunk)
                return adoptRef(new NativeExecutable(JITCode(), function, JITCode(), constructor));
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
            m_jitCodeForCallWithArityCheck = callThunk.addressForCall();
            m_jitCodeForConstructWithArityCheck = constructThunk.addressForCall();
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
        bool needsActivation() const { return m_hasCapturedVariables || m_features & (EvalFeature | WithFeature | CatchFeature); }

        virtual PassOwnPtr<ExceptionInfo> reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*) = 0;

    protected:
        void recordParse(CodeFeatures features, bool hasCapturedVariables, int firstLine, int lastLine)
        {
            m_features = features;
            m_hasCapturedVariables = hasCapturedVariables;
            m_firstLine = firstLine;
            m_lastLine = lastLine;
        }

        SourceCode m_source;
        CodeFeatures m_features;
        bool m_hasCapturedVariables;
        int m_firstLine;
        int m_lastLine;
    };

    class EvalExecutable : public ScriptExecutable {
    public:

        ~EvalExecutable();

        JSObject* compile(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            JSObject* error = 0;
            if (!m_evalCodeBlock)
                error = compileInternal(exec, scopeChainNode);
            ASSERT(!error == !!m_evalCodeBlock);
            return error;
        }

        EvalCodeBlock& generatedBytecode()
        {
            ASSERT(m_evalCodeBlock);
            return *m_evalCodeBlock;
        }

        static PassRefPtr<EvalExecutable> create(ExecState* exec, const SourceCode& source) { return adoptRef(new EvalExecutable(exec, source)); }

#if ENABLE(JIT)
        JITCode& generatedJITCode()
        {
            return generatedJITCodeForCall();
        }
#endif

    private:
        EvalExecutable(ExecState*, const SourceCode&);

        JSObject* compileInternal(ExecState*, ScopeChainNode*);

        virtual PassOwnPtr<ExceptionInfo> reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);

        OwnPtr<EvalCodeBlock> m_evalCodeBlock;
    };

    class ProgramExecutable : public ScriptExecutable {
    public:
        static PassRefPtr<ProgramExecutable> create(ExecState* exec, const SourceCode& source)
        {
            return adoptRef(new ProgramExecutable(exec, source));
        }

        ~ProgramExecutable();

        JSObject* compile(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            JSObject* error = 0;
            if (!m_programCodeBlock)
                error = compileInternal(exec, scopeChainNode);
            ASSERT(!error == !!m_programCodeBlock);
            return error;
        }

        ProgramCodeBlock& generatedBytecode()
        {
            ASSERT(m_programCodeBlock);
            return *m_programCodeBlock;
        }

        JSObject* checkSyntax(ExecState*);

#if ENABLE(JIT)
        JITCode& generatedJITCode()
        {
            return generatedJITCodeForCall();
        }
#endif

    private:
        ProgramExecutable(ExecState*, const SourceCode&);

        JSObject* compileInternal(ExecState*, ScopeChainNode*);

        virtual PassOwnPtr<ExceptionInfo> reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);

        OwnPtr<ProgramCodeBlock> m_programCodeBlock;
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
        FunctionCodeBlock& generatedBytecode()
        {
            if (m_codeBlockForCall)
                return *m_codeBlockForCall;
            ASSERT(m_codeBlockForConstruct);
            return *m_codeBlockForConstruct;
        }

        JSObject* compileForCall(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            JSObject* error = 0;
            if (!m_codeBlockForCall)
                error = compileForCallInternal(exec, scopeChainNode);
            ASSERT(!error == !!m_codeBlockForCall);
            return error;
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

        JSObject* compileForConstruct(ExecState* exec, ScopeChainNode* scopeChainNode)
        {
            JSObject* error = 0;
            if (!m_codeBlockForConstruct)
                error = compileForConstructInternal(exec, scopeChainNode);
            ASSERT(!error == !!m_codeBlockForConstruct);
            return error;
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
        void markAggregate(MarkStack&);
        static PassRefPtr<FunctionExecutable> fromGlobalCode(const Identifier&, ExecState*, Debugger*, const SourceCode&, JSObject** exception);

    private:
        FunctionExecutable(JSGlobalData*, const Identifier& name, const SourceCode&, bool forceUsesArguments, FunctionParameters*, int firstLine, int lastLine);
        FunctionExecutable(ExecState*, const Identifier& name, const SourceCode&, bool forceUsesArguments, FunctionParameters*, int firstLine, int lastLine);

        JSObject* compileForCallInternal(ExecState*, ScopeChainNode*);
        JSObject* compileForConstructInternal(ExecState*, ScopeChainNode*);

        virtual PassOwnPtr<ExceptionInfo> reparseExceptionInfo(JSGlobalData*, ScopeChainNode*, CodeBlock*);

        unsigned m_numVariables : 31;
        bool m_forceUsesArguments : 1;

        RefPtr<FunctionParameters> m_parameters;
        OwnPtr<FunctionCodeBlock> m_codeBlockForCall;
        OwnPtr<FunctionCodeBlock> m_codeBlockForConstruct;
        Identifier m_name;
        SharedSymbolTable* m_symbolTable;

#if ENABLE(JIT)
    public:
        MacroAssemblerCodePtr generatedJITCodeForCallWithArityCheck()
        {
            ASSERT(m_jitCodeForCall);
            ASSERT(m_jitCodeForCallWithArityCheck);
            return m_jitCodeForCallWithArityCheck;
        }

        MacroAssemblerCodePtr generatedJITCodeForConstructWithArityCheck()
        {
            ASSERT(m_jitCodeForConstruct);
            ASSERT(m_jitCodeForConstructWithArityCheck);
            return m_jitCodeForConstructWithArityCheck;
        }
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
