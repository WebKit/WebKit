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
#include <wtf/Vector.h>

namespace WebCore::ContentExtensions {

enum class ActionCondition : uint32_t {
    None = 0x00000,
    IfTopURL = 0x40000,
    UnlessTopURL = 0x80000,
    IfFrameURL = 0x140000,
    UnlessFrameURL = 0x180000,
    IfAncestorSubframeURL = 0x1C0000
};
static constexpr uint32_t ActionConditionMask = 0x1C0000;

enum class ResourceType : uint32_t {
    TopDocument = 0x0001,
    ChildDocument = 0x0002,
    Image = 0x0004,
    StyleSheet = 0x0008,
    Script = 0x0010,
    Font = 0x0020,
    SVGDocument = 0x0040,
    Media = 0x0080,
    Popup = 0x0100,
    Ping = 0x0200,
    Fetch = 0x0400,
    WebSocket = 0x0800,
    CSPReport = 0x1000,
    Other = 0x2000,
};
static constexpr uint32_t ResourceTypeMask = 0x3FFF;

enum class LoadType : uint32_t {
    FirstParty = 0x4000,
    ThirdParty = 0x8000,
};
static constexpr uint32_t LoadTypeMask = 0xC000;

enum class LoadContext : uint32_t {
    TopFrame = 0x10000,
    ChildFrame = 0x20000,
};
static constexpr uint32_t LoadContextMask = 0x30000;

enum class RequestMethod : uint32_t {
    None = 0x0000000,
    Get = 0x200000,
    Head = 0x400000,
    Options = 0x600000,
    Trace = 0x800000,
    Put = 0xA00000,
    Delete = 0xC00000,
    Post = 0xE00000,
    Patch = 0x1000000,
    Connect = 0x1200000,
};
static constexpr uint32_t RequestMethodMask = 0x1E00000;

using ResourceFlags = uint32_t;

constexpr ResourceFlags AllResourceFlags = LoadTypeMask | ResourceTypeMask | LoadContextMask | ActionConditionMask | RequestMethodMask;

// The first 32 bits of a uint64_t action are used for the action location.
// The next 25 bits are used for the flags (ResourceType, LoadType, LoadContext, ActionCondition, RequestMethod).
// The values -1 and -2 are used for removed and empty values in HashTables.
static constexpr uint64_t ActionFlagMask = 0x01FFFFFF00000000;

OptionSet<ResourceType> toResourceType(CachedResource::Type, ResourceRequestRequester, bool isMainFrame);
std::optional<OptionSet<ResourceType>> readResourceType(StringView);
std::optional<OptionSet<LoadType>> readLoadType(StringView);
std::optional<OptionSet<LoadContext>> readLoadContext(StringView);
std::optional<RequestMethod> readRequestMethod(StringView);

ASCIILiteral resourceTypeToString(OptionSet<ResourceType>);

struct ResourceLoadInfo {
    URL resourceURL;
    URL mainDocumentURL;
    URL frameURL;
    OptionSet<ResourceType> type;
    bool mainFrameContext { false };
    RequestMethod requestMethod { RequestMethod::None };
    Vector<URL> ancestorSubframeURLs { };

    bool isThirdParty() const;
    ResourceFlags getResourceFlags() const;
    ResourceLoadInfo isolatedCopy() const & { return { resourceURL.isolatedCopy(), mainDocumentURL.isolatedCopy(), frameURL.isolatedCopy(), type, mainFrameContext, requestMethod }; }
    ResourceLoadInfo isolatedCopy() && { return { WTFMove(resourceURL).isolatedCopy(), WTFMove(mainDocumentURL).isolatedCopy(), WTFMove(frameURL).isolatedCopy(), type, mainFrameContext, requestMethod }; }
};

} // namespace WebCore::ContentExtensions

#endif
