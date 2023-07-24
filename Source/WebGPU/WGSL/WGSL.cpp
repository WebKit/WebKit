/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#include "WGSL.h"

#include "CallGraph.h"
#include "ConstantRewriter.h"
#include "EntryPointRewriter.h"
#include "GlobalVariableRewriter.h"
#include "MangleNames.h"
#include "Metal/MetalCodeGenerator.h"
#include "Parser.h"
#include "PhaseTimer.h"
#include "TypeCheck.h"
#include "WGSLShaderModule.h"

namespace WGSL {

#define CHECK_PASS(name, pass, ...) \
    dumpASTBetweenEachPassIfNeeded(ast, "AST before " # pass); \
    auto name##Expected = [&]() { \
        PhaseTimer phaseTimer(#pass, phaseTimes); \
        return pass(__VA_ARGS__); \
    }(); \
    if (!name##Expected) { \
        if (dumpPassFailure) \
            dataLogLn("failed pass: " # pass, toString(name##Expected.error())); \
        return makeUnexpected(name##Expected.error()); \
    } \
    auto& name = *name##Expected; \

#define RUN_PASS(pass, ...) \
    do { \
        PhaseTimer phaseTimer(#pass, phaseTimes); \
        dumpASTBetweenEachPassIfNeeded(ast, "AST before " # pass); \
        pass(__VA_ARGS__); \
    } while (0)

#define RUN_PASS_WITH_RESULT(name, pass, ...) \
    dumpASTBetweenEachPassIfNeeded(ast, "AST before " # pass); \
    auto name = [&]() { \
        PhaseTimer phaseTimer(#pass, phaseTimes); \
        return pass(__VA_ARGS__); \
    }();

std::variant<SuccessfulCheck, FailedCheck> staticCheck(const String& wgsl, const std::optional<SourceMap>&, const Configuration& configuration)
{
    auto shaderModule = makeUniqueRef<ShaderModule>(wgsl, configuration);
    std::optional<Error> error = parse(shaderModule);
    if (error.has_value()) {
        // FIXME: Add support for returning multiple errors from the parser.
        return FailedCheck { { *error }, { /* warnings */ } };
    }

    // FIXME: add more validation
    auto maybeFailure = typeCheck(shaderModule);
    if (maybeFailure.has_value())
        return *maybeFailure;

    maybeFailure = rewriteConstants(shaderModule);
    if (maybeFailure.has_value())
        return *maybeFailure;

    Vector<Warning> warnings { };
    return std::variant<SuccessfulCheck, FailedCheck>(std::in_place_type<SuccessfulCheck>, WTFMove(warnings), WTFMove(shaderModule));
}

SuccessfulCheck::SuccessfulCheck(SuccessfulCheck&&) = default;

SuccessfulCheck::SuccessfulCheck(Vector<Warning>&& messages, UniqueRef<ShaderModule>&& shader)
    : warnings(WTFMove(messages))
    , ast(WTFMove(shader))
{
}

SuccessfulCheck::~SuccessfulCheck() = default;

inline PrepareResult prepareImpl(ShaderModule& ast, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts)
{
    ShaderModule::Compilation compilation(ast);

    PhaseTimes phaseTimes;
    PrepareResult result;

    {
        PhaseTimer phaseTimer("prepare total", phaseTimes);

        RUN_PASS_WITH_RESULT(callGraph, buildCallGraph, ast, pipelineLayouts);
        RUN_PASS(rewriteEntryPoints, callGraph, result);
        RUN_PASS(rewriteGlobalVariables, callGraph, pipelineLayouts, result);
        RUN_PASS(mangleNames, callGraph, result);

        dumpASTAtEndIfNeeded(ast);

        {
            PhaseTimer phaseTimer("generateMetalCode", phaseTimes);
            result.msl = Metal::generateMetalCode(callGraph);
        }
    }

    logPhaseTimes(phaseTimes);

    return result;
}

PrepareResult prepare(ShaderModule& ast, const HashMap<String, std::optional<PipelineLayout>>& pipelineLayouts)
{
    return prepareImpl(ast, pipelineLayouts);
}

PrepareResult prepare(ShaderModule& ast, const String& entryPointName, const std::optional<PipelineLayout>& pipelineLayout)
{
    HashMap<String, std::optional<PipelineLayout>> pipelineLayouts;
    pipelineLayouts.add(entryPointName, pipelineLayout);
    return prepareImpl(ast, pipelineLayouts);
}

}
