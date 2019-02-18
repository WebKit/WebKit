/*
 * Copyright (C) 2012-2018 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "UnlinkedFunctionExecutable.h"

#include "BuiltinExecutables.h"
#include "BytecodeGenerator.h"
#include "ClassInfo.h"
#include "CodeCache.h"
#include "Debugger.h"
#include "ExecutableInfo.h"
#include "FunctionOverrides.h"
#include "IsoCellSetInlines.h"
#include "JSCInlines.h"
#include "Parser.h"
#include "SourceProvider.h"
#include "Structure.h"
#include "UnlinkedFunctionCodeBlock.h"

namespace JSC {

static_assert(sizeof(UnlinkedFunctionExecutable) <= 160, "UnlinkedFunctionExecutable should fit in a 160-byte cell. If you increase the size of this class, consider making a size class that perfectly fits it.");

const ClassInfo UnlinkedFunctionExecutable::s_info = { "UnlinkedFunctionExecutable", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(UnlinkedFunctionExecutable) };

static UnlinkedFunctionCodeBlock* generateUnlinkedFunctionCodeBlock(
    VM& vm, UnlinkedFunctionExecutable* executable, const SourceCode& source,
    CodeSpecializationKind kind, DebuggerMode debuggerMode,
    UnlinkedFunctionKind functionKind, ParserError& error, SourceParseMode parseMode)
{
    JSParserBuiltinMode builtinMode = executable->isBuiltinFunction() ? JSParserBuiltinMode::Builtin : JSParserBuiltinMode::NotBuiltin;
    JSParserStrictMode strictMode = executable->isInStrictContext() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    JSParserScriptMode scriptMode = executable->scriptMode();
    ASSERT(isFunctionParseMode(executable->parseMode()));
    std::unique_ptr<FunctionNode> function = parse<FunctionNode>(
        &vm, source, executable->name(), builtinMode, strictMode, scriptMode, executable->parseMode(), executable->superBinding(), error, nullptr);

    if (!function) {
        ASSERT(error.isValid());
        return nullptr;
    }

    function->finishParsing(executable->name(), executable->functionMode());
    executable->recordParse(function->features(), function->hasCapturedVariables());

    bool isClassContext = executable->superBinding() == SuperBinding::Needed;

    UnlinkedFunctionCodeBlock* result = UnlinkedFunctionCodeBlock::create(&vm, FunctionCode, ExecutableInfo(function->usesEval(), function->isStrictMode(), kind == CodeForConstruct, functionKind == UnlinkedBuiltinFunction, executable->constructorKind(), scriptMode, executable->superBinding(), parseMode, executable->derivedContextType(), false, isClassContext, EvalContextType::FunctionEvalContext), debuggerMode);

    VariableEnvironment parentScopeTDZVariables = executable->parentScopeTDZVariables();
    error = BytecodeGenerator::generate(vm, function.get(), source, result, debuggerMode, &parentScopeTDZVariables);

    if (error.isValid())
        return nullptr;
    return result;
}

UnlinkedFunctionExecutable::UnlinkedFunctionExecutable(VM* vm, Structure* structure, const SourceCode& parentSource, FunctionMetadataNode* node, UnlinkedFunctionKind kind, ConstructAbility constructAbility, JSParserScriptMode scriptMode, VariableEnvironment& parentScopeTDZVariables, DerivedContextType derivedContextType, bool isBuiltinDefaultClassConstructor)
    : Base(*vm, structure)
    , m_firstLineOffset(node->firstLine() - parentSource.firstLine().oneBasedInt())
    , m_lineCount(node->lastLine() - node->firstLine())
    , m_unlinkedFunctionNameStart(node->functionNameStart() - parentSource.startOffset())
    , m_unlinkedBodyStartColumn(node->startColumn())
    , m_unlinkedBodyEndColumn(m_lineCount ? node->endColumn() : node->endColumn() - node->startColumn())
    , m_startOffset(node->source().startOffset() - parentSource.startOffset())
    , m_sourceLength(node->source().length())
    , m_parametersStartOffset(node->parametersStart())
    , m_typeProfilingStartOffset(node->functionKeywordStart())
    , m_typeProfilingEndOffset(node->startStartOffset() + node->source().length() - 1)
    , m_parameterCount(node->parameterCount())
    , m_features(0)
    , m_sourceParseMode(node->parseMode())
    , m_isInStrictContext(node->isInStrictContext())
    , m_hasCapturedVariables(false)
    , m_isBuiltinFunction(kind == UnlinkedBuiltinFunction)
    , m_isBuiltinDefaultClassConstructor(isBuiltinDefaultClassConstructor)
    , m_constructAbility(static_cast<unsigned>(constructAbility))
    , m_constructorKind(static_cast<unsigned>(node->constructorKind()))
    , m_functionMode(static_cast<unsigned>(node->functionMode()))
    , m_scriptMode(static_cast<unsigned>(scriptMode))
    , m_superBinding(static_cast<unsigned>(node->superBinding()))
    , m_derivedContextType(static_cast<unsigned>(derivedContextType))
    , m_name(node->ident())
    , m_ecmaName(node->ecmaName())
    , m_inferredName(node->inferredName())
    , m_classSource(node->classSource())
    , m_parentScopeTDZVariables(vm->m_compactVariableMap->get(parentScopeTDZVariables))
{
    // Make sure these bitfields are adequately wide.
    ASSERT(m_constructAbility == static_cast<unsigned>(constructAbility));
    ASSERT(m_constructorKind == static_cast<unsigned>(node->constructorKind()));
    ASSERT(m_functionMode == static_cast<unsigned>(node->functionMode()));
    ASSERT(m_scriptMode == static_cast<unsigned>(scriptMode));
    ASSERT(m_superBinding == static_cast<unsigned>(node->superBinding()));
    ASSERT(m_derivedContextType == static_cast<unsigned>(derivedContextType));
    ASSERT(!(m_isBuiltinDefaultClassConstructor && constructorKind() == ConstructorKind::None));
}

void UnlinkedFunctionExecutable::destroy(JSCell* cell)
{
    static_cast<UnlinkedFunctionExecutable*>(cell)->~UnlinkedFunctionExecutable();
}

void UnlinkedFunctionExecutable::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    UnlinkedFunctionExecutable* thisObject = jsCast<UnlinkedFunctionExecutable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_unlinkedCodeBlockForCall);
    visitor.append(thisObject->m_unlinkedCodeBlockForConstruct);
}

SourceCode UnlinkedFunctionExecutable::linkedSourceCode(const SourceCode& passedParentSource) const
{
    const SourceCode& parentSource = !m_isBuiltinDefaultClassConstructor ? passedParentSource : BuiltinExecutables::defaultConstructorSourceCode(constructorKind());
    unsigned startColumn = linkedStartColumn(parentSource.startColumn().oneBasedInt());
    unsigned startOffset = parentSource.startOffset() + m_startOffset;
    unsigned firstLine = parentSource.firstLine().oneBasedInt() + m_firstLineOffset;
    return SourceCode(parentSource.provider(), startOffset, startOffset + m_sourceLength, firstLine, startColumn);
}

FunctionExecutable* UnlinkedFunctionExecutable::link(VM& vm, const SourceCode& passedParentSource, Optional<int> overrideLineNumber, Intrinsic intrinsic)
{
    SourceCode source = linkedSourceCode(passedParentSource);
    unsigned firstLine = source.firstLine().oneBasedInt();
    unsigned lineCount = m_lineCount;
    unsigned endColumn = linkedEndColumn(source.startColumn().oneBasedInt());
    FunctionOverrides::OverrideInfo overrideInfo;
    bool hasFunctionOverride = false;
    if (UNLIKELY(Options::functionOverrides())) {
        hasFunctionOverride = FunctionOverrides::initializeOverrideFor(source, overrideInfo);
        if (UNLIKELY(hasFunctionOverride)) {
            firstLine = overrideInfo.firstLine;
            lineCount = overrideInfo.lineCount;
            endColumn = overrideInfo.endColumn;
            source = overrideInfo.sourceCode;
        }
    }

    FunctionExecutable* result = FunctionExecutable::create(vm, source, this, firstLine + lineCount, endColumn, intrinsic);
    if (overrideLineNumber)
        result->setOverrideLineNumber(*overrideLineNumber);

    if (UNLIKELY(hasFunctionOverride)) {
        result->overrideParameterAndTypeProfilingStartEndOffsets(
            overrideInfo.parametersStartOffset,
            overrideInfo.typeProfilingStartOffset,
            overrideInfo.typeProfilingEndOffset);
    }

    return result;
}

UnlinkedFunctionExecutable* UnlinkedFunctionExecutable::fromGlobalCode(
    const Identifier& name, ExecState& exec, const SourceCode& source, 
    JSObject*& exception, int overrideLineNumber, Optional<int> functionConstructorParametersEndPosition)
{
    ParserError error;
    VM& vm = exec.vm();
    auto& globalObject = *exec.lexicalGlobalObject();
    CodeCache* codeCache = vm.codeCache();
    DebuggerMode debuggerMode = globalObject.hasInteractiveDebugger() ? DebuggerOn : DebuggerOff;
    UnlinkedFunctionExecutable* executable = codeCache->getUnlinkedGlobalFunctionExecutable(vm, name, source, debuggerMode, functionConstructorParametersEndPosition, error);

    if (globalObject.hasDebugger())
        globalObject.debugger()->sourceParsed(&exec, source.provider(), error.line(), error.message());

    if (error.isValid()) {
        exception = error.toErrorObject(&globalObject, source, overrideLineNumber);
        return nullptr;
    }

    return executable;
}

UnlinkedFunctionCodeBlock* UnlinkedFunctionExecutable::unlinkedCodeBlockFor(CodeSpecializationKind specializationKind)
{
    switch (specializationKind) {
    case CodeForCall:
        return m_unlinkedCodeBlockForCall.get();
    case CodeForConstruct:
        return m_unlinkedCodeBlockForConstruct.get();
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

UnlinkedFunctionCodeBlock* UnlinkedFunctionExecutable::unlinkedCodeBlockFor(
    VM& vm, const SourceCode& source, CodeSpecializationKind specializationKind, 
    DebuggerMode debuggerMode, ParserError& error, SourceParseMode parseMode)
{
    switch (specializationKind) {
    case CodeForCall:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_unlinkedCodeBlockForCall.get())
            return codeBlock;
        break;
    case CodeForConstruct:
        if (UnlinkedFunctionCodeBlock* codeBlock = m_unlinkedCodeBlockForConstruct.get())
            return codeBlock;
        break;
    }

    UnlinkedFunctionCodeBlock* result = generateUnlinkedFunctionCodeBlock(
        vm, this, source, specializationKind, debuggerMode, 
        isBuiltinFunction() ? UnlinkedBuiltinFunction : UnlinkedNormalFunction, 
        error, parseMode);
    
    if (error.isValid())
        return nullptr;

    switch (specializationKind) {
    case CodeForCall:
        m_unlinkedCodeBlockForCall.set(vm, this, result);
        break;
    case CodeForConstruct:
        m_unlinkedCodeBlockForConstruct.set(vm, this, result);
        break;
    }
    vm.unlinkedFunctionExecutableSpace.set.add(this);
    return result;
}

void UnlinkedFunctionExecutable::setInvalidTypeProfilingOffsets()
{
    m_typeProfilingStartOffset = std::numeric_limits<unsigned>::max();
    m_typeProfilingEndOffset = std::numeric_limits<unsigned>::max();
}

} // namespace JSC
