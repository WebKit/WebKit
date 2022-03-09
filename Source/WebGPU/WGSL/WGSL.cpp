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

#include "Parser.h"

namespace WGSL {

std::variant<SuccessfulCheck, FailedCheck> staticCheck(const String& wgsl, const std::optional<SourceMap>&)
{
    Expected<AST::ShaderModule, Error> parserResult = wgsl.is8Bit() ? parseLChar(wgsl) : parseUChar(wgsl);
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

PrepareResult prepare(const AST::ShaderModule& ast, const HashMap<String, PipelineLayout>& pipelineLayouts)
{
    UNUSED_PARAM(ast);
    UNUSED_PARAM(pipelineLayouts);
    return { String(), { } };
}

PrepareResult prepare(const AST::ShaderModule& ast, const String& entryPointName, const std::optional<PipelineLayout>& pipelineLayouts)
{
    UNUSED_PARAM(ast);
    UNUSED_PARAM(entryPointName);
    UNUSED_PARAM(pipelineLayouts);
    return { String(), { } };
}

}
