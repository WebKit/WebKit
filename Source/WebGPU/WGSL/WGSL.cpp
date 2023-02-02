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
#include "EntryPointRewriter.h"
#include "GlobalVariableRewriter.h"
#include "MangleNames.h"
#include "Metal/MetalCodeGenerator.h"
#include "Parser.h"
#include "PhaseTimer.h"
#include "ResolveTypeReferences.h"
#include "TypeCheck.h"

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
    Expected<AST::ShaderModule, Error> parserResult = wgsl.is8Bit() ? parseLChar(wgsl, configuration) : parseUChar(wgsl, configuration);
    if (!parserResult.has_value()) {
        // FIXME: Add support for returning multiple errors from the parser.
        return FailedCheck { { parserResult.error() }, { /* warnings */ } };
    }
    UniqueRef<AST::ShaderModule> shader = makeUniqueRef<AST::ShaderModule>(WTFMove(parserResult.value()));

    Vector<Warning> warnings { };
    // FIXME: add validation
    return std::variant<SuccessfulCheck, FailedCheck>(std::in_place_type<SuccessfulCheck>, WTFMove(warnings), WTFMove(shader));
}

SuccessfulCheck::SuccessfulCheck(SuccessfulCheck&&) = default;

SuccessfulCheck::SuccessfulCheck(Vector<Warning>&& messages, UniqueRef<AST::ShaderModule>&& shader)
    : warnings(WTFMove(messages))
    , ast(WTFMove(shader))
{
}

SuccessfulCheck::~SuccessfulCheck() = default;

PrepareResult prepare(AST::ShaderModule& ast, const HashMap<String, PipelineLayout>& pipelineLayouts)
{
    PhaseTimes phaseTimes;
    PrepareResult result;

    {
        PhaseTimer phaseTimer("prepare total", phaseTimes);

        // FIXME: this should run as part of staticCheck
        RUN_PASS(typeCheck, ast);
        RUN_PASS_WITH_RESULT(callGraph, buildCallGraph, ast);
        RUN_PASS(resolveTypeReferences, ast);
        RUN_PASS(rewriteEntryPoints, callGraph);
        RUN_PASS(rewriteGlobalVariables, callGraph, pipelineLayouts);
        RUN_PASS_WITH_RESULT(entryPointMap, mangleNames, callGraph);

        for (const auto& it : entryPointMap) {
            Reflection::EntryPointInformation information;
            information.mangledName = it.value;
            auto addResult = result.entryPoints.add(it.key, WTFMove(information));
            ASSERT_UNUSED(addResult, addResult.isNewEntry);
        }

        dumpASTAtEndIfNeeded(ast);

        {
            PhaseTimer phaseTimer("generateMetalCode", phaseTimes);
            result.msl = Metal::generateMetalCode(ast).metalSource.toString();
        }
    }

    logPhaseTimes(phaseTimes);

    return result;
}

PrepareResult prepare(AST::ShaderModule& ast, const String& entryPointName, const std::optional<PipelineLayout>& pipelineLayouts)
{
    UNUSED_PARAM(entryPointName);
    UNUSED_PARAM(pipelineLayouts);
    Metal::RenderMetalCode metalCode = Metal::generateMetalCode(ast);
    return { metalCode.metalSource.toString(), { } };
}

}
