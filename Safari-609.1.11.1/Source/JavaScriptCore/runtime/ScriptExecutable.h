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

#include "ExecutableBase.h"

namespace JSC {

class JSArray;
class JSTemplateObjectDescriptor;
class IsoCellSet;

class ScriptExecutable : public ExecutableBase {
public:
    typedef ExecutableBase Base;
    static constexpr unsigned StructureFlags = Base::StructureFlags;

    static void destroy(JSCell*);

    using TemplateObjectMap = HashMap<uint64_t, WriteBarrier<JSArray>, WTF::IntHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>>;
        
    CodeBlockHash hashFor(CodeSpecializationKind) const;

    const SourceCode& source() const { return m_source; }
    intptr_t sourceID() const { return m_source.providerID(); }
    const SourceOrigin& sourceOrigin() const { return m_source.provider()->sourceOrigin(); }
    const String& sourceURL() const { return m_source.provider()->url(); }
    int firstLine() const { return m_source.firstLine().oneBasedInt(); }
    JS_EXPORT_PRIVATE int lastLine() const;
    unsigned startColumn() const { return m_source.startColumn().oneBasedInt(); }
    JS_EXPORT_PRIVATE unsigned endColumn() const;

    Optional<int> overrideLineNumber(VM&) const;
    unsigned typeProfilingStartOffset(VM&) const;
    unsigned typeProfilingEndOffset(VM&) const;

    bool usesEval() const { return m_features & EvalFeature; }
    bool usesArguments() const { return m_features & ArgumentsFeature; }
    bool isArrowFunctionContext() const { return m_isArrowFunctionContext; }
    bool isStrictMode() const { return m_features & StrictModeFeature; }
    DerivedContextType derivedContextType() const { return static_cast<DerivedContextType>(m_derivedContextType); }
    EvalContextType evalContextType() const { return static_cast<EvalContextType>(m_evalContextType); }

    ECMAMode ecmaMode() const { return isStrictMode() ? StrictMode : NotStrictMode; }
        
    void setNeverInline(bool value) { m_neverInline = value; }
    void setNeverOptimize(bool value) { m_neverOptimize = value; }
    void setNeverFTLOptimize(bool value) { m_neverFTLOptimize = value; }
    void setDidTryToEnterInLoop(bool value) { m_didTryToEnterInLoop = value; }
    void setCanUseOSRExitFuzzing(bool value) { m_canUseOSRExitFuzzing = value; }
    bool neverInline() const { return m_neverInline; }
    bool neverOptimize() const { return m_neverOptimize; }
    bool neverFTLOptimize() const { return m_neverFTLOptimize; }
    bool didTryToEnterInLoop() const { return m_didTryToEnterInLoop; }
    bool isInliningCandidate() const { return !neverInline(); }
    bool isOkToOptimize() const { return !neverOptimize(); }
    bool canUseOSRExitFuzzing() const { return m_canUseOSRExitFuzzing; }
    
    bool* addressOfDidTryToEnterInLoop() { return &m_didTryToEnterInLoop; }

    CodeFeatures features() const { return m_features; }
        
    DECLARE_EXPORT_INFO;

    void recordParse(CodeFeatures, bool hasCapturedVariables, int lastLine, unsigned endColumn);
    void installCode(CodeBlock*);
    void installCode(VM&, CodeBlock*, CodeType, CodeSpecializationKind);
    CodeBlock* newCodeBlockFor(CodeSpecializationKind, JSFunction*, JSScope*, Exception*&);
    CodeBlock* newReplacementCodeBlockFor(CodeSpecializationKind);

    void clearCode(IsoCellSet&);

    Intrinsic intrinsic() const
    {
        return m_intrinsic;
    }

    bool hasJITCodeForCall() const
    {
        return m_jitCodeForCall;
    }
    bool hasJITCodeForConstruct() const
    {
        return m_jitCodeForConstruct;
    }

    // This function has an interesting GC story. Callers of this function are asking us to create a CodeBlock
    // that is not jettisoned before this function returns. Callers are essentially asking for a strong reference
    // to the CodeBlock. Because the Executable may be allocating the CodeBlock, we require callers to pass in
    // their CodeBlock*& reference because it's safe for CodeBlock to be jettisoned if Executable is the only thing
    // to point to it. This forces callers to have a CodeBlock* in a register or on the stack that will be marked
    // by conservative GC if a GC happens after we create the CodeBlock.
    template <typename ExecutableType>
    Exception* prepareForExecution(VM&, JSFunction*, JSScope*, CodeSpecializationKind, CodeBlock*& resultCodeBlock);

    ScriptExecutable* topLevelExecutable();
    JSArray* createTemplateObject(JSGlobalObject*, JSTemplateObjectDescriptor*);

private:
    friend class ExecutableBase;
    Exception* prepareForExecutionImpl(VM&, JSFunction*, JSScope*, CodeSpecializationKind, CodeBlock*&);

    bool hasClearableCode(VM&) const;

    TemplateObjectMap& ensureTemplateObjectMap(VM&);

protected:
    ScriptExecutable(Structure*, VM&, const SourceCode&, bool isInStrictContext, DerivedContextType, bool isInArrowFunctionContext, EvalContextType, Intrinsic);

    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);

#if ENABLE(CODEBLOCK_SAMPLING)
        if (SamplingTool* sampler = vm.interpreter->sampler())
            sampler->notifyOfScope(vm, this);
#endif
    }

    void recordParse(CodeFeatures features, bool hasCapturedVariables)
    {
        m_features = features;
        m_hasCapturedVariables = hasCapturedVariables;
    }

    static TemplateObjectMap& ensureTemplateObjectMapImpl(std::unique_ptr<TemplateObjectMap>& dest);

    SourceCode m_source;
    Intrinsic m_intrinsic { NoIntrinsic };
    bool m_didTryToEnterInLoop { false };
    CodeFeatures m_features;
    OptionSet<CodeGenerationMode> m_codeGenerationModeForGeneratorBody;
    bool m_hasCapturedVariables : 1;
    bool m_neverInline : 1;
    bool m_neverOptimize : 1;
    bool m_neverFTLOptimize : 1;
    bool m_isArrowFunctionContext : 1;
    bool m_canUseOSRExitFuzzing : 1;
    bool m_codeForGeneratorBodyWasGenerated : 1;
    unsigned m_derivedContextType : 2; // DerivedContextType
    unsigned m_evalContextType : 2; // EvalContextType
};

} // namespace JSC
