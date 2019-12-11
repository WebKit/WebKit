/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CONTENT_EXTENSIONS)

#include "CachedResource.h"
#include <wtf/OptionSet.h>
#include <wtf/URL.h>

namespace WebCore {
namespace ContentExtensions {

enum class ResourceType : uint16_t {
    Invalid = 0x0000,
    Document = 0x0001,
    Image = 0x0002,
    StyleSheet = 0x0004,
    Script = 0x0008,
    Font = 0x0010,
    Raw = 0x0020,
    SVGDocument = 0x0040,
    Media = 0x0080,
    PlugInStream = 0x0100,
    Popup = 0x0200,
    // 0x0400 and 0x0800 are used by LoadType.
    Ping = 0x1000,
};
const uint16_t ResourceTypeMask = 0x13FF;

enum class LoadType : uint16_t {
    Invalid = 0x0000,
    FirstParty = 0x0400,
    ThirdParty = 0x0800,
};
const uint16_t LoadTypeMask = 0x0C00;

static_assert(!(ResourceTypeMask & LoadTypeMask), "ResourceTypeMask and LoadTypeMask should be mutually exclusive because they are stored in the same uint16_t");

typedef uint16_t ResourceFlags;

// The first 32 bits of a uint64_t action are used for the action location.
// The next 16 bits are used for the flags (ResourceType and LoadType).
// The next bit is used to mark actions that are from a rule with an if-domain.
//     Actions from rules with unless-domain conditions are distinguished from
//     rules with if-domain conditions by not having this bit set.
//     Actions from rules with no conditions are put in the DFA without conditions.
// The values -1 and -2 are used for removed and empty values in HashTables.
const uint64_t ActionFlagMask = 0x0000FFFF00000000;
const uint64_t IfConditionFlag = 0x0001000000000000;

ResourceType toResourceType(CachedResource::Type);
uint16_t readResourceType(const String&);
uint16_t readLoadType(const String&);

struct ResourceLoadInfo {
    URL resourceURL;
    URL mainDocumentURL;
    OptionSet<ResourceType> type;

    bool isThirdParty() const;
    ResourceFlags getResourceFlags() const;
};

} // namespace ContentExtensions
} // namespace WebCore

#endif
