/*
 * Copyright (C) 2009-2019 Apple Inc. All rights reserved.
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

struct FunctionOverrideInfo;

class FunctionExecutable final : public ScriptExecutable {
    friend class JIT;
    friend class LLIntOffsetsExtractor;
public:
    typedef ScriptExecutable Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    template<typename CellType, SubspaceAccess>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return &vm.functionExecutableSpace.space;
    }

    static FunctionExecutable* create(VM& vm, ScriptExecutable* topLevelExecutable, const SourceCode& source, UnlinkedFunctionExecutable* unlinkedExecutable, Intrinsic intrinsic)
    {
        FunctionExecutable* executable = new (NotNull, allocateCell<FunctionExecutable>(vm.heap)) FunctionExecutable(vm, source, unlinkedExecutable, intrinsic);
        executable->finishCreation(vm, topLevelExecutable);
        return executable;
    }
    static FunctionExecutable* fromGlobalCode(
        const Identifier& name, JSGlobalObject*, const SourceCode&, 
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
    bool isClass() const { return m_unlinkedExecutable->isClass(); }
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
    unsigned parameterCount() const { return m_unlinkedExecutable->parameterCount(); } // Excluding 'this'!
    SourceParseMode parseMode() const { return m_unlinkedExecutable->parseMode(); }
    JSParserScriptMode scriptMode() const { return m_unlinkedExecutable->scriptMode(); }
    SourceCode classSource() const { return m_unlinkedExecutable->classSource(); }

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

    int lineCount() const
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_lineCount;
        return m_unlinkedExecutable->lineCount();
    }

    int endColumn() const
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_endColumn;
        return m_unlinkedExecutable->linkedEndColumn(m_source.startColumn().oneBasedInt());
    }

    int firstLine() const
    {
        return source().firstLine().oneBasedInt();
    }

    int lastLine() const
    {
        return firstLine() + lineCount();
    }

    unsigned typeProfilingStartOffset(VM&) const
    {
        return typeProfilingStartOffset();
    }

    unsigned typeProfilingStartOffset() const
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_typeProfilingStartOffset;
        return m_unlinkedExecutable->typeProfilingStartOffset();
    }

    unsigned typeProfilingEndOffset(VM&) const
    {
        return typeProfilingEndOffset();
    }

    unsigned typeProfilingEndOffset() const
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

    void overrideInfo(const FunctionOverrideInfo&);

    DECLARE_INFO;

    InferredValue<JSFunction>& singleton()
    {
        return m_singleton;
    }

    void notifyCreation(VM& vm, JSFunction* function, const char* reason)
    {
        m_singleton.notifyWrite(vm, this, function, reason);
    }

    // Cached poly proto structure for the result of constructing this executable.
    Structure* cachedPolyProtoStructure()
    {
        if (UNLIKELY(m_rareData))
            return m_rareData->m_cachedPolyProtoStructure.get();
        return nullptr;
    }
    void setCachedPolyProtoStructure(VM& vm, Structure* structure)
    {
        ensureRareData().m_cachedPolyProtoStructure.set(vm, this, structure);
    }

    InlineWatchpointSet& ensurePolyProtoWatchpoint()
    {
        if (!m_polyProtoWatchpoint)
            m_polyProtoWatchpoint = Box<InlineWatchpointSet>::create(IsWatched);
        return *m_polyProtoWatchpoint;
    }

    Box<InlineWatchpointSet> sharedPolyProtoWatchpoint() const { return m_polyProtoWatchpoint; }

    ScriptExecutable* topLevelExecutable() const { return m_topLevelExecutable.get(); }

    TemplateObjectMap& ensureTemplateObjectMap(VM&);

    void finalizeUnconditionally(VM&);

private:
    friend class ExecutableBase;
    FunctionExecutable(VM&, const SourceCode&, UnlinkedFunctionExecutable*, Intrinsic);
    
    void finishCreation(VM&, ScriptExecutable* topLevelExecutable);

    friend class ScriptExecutable;

    struct RareData {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        RefPtr<TypeSet> m_returnStatementTypeSet;
        unsigned m_lineCount;
        unsigned m_endColumn;
        Markable<int, IntegralMarkableTraits<int, -1>> m_overrideLineNumber;
        unsigned m_parametersStartOffset { 0 };
        unsigned m_typeProfilingStartOffset { UINT_MAX };
        unsigned m_typeProfilingEndOffset { UINT_MAX };
        std::unique_ptr<TemplateObjectMap> m_templateObjectMap;
        WriteBarrier<Structure> m_cachedPolyProtoStructure;
    };

    RareData& ensureRareData()
    {
        if (LIKELY(m_rareData))
            return *m_rareData;
        return ensureRareDataSlow();
    }
    RareData& ensureRareDataSlow();

    // FIXME: We can merge rareData pointer and top-level executable pointer. First time, setting parent.
    // If RareData is required, materialize RareData, swap it, and store top-level executable pointer inside RareData.
    // https://bugs.webkit.org/show_bug.cgi?id=197625
    std::unique_ptr<RareData> m_rareData;
    WriteBarrier<ScriptExecutable> m_topLevelExecutable;
    WriteBarrier<UnlinkedFunctionExecutable> m_unlinkedExecutable;
    WriteBarrier<ExecutableToCodeBlockEdge> m_codeBlockForCall;
    WriteBarrier<ExecutableToCodeBlockEdge> m_codeBlockForConstruct;
    InferredValue<JSFunction> m_singleton;
    Box<InlineWatchpointSet> m_polyProtoWatchpoint;
};

} // namespace JSC
