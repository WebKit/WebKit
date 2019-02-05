/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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
#include "SourceCode.h"
#include <wtf/Box.h>
#include <wtf/Markable.h>

namespace JSC {

class FunctionExecutable final : public ScriptExecutable {
    friend class JIT;
    friend class LLIntOffsetsExtractor;
public:
    typedef ScriptExecutable Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.functionExecutableSpace.space;
    }

    static FunctionExecutable* create(
        VM& vm, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, 
        unsigned lastLine, unsigned endColumn, Intrinsic intrinsic)
    {
        FunctionExecutable* executable = new (NotNull, allocateCell<FunctionExecutable>(vm.heap)) FunctionExecutable(vm, source, unlinkedExecutable, lastLine, endColumn, intrinsic);
        executable->finishCreation(vm);
        return executable;
    }
    static FunctionExecutable* fromGlobalCode(
        const Identifier& name, ExecState&, const SourceCode&, 
        JSObject*& exception, int overrideLineNumber, Optional<int> functionConstructorParametersEndPosition);

    static void destroy(JSCell*);
        
    UnlinkedFunctionExecutable* unlinkedExecutable() const
    {
        return m_unlinkedExecutable.get();
    }

    // Returns either call or construct bytecode. This can be appropriate
    // for answering questions that that don't vary between call and construct --
    // for example, argumentsRegister().
    FunctionCodeBlock* eitherCodeBlock()
    {
        ExecutableToCodeBlockEdge* edge;
        if (m_codeBlockForCall)
            edge = m_codeBlockForCall.get();
        else
            edge = m_codeBlockForConstruct.get();
        return bitwise_cast<FunctionCodeBlock*>(ExecutableToCodeBlockEdge::unwrap(edge));
    }
        
    bool isGeneratedForCall() const
    {
        return !!m_codeBlockForCall;
    }

    FunctionCodeBlock* codeBlockForCall()
    {
        return bitwise_cast<FunctionCodeBlock*>(ExecutableToCodeBlockEdge::unwrap(m_codeBlockForCall.get()));
    }

    bool isGeneratedForConstruct() const
    {
        return !!m_codeBlockForConstruct;
    }

    FunctionCodeBlock* codeBlockForConstruct()
    {
        return bitwise_cast<FunctionCodeBlock*>(ExecutableToCodeBlockEdge::unwrap(m_codeBlockForConstruct.get()));
    }
        
    bool isGeneratedFor(CodeSpecializationKind kind)
    {
        if (kind == CodeForCall)
            return isGeneratedForCall();
        ASSERT(kind == CodeForConstruct);
        return isGeneratedForConstruct();
    }
        
    FunctionCodeBlock* codeBlockFor(CodeSpecializationKind kind)
    {
        if (kind == CodeForCall)
            return codeBlockForCall();
        ASSERT(kind == CodeForConstruct);
        return codeBlockForConstruct();
    }

    FunctionCodeBlock* baselineCodeBlockFor(CodeSpecializationKind);
        
    FunctionCodeBlock* profiledCodeBlockFor(CodeSpecializationKind kind)
    {
        return baselineCodeBlockFor(kind);
    }

    RefPtr<TypeSet> returnStatementTypeSet() 
    {
        RareData& rareData = ensureRareData();
        if (!rareData.m_returnStatementTypeSet)
            rareData.m_returnStatementTypeSet = TypeSet::create();
        return rareData.m_returnStatementTypeSet;
    }
        
    FunctionMode functionMode() { return m_unlinkedExecutable->functionMode(); }
    bool isBuiltinFunction() const { return m_unlinkedExecutable->isBuiltinFunction(); }
    ConstructAbility constructAbility() const { return m_unlinkedExecutable->constructAbility(); }
    bool isClass() const { return !classSource().isNull(); }
    bool isArrowFunction() const { return parseMode() == SourceParseMode::ArrowFunctionMode; }
    bool isGetter() const { return parseMode() == SourceParseMode::GetterMode; }
    bool isSetter() const { return parseMode() == SourceParseMode::SetterMode; }
    bool isGenerator() const { return isGeneratorParseMode(parseMode()); }
    bool isAsyncGenerator() const { return isAsyncGeneratorParseMode(parseMode()); }
    bool isMethod() const { return parseMode() == SourceParseMode::MethodMode; }
    bool hasCallerAndArgumentsProperties() const
    {
        // Per https://tc39.github.io/ecma262/#sec-forbidden-extensions, only sloppy-mode non-builtin functions in old-style (pre-ES6) syntactic forms can contain
        // "caller" and "arguments".
        return !isStrictMode() && parseMode() == SourceParseMode::NormalFunctionMode && !isClassConstructorFunction();
    }
    bool hasPrototypeProperty() const
    {
        return SourceParseModeSet(
            SourceParseMode::NormalFunctionMode,
            SourceParseMode::GeneratorBodyMode,
            SourceParseMode::GeneratorWrapperFunctionMode,
            SourceParseMode::GeneratorWrapperMethodMode,
            SourceParseMode::AsyncGeneratorWrapperFunctionMode,
            SourceParseMode::AsyncGeneratorWrapperMethodMode,
            SourceParseMode::AsyncGeneratorBodyMode
        ).contains(parseMode()) || isClass();
    }
    DerivedContextType derivedContextType() const { return m_unlinkedExecutable->derivedContextType(); }
    bool isClassConstructorFunction() const { return m_unlinkedExecutable->isClassConstructorFunction(); }
    const Identifier& name() { return m_unlinkedExecutable->name(); }
    const Identifier& ecmaName() { return m_unlinkedExecutable->ecmaName(); }
    const Identifier& inferredName() { return m_unlinkedExecutable->inferredName(); }
    unsigned parameterCount() const { return m_unlinkedExecutable->parameterCount(); } // Excluding 'this'!
    SourceParseMode parseMode() const { return m_unlinkedExecutable->parseMode(); }
    JSParserScriptMode scriptMode() const { return m_unlinkedExecutable->scriptMode(); }
    const SourceCode& classSource() const { return m_unlinkedExecutable->classSource(); }

    static void visitChildren(JSCell*, SlotVisitor&);
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(FunctionExecutableType, StructureFlags), info());
    }

    void setOverrideLineNumber(int overrideLineNumber)
    {
        if (overrideLineNumber == -1) {
            if (UNLIKELY(m_rareData))
                m_rareData->m_overrideLineNumber = WTF::nullopt;
            return;
        }
        ensureRareData().m_overrideLineNumber = overrideLineNumber;
    }

    Optional<int> overrideLineNumber() const
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_overrideLineNumber;
        return WTF::nullopt;
    }

    unsigned typeProfilingStartOffset(VM&) const
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_typeProfilingStartOffset;
        return m_unlinkedExecutable->typeProfilingStartOffset();
    }

    unsigned typeProfilingEndOffset(VM&) const
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_typeProfilingEndOffset;
        return m_unlinkedExecutable->typeProfilingEndOffset();
    }

    unsigned parametersStartOffset() const
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_parametersStartOffset;
        return m_unlinkedExecutable->parametersStartOffset();
    }

    void overrideParameterAndTypeProfilingStartEndOffsets(unsigned parametersStartOffset, unsigned typeProfilingStartOffset, unsigned typeProfilingEndOffset)
    {
        auto& rareData = ensureRareData();
        rareData.m_parametersStartOffset = parametersStartOffset;
        rareData.m_typeProfilingStartOffset = typeProfilingStartOffset;
        rareData.m_typeProfilingEndOffset = typeProfilingEndOffset;
    }

    DECLARE_INFO;

    InferredValue* singletonFunction()
    {
        if (VM::canUseJIT())
            return m_singletonFunction.get();
        return nullptr;
    }

    void notifyCreation(VM& vm, JSValue value, const char* reason)
    {
        if (VM::canUseJIT()) {
            singletonFunction()->notifyWrite(vm, value, reason);
            return;
        }
        switch (m_singletonFunctionState) {
        case ClearWatchpoint:
            m_singletonFunctionState = IsWatched;
            return;
        case IsWatched:
            m_singletonFunctionState = IsInvalidated;
            return;
        case IsInvalidated:
            return;
        }
    }

    bool singletonFunctionHasBeenInvalidated()
    {
        if (VM::canUseJIT())
            return singletonFunction()->hasBeenInvalidated();
        return m_singletonFunctionState == IsInvalidated;
    }

    // Cached poly proto structure for the result of constructing this executable.
    Structure* cachedPolyProtoStructure() { return m_cachedPolyProtoStructure.get(); }
    void setCachedPolyProtoStructure(VM& vm, Structure* structure) { m_cachedPolyProtoStructure.set(vm, this, structure); }

    InlineWatchpointSet& ensurePolyProtoWatchpoint()
    {
        if (!m_polyProtoWatchpoint)
            m_polyProtoWatchpoint = Box<InlineWatchpointSet>::create(IsWatched);
        return *m_polyProtoWatchpoint;
    }

    Box<InlineWatchpointSet> sharedPolyProtoWatchpoint() const { return m_polyProtoWatchpoint; }

private:
    friend class ExecutableBase;
    FunctionExecutable(
        VM&, const SourceCode&, UnlinkedFunctionExecutable*,
        unsigned lastLine, unsigned endColumn, Intrinsic);
    
    void finishCreation(VM&);

    friend class ScriptExecutable;

    struct RareData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        Markable<int, IntegralMarkableTraits<int, -1>> m_overrideLineNumber;
        unsigned m_parametersStartOffset { 0 };
        unsigned m_typeProfilingStartOffset { UINT_MAX };
        unsigned m_typeProfilingEndOffset { UINT_MAX };
        RefPtr<TypeSet> m_returnStatementTypeSet;
    };

    RareData& ensureRareData()
    {
        if (LIKELY(m_rareData))
            return *m_rareData;
        return ensureRareDataSlow();
    }
    RareData& ensureRareDataSlow();

    std::unique_ptr<RareData> m_rareData;
    WriteBarrier<UnlinkedFunctionExecutable> m_unlinkedExecutable;
    WriteBarrier<ExecutableToCodeBlockEdge> m_codeBlockForCall;
    WriteBarrier<ExecutableToCodeBlockEdge> m_codeBlockForConstruct;
    union {
        WriteBarrier<InferredValue> m_singletonFunction;
        WatchpointState m_singletonFunctionState;
    };
    WriteBarrier<Structure> m_cachedPolyProtoStructure;
    Box<InlineWatchpointSet> m_polyProtoWatchpoint;
};

} // namespace JSC
