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
    Document = 0x0001,
    Image = 0x0002,
    StyleSheet = 0x0004,
    Script = 0x0008,
    Font = 0x0010,
    SVGDocument = 0x0020,
    Media = 0x0040,
    Popup = 0x0080,
    Ping = 0x0100,
    Fetch = 0x0200,
    WebSocket = 0x0400,
    Other = 0x0800,
};
const uint16_t ResourceTypeMask = 0x0FFF;

enum class LoadType : uint16_t {
    FirstParty = 0x1000,
    ThirdParty = 0x2000,
};
const uint16_t LoadTypeMask = 0x3000;

enum class LoadContext : uint16_t {
    TopFrame = 0x4000,
    ChildFrame = 0x8000,
};
const uint16_t LoadContextMask = 0xC000;

static_assert(!(ResourceTypeMask & LoadTypeMask), "ResourceTypeMask and LoadTypeMask should be mutually exclusive because they are stored in the same uint16_t");
static_assert(!(ResourceTypeMask & LoadContextMask), "ResourceTypeMask and LoadContextMask should be mutually exclusive because they are stored in the same uint16_t");
static_assert(!(LoadContextMask & LoadTypeMask), "LoadContextMask and LoadTypeMask should be mutually exclusive because they are stored in the same uint16_t");

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

OptionSet<ResourceType> toResourceType(CachedResource::Type, ResourceRequestBase::Requester);
std::optional<OptionSet<ResourceType>> readResourceType(StringView);
std::optional<OptionSet<LoadType>> readLoadType(StringView);
std::optional<OptionSet<LoadContext>> readLoadContext(StringView);

struct ResourceLoadInfo {
    URL resourceURL;
    URL mainDocumentURL;
    OptionSet<ResourceType> type;
    bool mainFrameContext { false };

    bool isThirdParty() const;
    ResourceFlags getResourceFlags() const;
};

} // namespace ContentExtensions
} // namespace WebCore

#endif
