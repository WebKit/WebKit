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
#include "WHLSLAutoInitializeVariables.h"
#include "WHLSLCheckDuplicateFunctions.h"
#include "WHLSLCheckTextureReferences.h"
#include "WHLSLChecker.h"
#include "WHLSLComputeDimensions.h"
#include "WHLSLFunctionStageChecker.h"
#include "WHLSLHighZombieFinder.h"
#include "WHLSLLiteralTypeChecker.h"
#include "WHLSLMetalCodeGenerator.h"
#include "WHLSLNameResolver.h"
#include "WHLSLParser.h"
#include "WHLSLPreserveVariableLifetimes.h"
#include "WHLSLProgram.h"
#include "WHLSLPropertyResolver.h"
#include "WHLSLRecursionChecker.h"
#include "WHLSLRecursiveTypeChecker.h"
#include "WHLSLSemanticMatcher.h"
#include "WHLSLStandardLibraryUtilities.h"
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
static constexpr bool alwaysDumpPassFailures = false;
static constexpr bool dumpPassFailure = dumpASTBeforeEachPass || dumpASTAfterParsing || dumpASTAtEnd || alwaysDumpPassFailures;

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

#define CHECK_PASS(pass, ...) \
    do { \
        dumpASTBetweenEachPassIfNeeded(program, "AST before " # pass); \
        if (!pass(__VA_ARGS__)) { \
            if (dumpPassFailure) \
                dataLogLn("failed pass: " # pass); \
            return WTF::nullopt; \
        } \
    } while (0)

#define RUN_PASS(pass, ...) \
    do { \
        dumpASTBetweenEachPassIfNeeded(program, "AST before " # pass); \
        pass(__VA_ARGS__); \
    } while (0)

static Optional<Program> prepareShared(String& whlslSource)
{
    Program program;
    Parser parser;
    if (auto parseFailure = parser.parse(program, whlslSource, Parser::Mode::User)) {
        if (dumpPassFailure)
            dataLogLn("failed to parse the program: ", *parseFailure);
        return WTF::nullopt;
    }
    includeStandardLibrary(program, parser);

    if (!dumpASTBetweenEachPassIfNeeded(program, "AST after parsing"))
        dumpASTAfterParsingIfNeeded(program);

    NameResolver nameResolver(program.nameContext());
    CHECK_PASS(resolveNamesInTypes, program, nameResolver);
    CHECK_PASS(checkRecursiveTypes, program);
    CHECK_PASS(synthesizeStructureAccessors, program);
    CHECK_PASS(synthesizeEnumerationFunctions, program);
    CHECK_PASS(synthesizeArrayOperatorLength, program);
    CHECK_PASS(resolveTypeNamesInFunctions, program, nameResolver);
    CHECK_PASS(synthesizeConstructors, program);
    CHECK_PASS(checkDuplicateFunctions, program);

    CHECK_PASS(check, program);

    RUN_PASS(checkLiteralTypes, program);
    CHECK_PASS(checkTextureReferences, program);
    CHECK_PASS(autoInitializeVariables, program);
    RUN_PASS(resolveProperties, program);
    RUN_PASS(findHighZombies, program);
    CHECK_PASS(checkStatementBehavior, program);
    CHECK_PASS(checkRecursion, program);
    CHECK_PASS(checkFunctionStages, program);
    RUN_PASS(preserveVariableLifetimes, program);

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
    auto computeDimensions = WHLSL::computeDimensions(*program, *matchedSemantics->shader);
    if (!computeDimensions)
        return WTF::nullopt;

    auto generatedCode = Metal::generateMetalCode(*program, WTFMove(*matchedSemantics), computePipelineDescriptor.layout);

    ComputePrepareResult result;
    result.metalSource = WTFMove(generatedCode.metalSource);
    result.mangledEntryPointName = WTFMove(generatedCode.mangledEntryPointName);
    result.computeDimensions = WTFMove(*computeDimensions);
    return result;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
