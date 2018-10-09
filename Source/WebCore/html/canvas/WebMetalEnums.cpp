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
#include "WebMetalEnums.h"

#if ENABLE(WEBMETAL)

#include "GPUEnums.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

std::optional<WebMetalCompareFunction> toWebMetalCompareFunction(const String& name)
{
    if (equalLettersIgnoringASCIICase(name, "never"))
        return WebMetalCompareFunction::Never;
    if (equalLettersIgnoringASCIICase(name, "less"))
        return WebMetalCompareFunction::Less;
    if (equalLettersIgnoringASCIICase(name, "equal"))
        return WebMetalCompareFunction::Equal;
    if (equalLettersIgnoringASCIICase(name, "lessequal"))
        return WebMetalCompareFunction::Lessequal;
    if (equalLettersIgnoringASCIICase(name, "greater"))
        return WebMetalCompareFunction::Greater;
    if (equalLettersIgnoringASCIICase(name, "notequal"))
        return WebMetalCompareFunction::Notequal;
    if (equalLettersIgnoringASCIICase(name, "greaterequal"))
        return WebMetalCompareFunction::Greaterequal;
    if (equalLettersIgnoringASCIICase(name, "always"))
        return WebMetalCompareFunction::Always;

    ASSERT_NOT_REACHED();
    return std::nullopt;
}

String web3DCompareFunctionName(WebMetalCompareFunction value)
{
    if (value == WebMetalCompareFunction::Never)
        return "never"_s;
    if (value == WebMetalCompareFunction::Less)
        return "less"_s;
    if (value == WebMetalCompareFunction::Equal)
        return "equal"_s;
    if (value == WebMetalCompareFunction::Lessequal)
        return "lessequal"_s;
    if (value == WebMetalCompareFunction::Greater)
        return "greater"_s;
    if (value == WebMetalCompareFunction::Notequal)
        return "notequal"_s;
    if (value == WebMetalCompareFunction::Greaterequal)
        return "greaterequal"_s;
    if (value == WebMetalCompareFunction::Always)
        return "always"_s;
    
    ASSERT_NOT_REACHED();
    return emptyString();
}

GPUCompareFunction toGPUCompareFunction(const WebMetalCompareFunction value)
{
    if (value == WebMetalCompareFunction::Never)
        return GPUCompareFunction::Never;
    if (value == WebMetalCompareFunction::Less)
        return GPUCompareFunction::Less;
    if (value == WebMetalCompareFunction::Equal)
        return GPUCompareFunction::Equal;
    if (value == WebMetalCompareFunction::Lessequal)
        return GPUCompareFunction::LessEqual;
    if (value == WebMetalCompareFunction::Greater)
        return GPUCompareFunction::Greater;
    if (value == WebMetalCompareFunction::Notequal)
        return GPUCompareFunction::NotEqual;
    if (value == WebMetalCompareFunction::Greaterequal)
        return GPUCompareFunction::GreaterEqual;
    if (value == WebMetalCompareFunction::Always)
        return GPUCompareFunction::Always;
    
    ASSERT_NOT_REACHED();
    return GPUCompareFunction::Never;
}

} // namespace WebCore

#endif
