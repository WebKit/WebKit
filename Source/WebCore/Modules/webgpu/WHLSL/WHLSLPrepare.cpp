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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLASTDumper.h"
#include "WHLSLCheckDuplicateFunctions.h"
#include "WHLSLCheckReferenceTypes.h"
#include "WHLSLCheckTextureReferences.h"
#include "WHLSLChecker.h"
#include "WHLSLComputeDimensions.h"
#include "WHLSLFunctionStageChecker.h"
#include "WHLSLHighZombieFinder.h"
#include "WHLSLLiteralTypeChecker.h"
#include "WHLSLMetalCodeGenerator.h"
#include "WHLSLNameResolver.h"
#include "WHLSLNameSpace.h"
#include "WHLSLParser.h"
#include "WHLSLPreserveVariableLifetimes.h"
#include "WHLSLProgram.h"
#include "WHLSLPropertyResolver.h"
#include "WHLSLPruneUnreachableStandardLibraryFunctions.h"
#include "WHLSLRecursionChecker.h"
#include "WHLSLRecursiveTypeChecker.h"
#include "WHLSLSemanticMatcher.h"
#include "WHLSLStandardLibraryUtilities.h"
#include "WHLSLStatementBehaviorChecker.h"
#include "WHLSLSynthesizeConstructors.h"
#include "WHLSLSynthesizeEnumerationFunctions.h"
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>

namespace WebCore {

namespace WHLSL {

struct ShaderModule {
    WTF_MAKE_FAST_ALLOCATED;
public:

    ShaderModule(const String& whlslSource)
        : whlslSource(whlslSource)
    {
    }

    String whlslSource;
};

}

}

namespace std {

void default_delete<WebCore::WHLSL::ShaderModule>::operator()(WebCore::WHLSL::ShaderModule* shaderModule) const
{
    delete shaderModule;
}

}

namespace WebCore {

namespace WHLSL {

static constexpr bool dumpASTBeforeEachPass = false;
static constexpr bool dumpASTAfterParsing = false;
static constexpr bool dumpASTAtEnd = false;
static constexpr bool alwaysDumpPassFailures = false;
static constexpr bool dumpPassFailure = dumpASTBeforeEachPass || dumpASTAfterParsing || dumpASTAtEnd || alwaysDumpPassFailures;
static constexpr bool dumpPhaseTimes = false;

static constexpr bool parseFullStandardLibrary = false;

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

using PhaseTimes = Vector<std::pair<String, Seconds>>;

static void logPhaseTimes(PhaseTimes& phaseTimes)
{
    if (!dumpPhaseTimes)
        return;

    for (auto& entry : phaseTimes)
        dataLogLn(entry.first, ": ", entry.second.milliseconds(), " ms");
}

class PhaseTimer {
public:
    PhaseTimer(const char* phaseName, PhaseTimes& phaseTimes)
        : m_phaseTimes(phaseTimes)
    {
        if (dumpPhaseTimes) {
            m_phaseName = phaseName;
            m_start = MonotonicTime::now();
        }
    }

    ~PhaseTimer()
    {
        if (dumpPhaseTimes) {
            auto totalTime = MonotonicTime::now() - m_start;
            m_phaseTimes.append({ m_phaseName, totalTime });
        }
    }

private:
    String m_phaseName;
    PhaseTimes& m_phaseTimes;
    MonotonicTime m_start;
};

UniqueRef<ShaderModule> createShaderModule(const String& whlslSource)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=200872 We should consider moving as much work from prepare() into here as possible.
    return makeUniqueRef<ShaderModule>(whlslSource);
}

#define CHECK_PASS(pass, ...) \
    do { \
        dumpASTBetweenEachPassIfNeeded(program, "AST before " # pass); \
        PhaseTimer phaseTimer(#pass, phaseTimes); \
        auto result = pass(__VA_ARGS__); \
        if (!result) { \
            if (dumpPassFailure) \
                dataLogLn("failed pass: " # pass, Lexer::errorString(result.error(), whlslSource1, whlslSource2)); \
            return makeUnexpected(Lexer::errorString(result.error(), whlslSource1, whlslSource2)); \
        } \
    } while (0)

#define RUN_PASS(pass, ...) \
    do { \
        PhaseTimer phaseTimer(#pass, phaseTimes); \
        dumpASTBetweenEachPassIfNeeded(program, "AST before " # pass); \
        pass(__VA_ARGS__); \
    } while (0)

static Expected<Program, String> prepareShared(PhaseTimes& phaseTimes, const String& whlslSource1, const String* whlslSource2 = nullptr)
{
    Program program;
    Parser parser;

    {
        program.nameContext().setCurrentNameSpace(AST::NameSpace::NameSpace1);

        PhaseTimer phaseTimer("parse", phaseTimes);
        auto parseResult = parser.parse(program, whlslSource1, ParsingMode::User, AST::NameSpace::NameSpace1);
        if (!parseResult) {
            if (dumpPassFailure)
                dataLogLn("failed to parse the program: ", Lexer::errorString(parseResult.error(), whlslSource1, whlslSource2));
            return makeUnexpected(Lexer::errorString(parseResult.error(), whlslSource1, whlslSource2));
        }
        if (whlslSource2) {
            program.nameContext().setCurrentNameSpace(AST::NameSpace::NameSpace2);
            auto parseResult = parser.parse(program, *whlslSource2, ParsingMode::User, AST::NameSpace::NameSpace2);
            if (!parseResult) {
                if (dumpPassFailure)
                    dataLogLn("failed to parse the program: ", Lexer::errorString(parseResult.error(), whlslSource1, whlslSource2));
                return makeUnexpected(Lexer::errorString(parseResult.error(), whlslSource1, whlslSource2));
            }
        }
        program.nameContext().setCurrentNameSpace(AST::NameSpace::StandardLibrary);
    }

    {
        PhaseTimer phaseTimer("includeStandardLibrary", phaseTimes);
        includeStandardLibrary(program, parser, parseFullStandardLibrary);
    }

    if (!dumpASTBetweenEachPassIfNeeded(program, "AST after parsing"))
        dumpASTAfterParsingIfNeeded(program);

    NameResolver nameResolver(program.nameContext());
    CHECK_PASS(resolveNamesInTypes, program, nameResolver);
    CHECK_PASS(checkRecursiveTypes, program);
    CHECK_PASS(synthesizeEnumerationFunctions, program);
    CHECK_PASS(resolveTypeNamesInFunctions, program, nameResolver);
    CHECK_PASS(synthesizeConstructors, program);
    CHECK_PASS(checkDuplicateFunctions, program);

    CHECK_PASS(check, program);
    RUN_PASS(pruneUnreachableStandardLibraryFunctions, program);

    RUN_PASS(checkLiteralTypes, program);
    CHECK_PASS(checkTextureReferences, program);
    CHECK_PASS(checkReferenceTypes, program);
    RUN_PASS(resolveProperties, program);
    RUN_PASS(findHighZombies, program);
    CHECK_PASS(checkStatementBehavior, program);
    CHECK_PASS(checkRecursion, program);
    CHECK_PASS(checkFunctionStages, program);
    RUN_PASS(preserveVariableLifetimes, program);

    dumpASTAtEndIfNeeded(program);

    return program;
}

Expected<RenderPrepareResult, String> prepare(const ShaderModule& vertexShaderModule, const ShaderModule* fragmentShaderModule, RenderPipelineDescriptor& renderPipelineDescriptor)
{
    PhaseTimes phaseTimes;
    Metal::RenderMetalCode generatedCode;

    {
        PhaseTimer phaseTimer("prepare total", phaseTimes);
        const String* secondShader = nullptr;
        bool distinctFragmentShader = false;
        if (fragmentShaderModule && fragmentShaderModule != &vertexShaderModule) {
            secondShader = &fragmentShaderModule->whlslSource;
            distinctFragmentShader = true;
        }
        auto program = prepareShared(phaseTimes, vertexShaderModule.whlslSource, secondShader);
        if (!program)
            return makeUnexpected(program.error());

        Optional<MatchedRenderSemantics> matchedSemantics;
        {
            PhaseTimer phaseTimer("matchSemantics", phaseTimes);
            matchedSemantics = matchSemantics(*program, renderPipelineDescriptor, distinctFragmentShader, fragmentShaderModule);
            if (!matchedSemantics)
                return makeUnexpected(Lexer::errorString(Error("Could not match semantics"_str), vertexShaderModule.whlslSource, secondShader));
        }

        {
            PhaseTimer phaseTimer("generateMetalCode", phaseTimes);
            generatedCode = Metal::generateMetalCode(*program, WTFMove(*matchedSemantics), renderPipelineDescriptor.layout);
        }
    }

    logPhaseTimes(phaseTimes);

    RenderPrepareResult result;
    result.metalSource = WTFMove(generatedCode.metalSource);
    result.mangledVertexEntryPointName = WTFMove(generatedCode.mangledVertexEntryPointName);
    result.mangledFragmentEntryPointName = WTFMove(generatedCode.mangledFragmentEntryPointName);
    return result;
}

Expected<ComputePrepareResult, String> prepare(const ShaderModule& shaderModule, ComputePipelineDescriptor& computePipelineDescriptor)
{
    PhaseTimes phaseTimes;
    Metal::ComputeMetalCode generatedCode;
    Optional<ComputeDimensions> computeDimensions;

    {
        PhaseTimer phaseTimer("prepare total", phaseTimes);
        auto program = prepareShared(phaseTimes, shaderModule.whlslSource);
        if (!program)
            return makeUnexpected(program.error());

        Optional<MatchedComputeSemantics> matchedSemantics;
        {
            PhaseTimer phaseTimer("matchSemantics", phaseTimes);
            matchedSemantics = matchSemantics(*program, computePipelineDescriptor);
            if (!matchedSemantics)
                return makeUnexpected(Lexer::errorString(Error("Could not match semantics"_str), shaderModule.whlslSource));
        }

        {
            PhaseTimer phaseTimer("computeDimensions", phaseTimes);
            computeDimensions = WHLSL::computeDimensions(*program, *matchedSemantics->shader);
            if (!computeDimensions)
                return makeUnexpected(Lexer::errorString(Error("Could not match compute dimensions"_str), shaderModule.whlslSource));
        }

        {
            PhaseTimer phaseTimer("generateMetalCode", phaseTimes);
            generatedCode = Metal::generateMetalCode(*program, WTFMove(*matchedSemantics), computePipelineDescriptor.layout);
        }
    }

    logPhaseTimes(phaseTimes);

    ComputePrepareResult result;
    result.metalSource = WTFMove(generatedCode.metalSource);
    result.mangledEntryPointName = WTFMove(generatedCode.mangledEntryPointName);
    result.computeDimensions = WTFMove(*computeDimensions);
    return result;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WHLSL_COMPILER)
