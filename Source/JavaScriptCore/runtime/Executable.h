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
#include "CodeBlockHash.h"
#include "CodeSpecializationKind.h"
#include "HandlerInfo.h"
#include "JSFunction.h"
#include "Interpreter.h"
#include "JSGlobalObject.h"
#include "LLIntCLoop.h"
#include "Nodes.h"
#include "SamplingTool.h"
#include "UnlinkedCodeBlock.h"
#include <wtf/PassOwnPtr.h>

namespace JSC {

    class CodeBlock;
    class Debugger;
    class EvalCodeBlock;
    class FunctionCodeBlock;
    class LLIntOffsetsExtractor;
    class ProgramCodeBlock;
    class JSScope;
    
    enum CompilationKind { FirstCompilation, OptimizingCompilation };

    inline bool isCall(CodeSpecializationKind kind)
    {
        if (kind == CodeForCall)
            return true;
        ASSERT(kind == CodeForConstruct);
        return false;
    }

    class ExecutableBase : public JSCell, public DoublyLinkedListNode<ExecutableBase> {
        friend class WTF::DoublyLinkedListNode<ExecutableBase>;
        friend class JIT;

    protected:
        static const int NUM_PARAMETERS_IS_HOST = 0;
        static const int NUM_PARAMETERS_NOT_COMPILED = -1;

        ExecutableBase(JSGlobalData& globalData, Structure* structure, int numParameters)
            : JSCell(globalData, structure)
            , m_numParametersForCall(numParameters)
            , m_numParametersForConstruct(numParameters)
        {
        }

        void finishCreation(JSGlobalData& globalData)
        {
            Base::finishCreation(globalData);
        }

    public:
        typedef JSCell Base;

#if ENABLE(JIT)
        static const bool needsDestruction = true;
        static const bool hasImmortalStructure = true;
        static void destroy(JSCell*);
#endif
        
        CodeBlockHash hashFor(CodeSpecializationKind) const;

        bool isFunctionExecutable()
        {
            return structure()->typeInfo().type() == FunctionExecutableType;
        }

        bool isHostFunction() const
        {
            ASSERT((m_numParametersForCall == NUM_PARAMETERS_IS_HOST) == (m_numParametersForConstruct == NUM_PARAMETERS_IS_HOST));
            return m_numParametersForCall == NUM_PARAMETERS_IS_HOST;
        }

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(globalData, globalObject, proto, TypeInfo(CompoundType, StructureFlags), &s_info); }
        
        void clearCode();

        static JS_EXPORTDATA const ClassInfo s_info;

    protected:
        static const unsigned StructureFlags = 0;
        int m_numParametersForCall;
        int m_numParametersForConstruct;

    public:
        static void clearCodeVirtual(ExecutableBase*);

#if ENABLE(JIT)
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
        
        JITCode& generatedJITCodeFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return generatedJITCodeForCall();
            ASSERT(kind == CodeForConstruct);
            return generatedJITCodeForConstruct();
        }

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
        
        MacroAssemblerCodePtr generatedJITCodeWithArityCheckFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return generatedJITCodeForCallWithArityCheck();
            ASSERT(kind == CodeForConstruct);
            return generatedJITCodeForConstructWithArityCheck();
        }
        
        bool hasJITCodeForCall() const
        {
            return m_numParametersForCall >= 0;
        }
        
        bool hasJITCodeForConstruct() const
        {
            return m_numParametersForConstruct >= 0;
        }
        
        bool hasJITCodeFor(CodeSpecializationKind kind) const
        {
            if (kind == CodeForCall)
                return hasJITCodeForCall();
            ASSERT(kind == CodeForConstruct);
            return hasJITCodeForConstruct();
        }

        // Intrinsics are only for calls, currently.
        Intrinsic intrinsic() const;
        
        Intrinsic intrinsicFor(CodeSpecializationKind kind) const
        {
            if (isCall(kind))
                return intrinsic();
            return NoIntrinsic;
        }
        
        static ptrdiff_t offsetOfJITCodeFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return OBJECT_OFFSETOF(ExecutableBase, m_jitCodeForCall);
            ASSERT(kind == CodeForConstruct);
            return OBJECT_OFFSETOF(ExecutableBase, m_jitCodeForConstruct);
        }
        
        static ptrdiff_t offsetOfJITCodeWithArityCheckFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return OBJECT_OFFSETOF(ExecutableBase, m_jitCodeForCallWithArityCheck);
            ASSERT(kind == CodeForConstruct);
            return OBJECT_OFFSETOF(ExecutableBase, m_jitCodeForConstructWithArityCheck);
        }
        
        static ptrdiff_t offsetOfNumParametersFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return OBJECT_OFFSETOF(ExecutableBase, m_numParametersForCall);
            ASSERT(kind == CodeForConstruct);
            return OBJECT_OFFSETOF(ExecutableBase, m_numParametersForConstruct);
        }
#endif // ENABLE(JIT)

#if ENABLE(JIT) || ENABLE(LLINT_C_LOOP)
        MacroAssemblerCodePtr hostCodeEntryFor(CodeSpecializationKind kind)
        {
            #if ENABLE(JIT)
            return generatedJITCodeFor(kind).addressForCall();
            #else
            return LLInt::CLoop::hostCodeEntryFor(kind);
            #endif
        }

        MacroAssemblerCodePtr jsCodeEntryFor(CodeSpecializationKind kind)
        {
            #if ENABLE(JIT)
            return generatedJITCodeFor(kind).addressForCall();
            #else
            return LLInt::CLoop::jsCodeEntryFor(kind);
            #endif
        }

        MacroAssemblerCodePtr jsCodeWithArityCheckEntryFor(CodeSpecializationKind kind)
        {
            #if ENABLE(JIT)
            return generatedJITCodeWithArityCheckFor(kind);
            #else
            return LLInt::CLoop::jsCodeEntryWithArityCheckFor(kind);
            #endif
        }

        static void* catchRoutineFor(HandlerInfo* handler, Instruction* catchPCForInterpreter)
        {
            #if ENABLE(JIT)
            UNUSED_PARAM(catchPCForInterpreter);
            return handler->nativeCode.executableAddress();
            #else
            UNUSED_PARAM(handler);
            return LLInt::CLoop::catchRoutineFor(catchPCForInterpreter);
            #endif
        }
#endif // ENABLE(JIT || ENABLE(LLINT_C_LOOP)

    protected:
        ExecutableBase* m_prev;
        ExecutableBase* m_next;

#if ENABLE(JIT)
        JITCode m_jitCodeForCall;
        JITCode m_jitCodeForConstruct;
        MacroAssemblerCodePtr m_jitCodeForCallWithArityCheck;
        MacroAssemblerCodePtr m_jitCodeForConstructWithArityCheck;
#endif
    };

    class NativeExecutable : public ExecutableBase {
        friend class JIT;
        friend class LLIntOffsetsExtractor;
    public:
        typedef ExecutableBase Base;

#if ENABLE(JIT)
        static NativeExecutable* create(JSGlobalData& globalData, MacroAssemblerCodeRef callThunk, NativeFunction function, MacroAssemblerCodeRef constructThunk, NativeFunction constructor, Intrinsic intrinsic)
        {
            NativeExecutable* executable;
            if (!callThunk) {
                executable = new (NotNull, allocateCell<NativeExecutable>(globalData.heap)) NativeExecutable(globalData, function, constructor);
                executable->finishCreation(globalData, JITCode(), JITCode(), intrinsic);
            } else {
                executable = new (NotNull, allocateCell<NativeExecutable>(globalData.heap)) NativeExecutable(globalData, function, constructor);
                executable->finishCreation(globalData, JITCode::HostFunction(callThunk), JITCode::HostFunction(constructThunk), intrinsic);
            }
            return executable;
        }
#endif

#if ENABLE(LLINT_C_LOOP)
        static NativeExecutable* create(JSGlobalData& globalData, NativeFunction function, NativeFunction constructor)
        {
            ASSERT(!globalData.canUseJIT());
            NativeExecutable* executable = new (NotNull, allocateCell<NativeExecutable>(globalData.heap)) NativeExecutable(globalData, function, constructor);
            executable->finishCreation(globalData);
            return executable;
        }
#endif

#if ENABLE(JIT)
        static void destroy(JSCell*);
#endif

        CodeBlockHash hashFor(CodeSpecializationKind) const;

        NativeFunction function() { return m_function; }
        NativeFunction constructor() { return m_constructor; }
        
        NativeFunction nativeFunctionFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return function();
            ASSERT(kind == CodeForConstruct);
            return constructor();
        }
        
        static ptrdiff_t offsetOfNativeFunctionFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return OBJECT_OFFSETOF(NativeExecutable, m_function);
            ASSERT(kind == CodeForConstruct);
            return OBJECT_OFFSETOF(NativeExecutable, m_constructor);
        }

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto) { return Structure::create(globalData, globalObject, proto, TypeInfo(LeafType, StructureFlags), &s_info); }
        
        static const ClassInfo s_info;

        Intrinsic intrinsic() const;

    protected:
#if ENABLE(JIT)
        void finishCreation(JSGlobalData& globalData, JITCode callThunk, JITCode constructThunk, Intrinsic intrinsic)
        {
            Base::finishCreation(globalData);
            m_jitCodeForCall = callThunk;
            m_jitCodeForConstruct = constructThunk;
            m_jitCodeForCallWithArityCheck = callThunk.addressForCall();
            m_jitCodeForConstructWithArityCheck = constructThunk.addressForCall();
            m_intrinsic = intrinsic;
        }
#endif

    private:
        NativeExecutable(JSGlobalData& globalData, NativeFunction function, NativeFunction constructor)
            : ExecutableBase(globalData, globalData.nativeExecutableStructure.get(), NUM_PARAMETERS_IS_HOST)
            , m_function(function)
            , m_constructor(constructor)
        {
        }

        NativeFunction m_function;
        NativeFunction m_constructor;
        
        Intrinsic m_intrinsic;
    };

    class ScriptExecutable : public ExecutableBase {
    public:
        typedef ExecutableBase Base;

        ScriptExecutable(Structure* structure, JSGlobalData& globalData, const SourceCode& source, bool isInStrictContext)
            : ExecutableBase(globalData, structure, NUM_PARAMETERS_NOT_COMPILED)
            , m_source(source)
            , m_features(isInStrictContext ? StrictModeFeature : 0)
        {
        }

        ScriptExecutable(Structure* structure, ExecState* exec, const SourceCode& source, bool isInStrictContext)
            : ExecutableBase(exec->globalData(), structure, NUM_PARAMETERS_NOT_COMPILED)
            , m_source(source)
            , m_features(isInStrictContext ? StrictModeFeature : 0)
        {
        }

#if ENABLE(JIT)
        static void destroy(JSCell*);
#endif
        
        CodeBlockHash hashFor(CodeSpecializationKind) const;

        const SourceCode& source() const { return m_source; }
        intptr_t sourceID() const { return m_source.providerID(); }
        const String& sourceURL() const { return m_source.provider()->url(); }
        int lineNo() const { return m_firstLine; }
        int lastLine() const { return m_lastLine; }

        bool usesEval() const { return m_features & EvalFeature; }
        bool usesArguments() const { return m_features & ArgumentsFeature; }
        bool needsActivation() const { return m_hasCapturedVariables || m_features & (EvalFeature | WithFeature | CatchFeature); }
        bool isStrictMode() const { return m_features & StrictModeFeature; }

        void unlinkCalls();

        CodeFeatures features() const { return m_features; }
        
        static const ClassInfo s_info;

        void recordParse(CodeFeatures features, bool hasCapturedVariables, int firstLine, int lastLine)
        {
            m_features = features;
            m_hasCapturedVariables = hasCapturedVariables;
            m_firstLine = firstLine;
            m_lastLine = lastLine;
        }

    protected:
        void finishCreation(JSGlobalData& globalData)
        {
            Base::finishCreation(globalData);
            globalData.heap.addCompiledCode(this); // Balanced by Heap::deleteUnmarkedCompiledCode().

#if ENABLE(CODEBLOCK_SAMPLING)
            if (SamplingTool* sampler = globalData.interpreter->sampler())
                sampler->notifyOfScope(globalData, this);
#endif
        }

        SourceCode m_source;
        CodeFeatures m_features;
        bool m_hasCapturedVariables;
        int m_firstLine;
        int m_lastLine;
    };

    class EvalExecutable : public ScriptExecutable {
        friend class LLIntOffsetsExtractor;
    public:
        typedef ScriptExecutable Base;

        static void destroy(JSCell*);

        JSObject* compile(ExecState* exec, JSScope* scope)
        {
            ASSERT(exec->globalData().dynamicGlobalObject);
            JSObject* error = 0;
            if (!m_evalCodeBlock)
                error = compileInternal(exec, scope, JITCode::bottomTierJIT());
            ASSERT(!error == !!m_evalCodeBlock);
            return error;
        }
        
        JSObject* compileOptimized(ExecState*, JSScope*, unsigned bytecodeIndex);
        
#if ENABLE(JIT)
        void jettisonOptimizedCode(JSGlobalData&);
        bool jitCompile(ExecState*);
#endif

        EvalCodeBlock& generatedBytecode()
        {
            ASSERT(m_evalCodeBlock);
            return *m_evalCodeBlock;
        }

        static EvalExecutable* create(ExecState* exec, const SourceCode& source, bool isInStrictContext) 
        {
            EvalExecutable* executable = new (NotNull, allocateCell<EvalExecutable>(*exec->heap())) EvalExecutable(exec, source, isInStrictContext);
            executable->finishCreation(exec->globalData());
            return executable;
        }

#if ENABLE(JIT)
        JITCode& generatedJITCode()
        {
            return generatedJITCodeForCall();
        }
#endif
        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto)
        {
            return Structure::create(globalData, globalObject, proto, TypeInfo(EvalExecutableType, StructureFlags), &s_info);
        }
        
        static const ClassInfo s_info;

        void unlinkCalls();

        void clearCode();

        ExecutableInfo executableInfo() const { return ExecutableInfo(needsActivation(), usesEval(), isStrictMode(), false); }

    private:
        static const unsigned StructureFlags = OverridesVisitChildren | ScriptExecutable::StructureFlags;
        EvalExecutable(ExecState*, const SourceCode&, bool);

        JSObject* compileInternal(ExecState*, JSScope*, JITCode::JITType, unsigned bytecodeIndex = UINT_MAX);
        static void visitChildren(JSCell*, SlotVisitor&);

        OwnPtr<EvalCodeBlock> m_evalCodeBlock;
        WriteBarrier<UnlinkedEvalCodeBlock> m_unlinkedEvalCodeBlock;
    };

    class ProgramExecutable : public ScriptExecutable {
        friend class LLIntOffsetsExtractor;
    public:
        typedef ScriptExecutable Base;

        static ProgramExecutable* create(ExecState* exec, const SourceCode& source)
        {
            ProgramExecutable* executable = new (NotNull, allocateCell<ProgramExecutable>(*exec->heap())) ProgramExecutable(exec, source);
            executable->finishCreation(exec->globalData());
            return executable;
        }


        JSObject* initalizeGlobalProperties(JSGlobalData&, CallFrame*, JSScope*);

        static void destroy(JSCell*);

        JSObject* compile(ExecState* exec, JSScope* scope)
        {
            ASSERT(exec->globalData().dynamicGlobalObject);
            JSObject* error = 0;
            if (!m_programCodeBlock)
                error = compileInternal(exec, scope, JITCode::bottomTierJIT());
            ASSERT(!error == !!m_programCodeBlock);
            return error;
        }

        JSObject* compileOptimized(ExecState*, JSScope*, unsigned bytecodeIndex);
        
#if ENABLE(JIT)
        void jettisonOptimizedCode(JSGlobalData&);
        bool jitCompile(ExecState*);
#endif

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
        
        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto)
        {
            return Structure::create(globalData, globalObject, proto, TypeInfo(ProgramExecutableType, StructureFlags), &s_info);
        }
        
        static const ClassInfo s_info;
        
        void unlinkCalls();

        void clearCode();

        ExecutableInfo executableInfo() const { return ExecutableInfo(needsActivation(), usesEval(), isStrictMode(), false); }

    private:
        static const unsigned StructureFlags = OverridesVisitChildren | ScriptExecutable::StructureFlags;

        ProgramExecutable(ExecState*, const SourceCode&);

        enum ConstantMode { IsConstant, IsVariable };
        enum FunctionMode { IsFunctionToSpecialize, NotFunctionOrNotSpecializable };
        int addGlobalVar(JSGlobalObject*, const Identifier&, ConstantMode, FunctionMode);

        JSObject* compileInternal(ExecState*, JSScope*, JITCode::JITType, unsigned bytecodeIndex = UINT_MAX);
        static void visitChildren(JSCell*, SlotVisitor&);

        WriteBarrier<UnlinkedProgramCodeBlock> m_unlinkedProgramCodeBlock;
        OwnPtr<ProgramCodeBlock> m_programCodeBlock;
    };

    class FunctionExecutable : public ScriptExecutable {
        friend class JIT;
        friend class LLIntOffsetsExtractor;
    public:
        typedef ScriptExecutable Base;

        static FunctionExecutable* create(JSGlobalData& globalData, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, unsigned firstLine, unsigned lastLine)
        {
            FunctionExecutable* executable = new (NotNull, allocateCell<FunctionExecutable>(globalData.heap)) FunctionExecutable(globalData, source, unlinkedExecutable, firstLine, lastLine);
            executable->finishCreation(globalData);
            return executable;
        }
        static FunctionExecutable* fromGlobalCode(const Identifier& name, ExecState*, Debugger*, const SourceCode&, JSObject** exception);

        static void destroy(JSCell*);
        
        UnlinkedFunctionExecutable* unlinkedExecutable()
        {
            return m_unlinkedExecutable.get();
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
        
        PassOwnPtr<FunctionCodeBlock> produceCodeBlockFor(JSScope*, CodeSpecializationKind, JSObject*& exception);

        JSObject* compileForCall(ExecState* exec, JSScope* scope)
        {
            ASSERT(exec->globalData().dynamicGlobalObject);
            JSObject* error = 0;
            if (!m_codeBlockForCall)
                error = compileForCallInternal(exec, scope, JITCode::bottomTierJIT());
            ASSERT(!error == !!m_codeBlockForCall);
            return error;
        }

        JSObject* compileOptimizedForCall(ExecState*, JSScope*, unsigned bytecodeIndex);
        
#if ENABLE(JIT)
        void jettisonOptimizedCodeForCall(JSGlobalData&);
        bool jitCompileForCall(ExecState*);
#endif

        bool isGeneratedForCall() const
        {
            return m_codeBlockForCall;
        }

        FunctionCodeBlock& generatedBytecodeForCall()
        {
            ASSERT(m_codeBlockForCall);
            return *m_codeBlockForCall;
        }

        JSObject* compileForConstruct(ExecState* exec, JSScope* scope)
        {
            ASSERT(exec->globalData().dynamicGlobalObject);
            JSObject* error = 0;
            if (!m_codeBlockForConstruct)
                error = compileForConstructInternal(exec, scope, JITCode::bottomTierJIT());
            ASSERT(!error == !!m_codeBlockForConstruct);
            return error;
        }

        JSObject* compileOptimizedForConstruct(ExecState*, JSScope*, unsigned bytecodeIndex);
        
#if ENABLE(JIT)
        void jettisonOptimizedCodeForConstruct(JSGlobalData&);
        bool jitCompileForConstruct(ExecState*);
#endif

        bool isGeneratedForConstruct() const
        {
            return m_codeBlockForConstruct;
        }

        FunctionCodeBlock& generatedBytecodeForConstruct()
        {
            ASSERT(m_codeBlockForConstruct);
            return *m_codeBlockForConstruct;
        }
        
        JSObject* compileFor(ExecState* exec, JSScope* scope, CodeSpecializationKind kind)
        {
            ASSERT(exec->callee());
            ASSERT(exec->callee()->inherits(&JSFunction::s_info));
            ASSERT(jsCast<JSFunction*>(exec->callee())->jsExecutable() == this);

            if (kind == CodeForCall)
                return compileForCall(exec, scope);
            ASSERT(kind == CodeForConstruct);
            return compileForConstruct(exec, scope);
        }
        
        JSObject* compileOptimizedFor(ExecState* exec, JSScope* scope, unsigned bytecodeIndex, CodeSpecializationKind kind)
        {
            ASSERT(exec->callee());
            ASSERT(exec->callee()->inherits(&JSFunction::s_info));
            ASSERT(jsCast<JSFunction*>(exec->callee())->jsExecutable() == this);
            
            if (kind == CodeForCall)
                return compileOptimizedForCall(exec, scope, bytecodeIndex);
            ASSERT(kind == CodeForConstruct);
            return compileOptimizedForConstruct(exec, scope, bytecodeIndex);
        }
        
#if ENABLE(JIT)
        void jettisonOptimizedCodeFor(JSGlobalData& globalData, CodeSpecializationKind kind)
        {
            if (kind == CodeForCall) 
                jettisonOptimizedCodeForCall(globalData);
            else {
                ASSERT(kind == CodeForConstruct);
                jettisonOptimizedCodeForConstruct(globalData);
            }
        }
        
        bool jitCompileFor(ExecState* exec, CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return jitCompileForCall(exec);
            ASSERT(kind == CodeForConstruct);
            return jitCompileForConstruct(exec);
        }
#endif
        
        bool isGeneratedFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return isGeneratedForCall();
            ASSERT(kind == CodeForConstruct);
            return isGeneratedForConstruct();
        }
        
        FunctionCodeBlock& generatedBytecodeFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return generatedBytecodeForCall();
            ASSERT(kind == CodeForConstruct);
            return generatedBytecodeForConstruct();
        }

        FunctionCodeBlock* baselineCodeBlockFor(CodeSpecializationKind);
        
        FunctionCodeBlock* profiledCodeBlockFor(CodeSpecializationKind kind)
        {
            return baselineCodeBlockFor(kind);
        }
        
        const Identifier& name() { return m_unlinkedExecutable->name(); }
        const Identifier& inferredName() { return m_unlinkedExecutable->inferredName(); }
        JSString* nameValue() const { return m_unlinkedExecutable->nameValue(); }
        size_t parameterCount() const { return m_unlinkedExecutable->parameterCount(); } // Excluding 'this'!
        String paramString() const;
        SharedSymbolTable* symbolTable(CodeSpecializationKind kind) const { return m_unlinkedExecutable->symbolTable(kind); }

        void clearCodeIfNotCompiling();
        void clearUnlinkedCodeForRecompilationIfNotCompiling();
        static void visitChildren(JSCell*, SlotVisitor&);
        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue proto)
        {
            return Structure::create(globalData, globalObject, proto, TypeInfo(FunctionExecutableType, StructureFlags), &s_info);
        }
        
        static const ClassInfo s_info;
        
        void unlinkCalls();

        void clearCode();

    private:
        FunctionExecutable(JSGlobalData&, const SourceCode&, UnlinkedFunctionExecutable*, unsigned firstLine, unsigned lastLine);

        JSObject* compileForCallInternal(ExecState*, JSScope*, JITCode::JITType, unsigned bytecodeIndex = UINT_MAX);
        JSObject* compileForConstructInternal(ExecState*, JSScope*, JITCode::JITType, unsigned bytecodeIndex = UINT_MAX);
        
        OwnPtr<FunctionCodeBlock>& codeBlockFor(CodeSpecializationKind kind)
        {
            if (kind == CodeForCall)
                return m_codeBlockForCall;
            ASSERT(kind == CodeForConstruct);
            return m_codeBlockForConstruct;
        }
 
        bool isCompiling()
        {
#if ENABLE(JIT)
            if (!m_jitCodeForCall && m_codeBlockForCall)
                return true;
            if (!m_jitCodeForConstruct && m_codeBlockForConstruct)
                return true;
#endif
            return false;
        }

        static const unsigned StructureFlags = OverridesVisitChildren | ScriptExecutable::StructureFlags;
        WriteBarrier<UnlinkedFunctionExecutable> m_unlinkedExecutable;
        OwnPtr<FunctionCodeBlock> m_codeBlockForCall;
        OwnPtr<FunctionCodeBlock> m_codeBlockForConstruct;
    };

    inline JSFunction::JSFunction(JSGlobalData& globalData, FunctionExecutable* executable, JSScope* scope)
        : Base(globalData, scope->globalObject()->functionStructure())
        , m_executable(globalData, this, executable)
        , m_scope(globalData, this, scope)
        , m_inheritorIDWatchpoint(InitializedBlind) // See comment in JSFunction.cpp concerning the reason for using InitializedBlind as opposed to InitializedWatching.
    {
    }

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

    inline NativeFunction JSFunction::nativeFunction()
    {
        ASSERT(isHostFunction());
        return static_cast<NativeExecutable*>(m_executable.get())->function();
    }

    inline NativeFunction JSFunction::nativeConstructor()
    {
        ASSERT(isHostFunction());
        return static_cast<NativeExecutable*>(m_executable.get())->constructor();
    }

    inline bool isHostFunction(JSValue value, NativeFunction nativeFunction)
    {
        JSFunction* function = jsCast<JSFunction*>(getJSFunction(value));
        if (!function || !function->isHostFunction())
            return false;
        return function->nativeFunction() == nativeFunction;
    }

    inline void ExecutableBase::clearCodeVirtual(ExecutableBase* executable)
    {
        switch (executable->structure()->typeInfo().type()) {
        case EvalExecutableType:
            return jsCast<EvalExecutable*>(executable)->clearCode();
        case ProgramExecutableType:
            return jsCast<ProgramExecutable*>(executable)->clearCode();
        case FunctionExecutableType:
            return jsCast<FunctionExecutable*>(executable)->clearCode();
        default:
            return jsCast<NativeExecutable*>(executable)->clearCode();
        }
    }

    inline void ScriptExecutable::unlinkCalls()
    {
        switch (structure()->typeInfo().type()) {
        case EvalExecutableType:
            return jsCast<EvalExecutable*>(this)->unlinkCalls();
        case ProgramExecutableType:
            return jsCast<ProgramExecutable*>(this)->unlinkCalls();
        case FunctionExecutableType:
            return jsCast<FunctionExecutable*>(this)->unlinkCalls();
        default:
            ASSERT_NOT_REACHED();
        }
    }

}

#endif
