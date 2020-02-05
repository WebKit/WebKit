/*
 * Copyright (C) 2012-2019 Apple Inc. All Rights Reserved.
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
#include "CodeCache.h"

#include "BytecodeGenerator.h"
#include "IndirectEvalExecutable.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace JSC {

void CodeCacheMap::pruneSlowCase()
{
    m_minCapacity = std::max(m_size - m_sizeAtLastPrune, static_cast<int64_t>(0));
    m_sizeAtLastPrune = m_size;
    m_timeAtLastPrune = MonotonicTime::now();

    if (m_capacity < m_minCapacity)
        m_capacity = m_minCapacity;

    while (m_size > m_capacity || !canPruneQuickly()) {
        MapType::iterator it = m_map.begin();

        writeCodeBlock(it->value.cell->vm(), it->key, it->value);

        m_size -= it->key.length();
        m_map.remove(it);
    }
}

static void generateUnlinkedCodeBlockForFunctions(VM& vm, UnlinkedCodeBlock* unlinkedCodeBlock, const SourceCode& parentSource, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error)
{
    auto generate = [&](UnlinkedFunctionExecutable* unlinkedExecutable, CodeSpecializationKind constructorKind) {
        if (constructorKind == CodeForConstruct && SourceParseModeSet(SourceParseMode::AsyncArrowFunctionMode, SourceParseMode::AsyncMethodMode, SourceParseMode::AsyncFunctionMode).contains(unlinkedExecutable->parseMode()))
            return;

        SourceCode source = unlinkedExecutable->linkedSourceCode(parentSource);
        UnlinkedFunctionCodeBlock* unlinkedFunctionCodeBlock = unlinkedExecutable->unlinkedCodeBlockFor(vm, source, constructorKind, codeGenerationMode, error, unlinkedExecutable->parseMode());
        if (unlinkedFunctionCodeBlock)
            generateUnlinkedCodeBlockForFunctions(vm, unlinkedFunctionCodeBlock, source, codeGenerationMode, error);
    };

    // FIXME: We should also generate CodeBlocks for CodeForConstruct
    // https://bugs.webkit.org/show_bug.cgi?id=193823
    for (unsigned i = 0; i < unlinkedCodeBlock->numberOfFunctionDecls(); i++)
        generate(unlinkedCodeBlock->functionDecl(i), CodeForCall);
    for (unsigned i = 0; i < unlinkedCodeBlock->numberOfFunctionExprs(); i++)
        generate(unlinkedCodeBlock->functionExpr(i), CodeForCall);
}

template <class UnlinkedCodeBlockType, class ExecutableType = ScriptExecutable>
UnlinkedCodeBlockType* generateUnlinkedCodeBlockImpl(VM& vm, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType, DerivedContextType derivedContextType, bool isArrowFunctionContext, const VariableEnvironment* variablesUnderTDZ, ExecutableType* executable = nullptr)
{
    typedef typename CacheTypes<UnlinkedCodeBlockType>::RootNode RootNode;
    std::unique_ptr<RootNode> rootNode = parse<RootNode>(
        vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin, strictMode, scriptMode, CacheTypes<UnlinkedCodeBlockType>::parseMode, SuperBinding::NotNeeded, error, nullptr, ConstructorKind::None, derivedContextType, evalContextType);
    if (!rootNode)
        return nullptr;

    unsigned lineCount = rootNode->lastLine() - rootNode->firstLine();
    unsigned startColumn = rootNode->startColumn() + 1;
    bool endColumnIsOnStartLine = !lineCount;
    unsigned unlinkedEndColumn = rootNode->endColumn();
    unsigned endColumn = unlinkedEndColumn + (endColumnIsOnStartLine ? startColumn : 1);
    unsigned arrowContextFeature = isArrowFunctionContext ? ArrowFunctionContextFeature : 0;
    if (executable)
        executable->recordParse(rootNode->features() | arrowContextFeature, rootNode->hasCapturedVariables(), rootNode->lastLine(), endColumn);

    bool usesEval = rootNode->features() & EvalFeature;
    bool isStrictMode = rootNode->features() & StrictModeFeature;
    NeedsClassFieldInitializer needsClassFieldInitializer = NeedsClassFieldInitializer::No;
    if constexpr (std::is_same_v<ExecutableType, DirectEvalExecutable>)
        needsClassFieldInitializer = executable->needsClassFieldInitializer();
    ExecutableInfo executableInfo(usesEval, isStrictMode, false, false, ConstructorKind::None, scriptMode, SuperBinding::NotNeeded, CacheTypes<UnlinkedCodeBlockType>::parseMode, derivedContextType, needsClassFieldInitializer, isArrowFunctionContext, false, evalContextType);

    UnlinkedCodeBlockType* unlinkedCodeBlock = UnlinkedCodeBlockType::create(vm, executableInfo, codeGenerationMode);
    unlinkedCodeBlock->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), lineCount, unlinkedEndColumn);
    if (!source.provider()->sourceURLDirective().isNull())
        unlinkedCodeBlock->setSourceURLDirective(source.provider()->sourceURLDirective());
    if (!source.provider()->sourceMappingURLDirective().isNull())
        unlinkedCodeBlock->setSourceMappingURLDirective(source.provider()->sourceMappingURLDirective());

    error = BytecodeGenerator::generate(vm, rootNode.get(), source, unlinkedCodeBlock, codeGenerationMode, variablesUnderTDZ);

    if (error.isValid())
        return nullptr;

    return unlinkedCodeBlock;
}

template <class UnlinkedCodeBlockType, class ExecutableType>
UnlinkedCodeBlockType* generateUnlinkedCodeBlock(VM& vm, ExecutableType* executable, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    return generateUnlinkedCodeBlockImpl<UnlinkedCodeBlockType, ExecutableType>(vm, source, strictMode, scriptMode, codeGenerationMode, error, evalContextType, executable->derivedContextType(), executable->isArrowFunctionContext(), variablesUnderTDZ, executable);
}

UnlinkedEvalCodeBlock* generateUnlinkedCodeBlockForDirectEval(VM& vm, DirectEvalExecutable* executable, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    return generateUnlinkedCodeBlock<UnlinkedEvalCodeBlock>(vm, executable, source, strictMode, scriptMode, codeGenerationMode, error, evalContextType, variablesUnderTDZ);
}

template <class UnlinkedCodeBlockType>
std::enable_if_t<!std::is_same<UnlinkedCodeBlockType, UnlinkedEvalCodeBlock>::value, UnlinkedCodeBlockType*>
recursivelyGenerateUnlinkedCodeBlock(VM& vm, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    bool isArrowFunctionContext = false;
    UnlinkedCodeBlockType* unlinkedCodeBlock = generateUnlinkedCodeBlockImpl<UnlinkedCodeBlockType>(vm, source, strictMode, scriptMode, codeGenerationMode, error, evalContextType, DerivedContextType::None, isArrowFunctionContext, variablesUnderTDZ);
    if (!unlinkedCodeBlock)
        return nullptr;

    generateUnlinkedCodeBlockForFunctions(vm, unlinkedCodeBlock, source, codeGenerationMode, error);
    return unlinkedCodeBlock;
}

UnlinkedProgramCodeBlock* recursivelyGenerateUnlinkedCodeBlockForProgram(VM& vm, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    return recursivelyGenerateUnlinkedCodeBlock<UnlinkedProgramCodeBlock>(vm, source, strictMode, scriptMode, codeGenerationMode, error, evalContextType, variablesUnderTDZ);
}

UnlinkedModuleProgramCodeBlock* recursivelyGenerateUnlinkedCodeBlockForModuleProgram(VM& vm, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    return recursivelyGenerateUnlinkedCodeBlock<UnlinkedModuleProgramCodeBlock>(vm, source, strictMode, scriptMode, codeGenerationMode, error, evalContextType, variablesUnderTDZ);
}

template <class UnlinkedCodeBlockType, class ExecutableType>
UnlinkedCodeBlockType* CodeCache::getUnlinkedGlobalCodeBlock(VM& vm, ExecutableType* executable, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType)
{
    DerivedContextType derivedContextType = executable->derivedContextType();
    bool isArrowFunctionContext = executable->isArrowFunctionContext();
    SourceCodeKey key(
        source, String(), CacheTypes<UnlinkedCodeBlockType>::codeType, strictMode, scriptMode, 
        derivedContextType, evalContextType, isArrowFunctionContext, codeGenerationMode,
        WTF::nullopt);
    UnlinkedCodeBlockType* unlinkedCodeBlock = m_sourceCode.findCacheAndUpdateAge<UnlinkedCodeBlockType>(vm, key);
    if (unlinkedCodeBlock && Options::useCodeCache()) {
        unsigned lineCount = unlinkedCodeBlock->lineCount();
        unsigned startColumn = unlinkedCodeBlock->startColumn() + source.startColumn().oneBasedInt();
        bool endColumnIsOnStartLine = !lineCount;
        unsigned endColumn = unlinkedCodeBlock->endColumn() + (endColumnIsOnStartLine ? startColumn : 1);
        executable->recordParse(unlinkedCodeBlock->codeFeatures(), unlinkedCodeBlock->hasCapturedVariables(), source.firstLine().oneBasedInt() + lineCount, endColumn);
        if (unlinkedCodeBlock->sourceURLDirective())
            source.provider()->setSourceURLDirective(unlinkedCodeBlock->sourceURLDirective());
        if (unlinkedCodeBlock->sourceMappingURLDirective())
            source.provider()->setSourceMappingURLDirective(unlinkedCodeBlock->sourceMappingURLDirective());
        return unlinkedCodeBlock;
    }

    VariableEnvironment variablesUnderTDZ;
    unlinkedCodeBlock = generateUnlinkedCodeBlock<UnlinkedCodeBlockType, ExecutableType>(vm, executable, source, strictMode, scriptMode, codeGenerationMode, error, evalContextType, &variablesUnderTDZ);

    if (unlinkedCodeBlock && Options::useCodeCache()) {
        m_sourceCode.addCache(key, SourceCodeValue(vm, unlinkedCodeBlock, m_sourceCode.age()));

        key.source().provider().cacheBytecode([&] {
            return encodeCodeBlock(vm, key, unlinkedCodeBlock);
        });
    }

    return unlinkedCodeBlock;
}

UnlinkedProgramCodeBlock* CodeCache::getUnlinkedProgramCodeBlock(VM& vm, ProgramExecutable* executable, const SourceCode& source, JSParserStrictMode strictMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error)
{
    return getUnlinkedGlobalCodeBlock<UnlinkedProgramCodeBlock>(vm, executable, source, strictMode, JSParserScriptMode::Classic, codeGenerationMode, error, EvalContextType::None);
}

UnlinkedEvalCodeBlock* CodeCache::getUnlinkedEvalCodeBlock(VM& vm, IndirectEvalExecutable* executable, const SourceCode& source, JSParserStrictMode strictMode, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error, EvalContextType evalContextType)
{
    return getUnlinkedGlobalCodeBlock<UnlinkedEvalCodeBlock>(vm, executable, source, strictMode, JSParserScriptMode::Classic, codeGenerationMode, error, evalContextType);
}

UnlinkedModuleProgramCodeBlock* CodeCache::getUnlinkedModuleProgramCodeBlock(VM& vm, ModuleProgramExecutable* executable, const SourceCode& source, OptionSet<CodeGenerationMode> codeGenerationMode, ParserError& error)
{
    return getUnlinkedGlobalCodeBlock<UnlinkedModuleProgramCodeBlock>(vm, executable, source, JSParserStrictMode::Strict, JSParserScriptMode::Module, codeGenerationMode, error, EvalContextType::None);
}

UnlinkedFunctionExecutable* CodeCache::getUnlinkedGlobalFunctionExecutable(VM& vm, const Identifier& name, const SourceCode& source, OptionSet<CodeGenerationMode> codeGenerationMode, Optional<int> functionConstructorParametersEndPosition, ParserError& error)
{
    bool isArrowFunctionContext = false;
    SourceCodeKey key(
        source, name.string(), SourceCodeType::FunctionType,
        JSParserStrictMode::NotStrict,
        JSParserScriptMode::Classic,
        DerivedContextType::None,
        EvalContextType::None,
        isArrowFunctionContext,
        codeGenerationMode,
        functionConstructorParametersEndPosition);
    UnlinkedFunctionExecutable* executable = m_sourceCode.findCacheAndUpdateAge<UnlinkedFunctionExecutable>(vm, key);
    if (executable && Options::useCodeCache()) {
        if (!executable->sourceURLDirective().isNull())
            source.provider()->setSourceURLDirective(executable->sourceURLDirective());
        if (!executable->sourceMappingURLDirective().isNull())
            source.provider()->setSourceMappingURLDirective(executable->sourceMappingURLDirective());
        return executable;
    }

    JSTextPosition positionBeforeLastNewline;
    std::unique_ptr<ProgramNode> program = parseFunctionForFunctionConstructor(vm, source, error, &positionBeforeLastNewline, functionConstructorParametersEndPosition);
    if (!program) {
        RELEASE_ASSERT(error.isValid());
        return nullptr;
    }

    // This function assumes an input string that would result in a single function declaration.
    StatementNode* funcDecl = program->singleStatement();
    if (UNLIKELY(!funcDecl)) {
        JSToken token;
        error = ParserError(ParserError::SyntaxError, ParserError::SyntaxErrorIrrecoverable, token, "Parser error", -1);
        return nullptr;
    }
    ASSERT(funcDecl->isFuncDeclNode());

    FunctionMetadataNode* metadata = static_cast<FuncDeclNode*>(funcDecl)->metadata();
    ASSERT(metadata);
    if (!metadata)
        return nullptr;
    
    metadata->overrideName(name);
    metadata->setEndPosition(positionBeforeLastNewline);
    // The Function constructor only has access to global variables, so no variables will be under TDZ unless they're
    // in the global lexical environment, which we always TDZ check accesses from.
    ConstructAbility constructAbility = constructAbilityForParseMode(metadata->parseMode());
    UnlinkedFunctionExecutable* functionExecutable = UnlinkedFunctionExecutable::create(vm, source, metadata, UnlinkedNormalFunction, constructAbility, JSParserScriptMode::Classic, WTF::nullopt, DerivedContextType::None, NeedsClassFieldInitializer::No);

    if (!source.provider()->sourceURLDirective().isNull())
        functionExecutable->setSourceURLDirective(source.provider()->sourceURLDirective());
    if (!source.provider()->sourceMappingURLDirective().isNull())
        functionExecutable->setSourceMappingURLDirective(source.provider()->sourceMappingURLDirective());

    if (Options::useCodeCache())
        m_sourceCode.addCache(key, SourceCodeValue(vm, functionExecutable, m_sourceCode.age()));
    return functionExecutable;
}

void CodeCache::updateCache(const UnlinkedFunctionExecutable* executable, const SourceCode& parentSource, CodeSpecializationKind kind, const UnlinkedFunctionCodeBlock* codeBlock)
{
    parentSource.provider()->updateCache(executable, parentSource, kind, codeBlock);
}

void CodeCache::write(VM& vm)
{
    for (auto& it : m_sourceCode)
        writeCodeBlock(vm, it.key, it.value);
}

void writeCodeBlock(VM& vm, const SourceCodeKey& key, const SourceCodeValue& value)
{
    UnlinkedCodeBlock* codeBlock = jsDynamicCast<UnlinkedCodeBlock*>(vm, value.cell.get());
    if (!codeBlock)
        return;

    key.source().provider().commitCachedBytecode();
}

static SourceCodeKey sourceCodeKeyForSerializedBytecode(VM&, const SourceCode& sourceCode, SourceCodeType codeType, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, OptionSet<CodeGenerationMode> codeGenerationMode)
{
    return SourceCodeKey(
        sourceCode, String(), codeType, strictMode, scriptMode,
        DerivedContextType::None, EvalContextType::None, false, codeGenerationMode,
        WTF::nullopt);
}

SourceCodeKey sourceCodeKeyForSerializedProgram(VM& vm, const SourceCode& sourceCode)
{
    JSParserStrictMode strictMode = JSParserStrictMode::NotStrict;
    JSParserScriptMode scriptMode = JSParserScriptMode::Classic;
    return sourceCodeKeyForSerializedBytecode(vm, sourceCode, SourceCodeType::ProgramType, strictMode, scriptMode, { });
}

SourceCodeKey sourceCodeKeyForSerializedModule(VM& vm, const SourceCode& sourceCode)
{
    JSParserStrictMode strictMode = JSParserStrictMode::Strict;
    JSParserScriptMode scriptMode = JSParserScriptMode::Module;
    return sourceCodeKeyForSerializedBytecode(vm, sourceCode, SourceCodeType::ModuleType, strictMode, scriptMode, { });
}

RefPtr<CachedBytecode> serializeBytecode(VM& vm, UnlinkedCodeBlock* codeBlock, const SourceCode& source, SourceCodeType codeType, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, FileSystem::PlatformFileHandle fd, BytecodeCacheError& error, OptionSet<CodeGenerationMode> codeGenerationMode)
{
    return encodeCodeBlock(vm,
        sourceCodeKeyForSerializedBytecode(vm, source, codeType, strictMode, scriptMode, codeGenerationMode), codeBlock, fd, error);
}

}
