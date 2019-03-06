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
#include <wtf/Variant.h>

namespace WebCore {

namespace WHLSL {

static Optional<Program> prepareShared(String& whlslSource)
{
    Program program;
    Parser parser;
    auto standardLibrary = String::fromUTF8(WHLSLStandardLibrary, sizeof(WHLSLStandardLibrary));
    auto failure = static_cast<bool>(parser.parse(program, standardLibrary, Parser::Mode::StandardLibrary));
    ASSERT_UNUSED(failure, !failure);
    if (parser.parse(program, whlslSource, Parser::Mode::User))
        return WTF::nullopt;
    NameResolver nameResolver(program.nameContext());
    if (!resolveNamesInTypes(program, nameResolver))
        return WTF::nullopt;
    if (!checkRecursiveTypes(program))
        return WTF::nullopt;
    synthesizeStructureAccessors(program);
    synthesizeEnumerationFunctions(program);
    synthesizeArrayOperatorLength(program);
    synthesizeConstructors(program);
    resolveNamesInFunctions(program, nameResolver);
    if (!checkDuplicateFunctions(program))
        return WTF::nullopt;

    if (!check(program))
        return WTF::nullopt;
    checkLiteralTypes(program);
    // resolveProperties(program);
    findHighZombies(program);
    if (!checkStatementBehavior(program))
        return WTF::nullopt;
    if (!checkRecursion(program))
        return WTF::nullopt;
    if (!checkFunctionStages(program))
        return WTF::nullopt;
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
    result.vertexMappedBindGroups = WTFMove(generatedCode.vertexMappedBindGroups);
    result.fragmentMappedBindGroups = WTFMove(generatedCode.fragmentMappedBindGroups);
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
    result.mappedBindGroups = WTFMove(generatedCode.bindGroups);
    return result;
}

} // namespace WHLSL

} // namespace WebCore

#endif // ENABLE(WEBGPU)
