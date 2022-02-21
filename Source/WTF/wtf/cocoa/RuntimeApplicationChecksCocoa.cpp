/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#include <wtf/NeverDestroyed.h>

namespace WTF {

static std::optional<uint32_t>& applicationSDKVersionOverride()
{
    static std::optional<uint32_t> version;
    return version;
}

void setApplicationSDKVersion(uint32_t version)
{
    applicationSDKVersionOverride() = version;
}

uint32_t applicationSDKVersion()
{
    if (applicationSDKVersionOverride())
        return *applicationSDKVersionOverride();
    return dyld_get_program_sdk_version();
}

static std::optional<LinkedOnOrAfterOverride>& linkedOnOrAfterOverrideValue()
{
    static std::optional<LinkedOnOrAfterOverride> linkedOnOrAfter;
    return linkedOnOrAfter;
}

void setLinkedOnOrAfterOverride(std::optional<LinkedOnOrAfterOverride> linkedOnOrAfter)
{
    linkedOnOrAfterOverrideValue() = linkedOnOrAfter;
}

std::optional<LinkedOnOrAfterOverride> linkedOnOrAfterOverride()
{
    return linkedOnOrAfterOverrideValue();
}

bool linkedOnOrAfter(SDKVersion sdkVersion)
{
    if (auto overrideValue = linkedOnOrAfterOverride())
        return *overrideValue == LinkedOnOrAfterOverride::AfterEverything;

    auto sdkVersionAsInteger = static_cast<uint32_t>(sdkVersion);
    return sdkVersionAsInteger && applicationSDKVersion() >= sdkVersionAsInteger;
}

}
