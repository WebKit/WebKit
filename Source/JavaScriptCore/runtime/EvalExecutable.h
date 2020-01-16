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
#include "GlobalExecutable.h"
#include "UnlinkedEvalCodeBlock.h"

namespace JSC {

class EvalExecutable : public GlobalExecutable {
    friend class LLIntOffsetsExtractor;
public:
    using Base = GlobalExecutable;
    static constexpr unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static void destroy(JSCell*);
    
    EvalCodeBlock* codeBlock()
    {
        return bitwise_cast<EvalCodeBlock*>(ExecutableToCodeBlockEdge::unwrap(m_evalCodeBlock.get()));
    }

    Ref<JITCode> generatedJITCode()
    {
        return generatedJITCodeForCall();
    }

    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(EvalExecutableType, StructureFlags), info());
    }

    template<typename CellType, SubspaceAccess mode>
    static IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.evalExecutableSpace<mode>();
    }

    DECLARE_INFO;

    ExecutableInfo executableInfo() const { return ExecutableInfo(usesEval(), isStrictMode(), false, false, ConstructorKind::None, JSParserScriptMode::Classic, SuperBinding::NotNeeded, SourceParseMode::ProgramMode, derivedContextType(), needsClassFieldInitializer(), isArrowFunctionContext(), false, evalContextType()); }

    unsigned numVariables() { return m_unlinkedEvalCodeBlock->numVariables(); }
    unsigned numFunctionHoistingCandidates() { return m_unlinkedEvalCodeBlock->numFunctionHoistingCandidates(); }
    unsigned numTopLevelFunctionDecls() { return m_unlinkedEvalCodeBlock->numberOfFunctionDecls(); }
    bool allowDirectEvalCache() const { return m_unlinkedEvalCodeBlock->allowDirectEvalCache(); }
    NeedsClassFieldInitializer needsClassFieldInitializer() const { return static_cast<NeedsClassFieldInitializer>(m_needsClassFieldInitializer); }
    TemplateObjectMap& ensureTemplateObjectMap(VM&);

protected:
    friend class ExecutableBase;
    friend class ScriptExecutable;

    using Base::finishCreation;
    EvalExecutable(JSGlobalObject*, const SourceCode&, bool inStrictContext, DerivedContextType, bool isArrowFunctionContext, EvalContextType, NeedsClassFieldInitializer);

    static void visitChildren(JSCell*, SlotVisitor&);

    unsigned m_needsClassFieldInitializer : 1;

    WriteBarrier<ExecutableToCodeBlockEdge> m_evalCodeBlock;
    WriteBarrier<UnlinkedEvalCodeBlock> m_unlinkedEvalCodeBlock;
    std::unique_ptr<TemplateObjectMap> m_templateObjectMap;
};

} // namespace JSC
