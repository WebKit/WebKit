/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#include "BuiltinExecutables.h"

#include "BuiltinNames.h"
#include "JSCInlines.h"
#include "Parser.h"
#include <wtf/NeverDestroyed.h>

namespace JSC {

BuiltinExecutables::BuiltinExecutables(VM& vm)
    : m_vm(vm)
#define INITIALIZE_BUILTIN_SOURCE_MEMBERS(name, functionName, overrideName, length) , m_##name##Source(makeSource(StringImpl::createFromLiteral(s_##name, length), { }))
    JSC_FOREACH_BUILTIN_CODE(INITIALIZE_BUILTIN_SOURCE_MEMBERS)
#undef EXPOSE_BUILTIN_STRINGS
{
}

SourceCode BuiltinExecutables::defaultConstructorSourceCode(ConstructorKind constructorKind)
{
    switch (constructorKind) {
    case ConstructorKind::None:
        break;
    case ConstructorKind::Base: {
        static NeverDestroyed<const String> baseConstructorCode(MAKE_STATIC_STRING_IMPL("(function () { })"));
        return makeSource(baseConstructorCode, { });
    }
    case ConstructorKind::Extends: {
        static NeverDestroyed<const String> derivedConstructorCode(MAKE_STATIC_STRING_IMPL("(function (...args) { super(...args); })"));
        return makeSource(derivedConstructorCode, { });
    }
    }
    RELEASE_ASSERT_NOT_REACHED();
    return SourceCode();
}

UnlinkedFunctionExecutable* BuiltinExecutables::createDefaultConstructor(ConstructorKind constructorKind, const Identifier& name)
{
    switch (constructorKind) {
    case ConstructorKind::None:
        break;
    case ConstructorKind::Base:
    case ConstructorKind::Extends:
        return createExecutable(m_vm, defaultConstructorSourceCode(constructorKind), name, constructorKind, ConstructAbility::CanConstruct);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

UnlinkedFunctionExecutable* BuiltinExecutables::createBuiltinExecutable(const SourceCode& code, const Identifier& name, ConstructAbility constructAbility)
{
    return createExecutable(m_vm, code, name, ConstructorKind::None, constructAbility);
}

UnlinkedFunctionExecutable* createBuiltinExecutable(VM& vm, const SourceCode& code, const Identifier& name, ConstructAbility constructAbility)
{
    return BuiltinExecutables::createExecutable(vm, code, name, ConstructorKind::None, constructAbility);
}

UnlinkedFunctionExecutable* BuiltinExecutables::createExecutable(VM& vm, const SourceCode& source, const Identifier& name, ConstructorKind constructorKind, ConstructAbility constructAbility)
{
    // FIXME: Can we just make MetaData computation be constexpr and have the compiler do this for us?
    // https://bugs.webkit.org/show_bug.cgi?id=193272
    // Someone should get mad at me for writing this code. But, it prevents us from recursing into
    // the parser, and hence, from throwing stack overflow when parsing a builtin.
    StringView view = source.view();
    RELEASE_ASSERT(!view.isNull());
    RELEASE_ASSERT(view.is8Bit());
    auto* characters = view.characters8();
    const char* regularFunctionBegin = "(function (";
    const char* asyncFunctionBegin = "(async function (";
    RELEASE_ASSERT(view.length() >= strlen("(function (){})"));
    bool isAsyncFunction = view.length() >= strlen("(async function (){})") && !memcmp(characters, asyncFunctionBegin, strlen(asyncFunctionBegin));
    RELEASE_ASSERT(isAsyncFunction || !memcmp(characters, regularFunctionBegin, strlen(regularFunctionBegin)));

    unsigned asyncOffset = isAsyncFunction ? strlen("async ") : 0;
    unsigned parametersStart = strlen("function (") + asyncOffset;
    JSTokenLocation start;
    start.line = -1;
    start.lineStartOffset = std::numeric_limits<unsigned>::max();
    start.startOffset = parametersStart;
    start.endOffset = std::numeric_limits<unsigned>::max();

    JSTokenLocation end;
    end.line = 1;
    end.lineStartOffset = 0;
    end.startOffset = strlen("(") + asyncOffset;
    end.endOffset = std::numeric_limits<unsigned>::max();

    unsigned startColumn = parametersStart;
    int functionKeywordStart = strlen("(") + asyncOffset;
    int functionNameStart = parametersStart;
    bool isInStrictContext = false;
    bool isArrowFunctionBodyExpression = false;

    unsigned parameterCount;
    {
        unsigned i = parametersStart + 1;
        unsigned commas = 0;
        bool sawOneParam = false;
        bool hasRestParam = false;
        while (true) {
            ASSERT(i < view.length());
            if (characters[i] == ')')
                break;

            if (characters[i] == ',')
                ++commas;
            else if (!Lexer<LChar>::isWhiteSpace(characters[i]))
                sawOneParam = true;

            if (i + 2 < view.length() && characters[i] == '.' && characters[i + 1] == '.' && characters[i + 2] == '.') {
                hasRestParam = true;
                i += 2;
            }

            ++i;
        }

        if (commas)
            parameterCount = commas + 1;
        else if (sawOneParam)
            parameterCount = 1;
        else
            parameterCount = 0;

        if (hasRestParam) {
            RELEASE_ASSERT(parameterCount);
            --parameterCount;
        }
    }

    unsigned lineCount = 0;
    unsigned endColumn = 0;
    unsigned offsetOfLastNewline = 0;
    Optional<unsigned> offsetOfSecondToLastNewline;
    for (unsigned i = 0; i < view.length(); ++i) {
        if (characters[i] == '\n') {
            if (lineCount)
                offsetOfSecondToLastNewline = offsetOfLastNewline;
            ++lineCount;
            endColumn = 0;
            offsetOfLastNewline = i;
        } else
            ++endColumn;

        if (!isInStrictContext && (characters[i] == '"' || characters[i] == '\'')) {
            const unsigned useStrictLength = strlen("use strict");
            if (i + 1 + useStrictLength < view.length()) {
                if (!memcmp(characters + i + 1, "use strict", useStrictLength)) {
                    isInStrictContext = true;
                    i += 1 + useStrictLength;
                }
            }
        }
    }

    unsigned positionBeforeLastNewlineLineStartOffset = offsetOfSecondToLastNewline ? *offsetOfSecondToLastNewline + 1 : 0;

    int closeBraceOffsetFromEnd = 1;
    while (true) {
        if (characters[view.length() - closeBraceOffsetFromEnd] == '}')
            break;
        ++closeBraceOffsetFromEnd;
    }

    JSTextPosition positionBeforeLastNewline;
    positionBeforeLastNewline.line = lineCount;
    positionBeforeLastNewline.offset = offsetOfLastNewline;
    positionBeforeLastNewline.lineStartOffset = positionBeforeLastNewlineLineStartOffset;

    SourceCode newSource = source.subExpression(parametersStart, view.length() - closeBraceOffsetFromEnd, 0, parametersStart);
    bool isBuiltinDefaultClassConstructor = constructorKind != ConstructorKind::None;
    UnlinkedFunctionKind kind = isBuiltinDefaultClassConstructor ? UnlinkedNormalFunction : UnlinkedBuiltinFunction;

    SourceParseMode parseMode = isAsyncFunction ? SourceParseMode::AsyncFunctionMode : SourceParseMode::NormalFunctionMode;
    FunctionMetadataNode metadata(
        start, end, startColumn, endColumn, functionKeywordStart, functionNameStart, parametersStart,
        isInStrictContext, constructorKind, constructorKind == ConstructorKind::Extends ? SuperBinding::Needed : SuperBinding::NotNeeded,
        parameterCount, parseMode, isArrowFunctionBodyExpression);

    metadata.finishParsing(newSource, Identifier(), FunctionMode::FunctionExpression);
    metadata.overrideName(name);
    metadata.setEndPosition(positionBeforeLastNewline);

    if (!ASSERT_DISABLED || Options::validateBytecode()) {
        JSTextPosition positionBeforeLastNewlineFromParser;
        ParserError error;
        JSParserBuiltinMode builtinMode = isBuiltinDefaultClassConstructor ? JSParserBuiltinMode::NotBuiltin : JSParserBuiltinMode::Builtin;
        std::unique_ptr<ProgramNode> program = parse<ProgramNode>(
            &vm, source, Identifier(), builtinMode,
            JSParserStrictMode::NotStrict, JSParserScriptMode::Classic, SourceParseMode::ProgramMode, SuperBinding::NotNeeded, error,
            &positionBeforeLastNewlineFromParser, constructorKind);

        if (program) {
            StatementNode* exprStatement = program->singleStatement();
            RELEASE_ASSERT(exprStatement);
            RELEASE_ASSERT(exprStatement->isExprStatement());
            ExpressionNode* funcExpr = static_cast<ExprStatementNode*>(exprStatement)->expr();
            RELEASE_ASSERT(funcExpr);
            RELEASE_ASSERT(funcExpr->isFuncExprNode());
            FunctionMetadataNode* metadataFromParser = static_cast<FuncExprNode*>(funcExpr)->metadata();
            RELEASE_ASSERT(!program->hasCapturedVariables());
            
            metadataFromParser->setEndPosition(positionBeforeLastNewlineFromParser);
            RELEASE_ASSERT(metadataFromParser);
            RELEASE_ASSERT(metadataFromParser->ident().isNull());
            
            // This function assumes an input string that would result in a single anonymous function expression.
            metadataFromParser->setEndPosition(positionBeforeLastNewlineFromParser);
            RELEASE_ASSERT(metadataFromParser);
            metadataFromParser->overrideName(name);
            metadataFromParser->setEndPosition(positionBeforeLastNewlineFromParser);

            if (metadata != *metadataFromParser || positionBeforeLastNewlineFromParser != positionBeforeLastNewline) {
                dataLogLn("Expected Metadata:\n", metadata);
                dataLogLn("Metadata from parser:\n", *metadataFromParser);
                dataLogLn("positionBeforeLastNewlineFromParser.line ", positionBeforeLastNewlineFromParser.line);
                dataLogLn("positionBeforeLastNewlineFromParser.offset ", positionBeforeLastNewlineFromParser.offset);
                dataLogLn("positionBeforeLastNewlineFromParser.lineStartOffset ", positionBeforeLastNewlineFromParser.lineStartOffset);
                dataLogLn("positionBeforeLastNewline.line ", positionBeforeLastNewline.line);
                dataLogLn("positionBeforeLastNewline.offset ", positionBeforeLastNewline.offset);
                dataLogLn("positionBeforeLastNewline.lineStartOffset ", positionBeforeLastNewline.lineStartOffset);
                WTFLogAlways("Metadata of parser and hand rolled parser don't match\n");
                CRASH();
            }
        } else {
            RELEASE_ASSERT(error.isValid());
            RELEASE_ASSERT(error.type() == ParserError::StackOverflow);
        }
    }

    VariableEnvironment dummyTDZVariables;
    UnlinkedFunctionExecutable* functionExecutable = UnlinkedFunctionExecutable::create(&vm, source, &metadata, kind, constructAbility, JSParserScriptMode::Classic, dummyTDZVariables, DerivedContextType::None, isBuiltinDefaultClassConstructor);
    return functionExecutable;
}

void BuiltinExecutables::finalize(Handle<Unknown>, void* context)
{
    static_cast<Weak<UnlinkedFunctionExecutable>*>(context)->clear();
}

#define DEFINE_BUILTIN_EXECUTABLES(name, functionName, overrideName, length) \
UnlinkedFunctionExecutable* BuiltinExecutables::name##Executable() \
{\
    if (!m_##name##Executable) {\
        Identifier executableName = m_vm.propertyNames->builtinNames().functionName##PublicName();\
        if (overrideName)\
            executableName = Identifier::fromString(&m_vm, overrideName);\
        m_##name##Executable = Weak<UnlinkedFunctionExecutable>(createBuiltinExecutable(m_##name##Source, executableName, s_##name##ConstructAbility), this, &m_##name##Executable);\
    }\
    return m_##name##Executable.get();\
}
JSC_FOREACH_BUILTIN_CODE(DEFINE_BUILTIN_EXECUTABLES)
#undef EXPOSE_BUILTIN_SOURCES

}
