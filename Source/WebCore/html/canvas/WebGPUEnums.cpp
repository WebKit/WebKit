/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebGPUEnums.h"

#if ENABLE(WEBGPU)

#include "GPUEnums.h"

namespace WebCore {

std::optional<WebGPUCompareFunction> toWebGPUCompareFunction(const String& name)
{
    if (equalLettersIgnoringASCIICase(name, "never"))
        return WebGPUCompareFunction::Never;
    if (equalLettersIgnoringASCIICase(name, "less"))
        return WebGPUCompareFunction::Less;
    if (equalLettersIgnoringASCIICase(name, "equal"))
        return WebGPUCompareFunction::Equal;
    if (equalLettersIgnoringASCIICase(name, "lessequal"))
        return WebGPUCompareFunction::Lessequal;
    if (equalLettersIgnoringASCIICase(name, "greater"))
        return WebGPUCompareFunction::Greater;
    if (equalLettersIgnoringASCIICase(name, "notequal"))
        return WebGPUCompareFunction::Notequal;
    if (equalLettersIgnoringASCIICase(name, "greaterequal"))
        return WebGPUCompareFunction::Greaterequal;
    if (equalLettersIgnoringASCIICase(name, "always"))
        return WebGPUCompareFunction::Always;

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

String web3DCompareFunctionName(const WebGPUCompareFunction value)
{
    if (value == WebGPUCompareFunction::Never)
        return "never"_s;
    if (value == WebGPUCompareFunction::Less)
        return "less"_s;
    if (value == WebGPUCompareFunction::Equal)
        return "equal"_s;
    if (value == WebGPUCompareFunction::Lessequal)
        return "lessequal"_s;
    if (value == WebGPUCompareFunction::Greater)
        return "greater"_s;
    if (value == WebGPUCompareFunction::Notequal)
        return "notequal"_s;
    if (value == WebGPUCompareFunction::Greaterequal)
        return "greaterequal"_s;
    if (value == WebGPUCompareFunction::Always)
        return "always"_s;
    
    ASSERT_NOT_REACHED();
    return emptyString();
}

GPUCompareFunction toGPUCompareFunction(const WebGPUCompareFunction value)
{
    if (value == WebGPUCompareFunction::Never)
        return GPUCompareFunction::Never;
    if (value == WebGPUCompareFunction::Less)
        return GPUCompareFunction::Less;
    if (value == WebGPUCompareFunction::Equal)
        return GPUCompareFunction::Equal;
    if (value == WebGPUCompareFunction::Lessequal)
        return GPUCompareFunction::LessEqual;
    if (value == WebGPUCompareFunction::Greater)
        return GPUCompareFunction::Greater;
    if (value == WebGPUCompareFunction::Notequal)
        return GPUCompareFunction::NotEqual;
    if (value == WebGPUCompareFunction::Greaterequal)
        return GPUCompareFunction::GreaterEqual;
    if (value == WebGPUCompareFunction::Always)
        return GPUCompareFunction::Always;
    
    ASSERT_NOT_REACHED();
    return GPUCompareFunction::Never;
}

} // namespace WebCore

#endif
