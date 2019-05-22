/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WHLSLPrepare.h"

#if ENABLE(WEBGPU)

#include "WHLSLASTDumper.h"
#include "WHLSLCheckDuplicateFunctions.h"
#include "WHLSLChecker.h"
#include "WHLSLFunctionStageChecker.h"
#include "WHLSLHighZombieFinder.h"
#include "WHLSLLiteralTypeChecker.h"
#include "WHLSLMetalCodeGenerator.h"
#include "WHLSLNameResolver.h"
#include "WHLSLParser.h"
#include "WHLSLProgram.h"
#include "WHLSLRecursionChecker.h"
#include "WHLSLRecursiveTypeChecker.h"
#include "WHLSLSemanticMatcher.h"
#include "WHLSLStandardLibrary.h"
#include "WHLSLStatementBehaviorChecker.h"
#include "WHLSLSynthesizeArrayOperatorLength.h"
#include "WHLSLSynthesizeConstructors.h"
#include "WHLSLSynthesizeEnumerationFunctions.h"
#include "WHLSLSynthesizeStructureAccessors.h"
#include <wtf/Optional.h>

namespace WebCore {

namespace WHLSL {

static constexpr bool dumpASTBeforeEachPass = false;
static constexpr bool dumpASTAfterParsing = false;
static constexpr bool dumpASTAtEnd = false;

static bool dumpASTIfNeeded(bool shouldDump, Program& program, const char* message)
{
    if (shouldDump) {
        dataLogLn(message);
        dumpAST(program);
        return true;
    }

    return false;
}

static bool dumpASTAfterParsingIfNeeded(Program& program)
{
    return dumpASTIfNeeded(dumpASTAfterParsing, program, "AST after parsing");
}

static bool dumpASTBetweenEachPassIfNeeded(Program& program, const char* message)
{
    return dumpASTIfNeeded(dumpASTBeforeEachPass, program, message);
}

static bool dumpASTAtEndIfNeeded(Program& program)
{
    return dumpASTIfNeeded(dumpASTAtEnd, program, "AST at end");
}

#define RUN_PASS(pass, ...) \
    do { \
        dumpASTBetweenEachPassIfNeeded(program, "AST before " # pass); \
        if (!pass(__VA_ARGS__)) \
            return WTF::nullopt; \
    } while (0)
    

static Optional<Program> prepareShared(String& whlslSource)
{
    Program program;
    Parser parser;
    auto standardLibrary = String::fromUTF8(WHLSLStandardLibrary, sizeof(WHLSLStandardLibrary));
    auto failure = static_cast<bool>(parser.parse(program, standardLibrary, Parser::Mode::StandardLibrary));
    ASSERT_UNUSED(failure, !failure);
    if (parser.parse(program, whlslSource, Parser::Mode::User))
        return WTF::nullopt;

    if (!dumpASTBetweenEachPassIfNeeded(program, "AST after parsing"))
        dumpASTAfterParsingIfNeeded(program);

    NameResolver nameResolver(program.nameContext());
    RUN_PASS(resolveNamesInTypes, program, nameResolver);
    RUN_PASS(checkRecursiveTypes, program);
    RUN_PASS(synthesizeStructureAccessors, program);
    RUN_PASS(synthesizeEnumerationFunctions, program);
    RUN_PASS(synthesizeArrayOperatorLength, program);
    RUN_PASS(synthesizeConstructors, program);
    RUN_PASS(resolveNamesInFunctions, program, nameResolver);
    RUN_PASS(checkDuplicateFunctions, program);

    RUN_PASS(check, program);

    checkLiteralTypes(program);
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195788 Resolve properties here
    findHighZombies(program);
    RUN_PASS(checkStatementBehavior, program);
    RUN_PASS(checkRecursion, program);
    RUN_PASS(checkFunctionStages, program);

    dumpASTAtEndIfNeeded(program);

    return program;
}

Optional<RenderPrepareResult> prepare(String& whlslSource, RenderPipelineDescriptor& renderPipelineDescriptor)
{
    auto program = prepareShared(whlslSource);
    if (!program)
        return WTF::nullopt;
    auto matchedSemantics = matchSemantics(*program, renderPipelineDescriptor);
    if (!matchedSemantics)
        return WTF::nullopt;

    auto generatedCode = Metal::generateMetalCode(*program, WTFMove(*matchedSemantics), renderPipelineDescriptor.layout);

    RenderPrepareResult result;
    result.metalSource = WTFMove(generatedCode.metalSource);
    result.mangledVertexEntryPointName = WTFMove(generatedCode.mangledVertexEntryPointName);
    result.mangledFragmentEntryPointName = WTFMove(generatedCode.mangledFragmentEntryPointName);
    return result;
}

Optional<ComputePrepareResult> prepare(String& whlslSource, ComputePipelineDescriptor& computePipelineDescriptor)
{
    auto program = prepareShared(whlslSource);
    if (!program)
        return WTF::nullopt;
    auto matchedSemantics = matchSemantics(*program, computePipelineDescriptor);
    if (!matchedSemantics)
        return WTF::nullopt;

    auto generatedCode = Metal::generateMetalCode(*program, WTFMove(*matchedSemantics), computePipelineDescriptor.layout);

    ComputePrepareResult result;
    result.metalSource = WTFMove(generatedCode.metalSource);
    result.mangledEntryPointName = WTFMove(generatedCode.mangledEntryPointName);
    return result;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
