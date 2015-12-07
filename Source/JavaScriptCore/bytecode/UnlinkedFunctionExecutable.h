/*
 * Copyright (C) 2012-2015 Apple Inc. All Rights Reserved.
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

#ifndef UnlinkedFunctionExecutable_h
#define UnlinkedFunctionExecutable_h

#include "BytecodeConventions.h"
#include "CodeSpecializationKind.h"
#include "CodeType.h"
#include "ConstructAbility.h"
#include "ExpressionRangeInfo.h"
#include "GeneratorThisMode.h"
#include "HandlerInfo.h"
#include "Identifier.h"
#include "JSCell.h"
#include "JSString.h"
#include "ParserModes.h"
#include "RegExp.h"
#include "SpecialPointer.h"
#include "VariableEnvironment.h"
#include "VirtualRegister.h"
#include <wtf/RefCountedArray.h>
#include <wtf/Vector.h>

namespace JSC {

class FunctionMetadataNode;
class FunctionExecutable;
class ParserError;
class SourceCode;
class SourceProvider;
class UnlinkedFunctionCodeBlock;

enum UnlinkedFunctionKind {
    UnlinkedNormalFunction,
    UnlinkedBuiltinFunction,
};

class UnlinkedFunctionExecutable final : public JSCell {
public:
    friend class CodeCache;
    friend class VM;

    typedef JSCell Base;
    static const unsigned StructureFlags = Base::StructureFlags | StructureIsImmortal;

    static UnlinkedFunctionExecutable* create(VM* vm, const SourceCode& source, FunctionMetadataNode* node, UnlinkedFunctionKind unlinkedFunctionKind, ConstructAbility constructAbility, GeneratorThisMode generatorThisMode, VariableEnvironment& parentScopeTDZVariables, RefPtr<SourceProvider>&& sourceOverride = nullptr)
    {
        UnlinkedFunctionExecutable* instance = new (NotNull, allocateCell<UnlinkedFunctionExecutable>(vm->heap))
            UnlinkedFunctionExecutable(vm, vm->unlinkedFunctionExecutableStructure.get(), source, WTF::move(sourceOverride), node, unlinkedFunctionKind, constructAbility, generatorThisMode, parentScopeTDZVariables);
        instance->finishCreation(*vm);
        return instance;
    }

    const Identifier& name() const { return m_name; }
    const Identifier& inferredName() const { return m_inferredName; }
    JSString* nameValue() const { return m_nameValue.get(); }
    void setNameValue(VM& vm, JSString* nameValue) { m_nameValue.set(vm, this, nameValue); }
    unsigned parameterCount() const { return m_parameterCount; };
    SourceParseMode parseMode() const { return m_parseMode; };
    bool isInStrictContext() const { return m_isInStrictContext; }
    FunctionMode functionMode() const { return static_cast<FunctionMode>(m_functionMode); }
    ConstructorKind constructorKind() const { return static_cast<ConstructorKind>(m_constructorKind); }
    GeneratorThisMode generatorThisMode() const { return static_cast<GeneratorThisMode>(m_generatorThisMode); }
    SuperBinding superBinding() const { return static_cast<SuperBinding>(m_superBinding); }

    unsigned unlinkedFunctionNameStart() const { return m_unlinkedFunctionNameStart; }
    unsigned unlinkedBodyStartColumn() const { return m_unlinkedBodyStartColumn; }
    unsigned unlinkedBodyEndColumn() const { return m_unlinkedBodyEndColumn; }
    unsigned startOffset() const { return m_startOffset; }
    unsigned sourceLength() { return m_sourceLength; }
    unsigned parametersStartOffset() const { return m_parametersStartOffset; }
    unsigned typeProfilingStartOffset() const { return m_typeProfilingStartOffset; }
    unsigned typeProfilingEndOffset() const { return m_typeProfilingEndOffset; }
    void setInvalidTypeProfilingOffsets();

    UnlinkedFunctionCodeBlock* unlinkedCodeBlockFor(
        VM&, const SourceCode&, CodeSpecializationKind, DebuggerMode, ProfilerMode, 
        ParserError&, SourceParseMode);

    static UnlinkedFunctionExecutable* fromGlobalCode(
        const Identifier&, ExecState&, const SourceCode&, JSObject*& exception, 
        int overrideLineNumber);

    JS_EXPORT_PRIVATE FunctionExecutable* link(VM&, const SourceCode&, int overrideLineNumber = -1);

    void clearCode()
    {
        m_unlinkedCodeBlockForCall.clear();
        m_unlinkedCodeBlockForConstruct.clear();
    }

    void recordParse(CodeFeatures features, bool hasCapturedVariables)
    {
        m_features = features;
        m_hasCapturedVariables = hasCapturedVariables;
    }

    CodeFeatures features() const { return m_features; }
    bool hasCapturedVariables() const { return m_hasCapturedVariables; }

    static const bool needsDestruction = true;
    static void destroy(JSCell*);

    bool isBuiltinFunction() const { return m_isBuiltinFunction; }
    ConstructAbility constructAbility() const { return static_cast<ConstructAbility>(m_constructAbility); }
    bool isClassConstructorFunction() const { return constructorKind() != ConstructorKind::None; }
    const VariableEnvironment* parentScopeTDZVariables() const { return &m_parentScopeTDZVariables; }

private:
    UnlinkedFunctionExecutable(VM*, Structure*, const SourceCode&, RefPtr<SourceProvider>&& sourceOverride, FunctionMetadataNode*, UnlinkedFunctionKind, ConstructAbility, GeneratorThisMode, VariableEnvironment&);
    WriteBarrier<UnlinkedFunctionCodeBlock> m_unlinkedCodeBlockForCall;
    WriteBarrier<UnlinkedFunctionCodeBlock> m_unlinkedCodeBlockForConstruct;

    Identifier m_name;
    Identifier m_inferredName;
    WriteBarrier<JSString> m_nameValue;
    RefPtr<SourceProvider> m_sourceOverride;
    VariableEnvironment m_parentScopeTDZVariables;
    unsigned m_firstLineOffset;
    unsigned m_lineCount;
    unsigned m_unlinkedFunctionNameStart;
    unsigned m_unlinkedBodyStartColumn;
    unsigned m_unlinkedBodyEndColumn;
    unsigned m_startOffset;
    unsigned m_sourceLength;
    unsigned m_parametersStartOffset;
    unsigned m_typeProfilingStartOffset;
    unsigned m_typeProfilingEndOffset;
    unsigned m_parameterCount;
    SourceParseMode m_parseMode;

    CodeFeatures m_features;

    unsigned m_isInStrictContext : 1;
    unsigned m_hasCapturedVariables : 1;
    unsigned m_isBuiltinFunction : 1;
    unsigned m_constructAbility: 1;
    unsigned m_constructorKind : 2;
    unsigned m_functionMode : 1; // FunctionMode
    unsigned m_generatorThisMode : 1;
    unsigned m_superBinding : 1;

protected:
    void finishCreation(VM& vm)
    {
        Base::finishCreation(vm);
        m_nameValue.set(vm, this, jsString(&vm, name().string()));
    }

    static void visitChildren(JSCell*, SlotVisitor&);

public:
    static Structure* createStructure(VM& vm, JSGlobalObject* globalObject, JSValue proto)
    {
        return Structure::create(vm, globalObject, proto, TypeInfo(UnlinkedFunctionExecutableType, StructureFlags), info());
    }

    DECLARE_EXPORT_INFO;
};

} // namespace JSC

#endif // UnlinkedFunctionExecutable_h
