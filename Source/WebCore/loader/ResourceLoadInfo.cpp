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

#include "config.h"
#include "ResourceLoadInfo.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionActions.h"
#include "RegistrableDomain.h"
#include "SecurityOrigin.h"

namespace WebCore::ContentExtensions {

static_assert(!(ResourceTypeMask & LoadTypeMask), "ResourceTypeMask and LoadTypeMask should be mutually exclusive because they are stored in the same uint32_t");
static_assert(!(ResourceTypeMask & LoadContextMask), "ResourceTypeMask and LoadContextMask should be mutually exclusive because they are stored in the same uint32_t");
static_assert(!(ResourceTypeMask & ActionConditionMask), "ResourceTypeMask and ActionConditionMask should be mutually exclusive because they are stored in the same uint32_t");
static_assert(!(LoadContextMask & LoadTypeMask), "LoadContextMask and LoadTypeMask should be mutually exclusive because they are stored in the same uint32_t");
static_assert(!(LoadContextMask & ActionConditionMask), "LoadContextMask and ActionConditionMask should be mutually exclusive because they are stored in the same uint32_t");
static_assert(!(LoadTypeMask & ActionConditionMask), "LoadTypeMask and ActionConditionMask should be mutually exclusive because they are stored in the same uint32_t");
static_assert(static_cast<uint64_t>(AllResourceFlags) << 32 == ActionFlagMask, "ActionFlagMask should cover all the action flags");

OptionSet<ResourceType> toResourceType(CachedResource::Type type, ResourceRequestRequester requester, bool isMainFrame)
{
    switch (type) {
    case CachedResource::Type::LinkPrefetch:
    case CachedResource::Type::MainResource:
        if (isMainFrame)
            return { ResourceType::TopDocument };
        return { ResourceType::ChildDocument };
    case CachedResource::Type::SVGDocumentResource:
        return { ResourceType::SVGDocument };
    case CachedResource::Type::ImageResource:
        return { ResourceType::Image };
    case CachedResource::Type::CSSStyleSheet:
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
        return { ResourceType::StyleSheet };

    case CachedResource::Type::JSON:
    case CachedResource::Type::Script:
        return { ResourceType::Script };

    case CachedResource::Type::FontResource:
    case CachedResource::Type::SVGFontResource:
        return { ResourceType::Font };

    case CachedResource::Type::MediaResource:
        return { ResourceType::Media };

    case CachedResource::Type::RawResource:
        if (requester == ResourceRequestRequester::XHR
            || requester == ResourceRequestRequester::Fetch)
            return { ResourceType::Fetch };
        [[fallthrough]];
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::Icon:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::EnvironmentMapResource:
    case CachedResource::Type::ModelResource:
#endif
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
        return { ResourceType::Other };

#if ENABLE(VIDEO)
    case CachedResource::Type::TextTrackResource:
        return { ResourceType::Media };
#endif

    };
    ASSERT_NOT_REACHED();
    return { };
}

std::optional<OptionSet<ResourceType>> readResourceType(StringView name)
{
    if (name == "document"_s)
        return { { ResourceType::TopDocument, ResourceType::ChildDocument } };
    if (name == "top-document"_s)
        return { ResourceType::TopDocument };
    if (name == "child-document"_s)
        return { ResourceType::ChildDocument };
    if (name == "image"_s)
        return { ResourceType::Image };
    if (name == "style-sheet"_s)
        return { ResourceType::StyleSheet };
    if (name == "script"_s)
        return { ResourceType::Script };
    if (name == "font"_s)
        return { ResourceType::Font };
    if (name == "raw"_s)
        return { { ResourceType::Fetch, ResourceType::WebSocket, ResourceType::Other, ResourceType::Ping } };
    if (name == "websocket"_s)
        return { ResourceType::WebSocket };
    if (name == "fetch"_s)
        return { ResourceType::Fetch };
    if (name == "other"_s)
        return { { ResourceType::Other, ResourceType::Ping, ResourceType::CSPReport } };
    if (name == "svg-document"_s)
        return { ResourceType::SVGDocument };
    if (name == "media"_s)
        return { ResourceType::Media };
    if (name == "popup"_s)
        return { ResourceType::Popup };
    if (name == "ping"_s)
        return { ResourceType::Ping };
    if (name == "csp-report"_s)
        return { ResourceType::CSPReport };
    return std::nullopt;
}

std::optional<OptionSet<LoadType>> readLoadType(StringView name)
{
    if (name == "first-party"_s)
        return { LoadType::FirstParty };
    if (name == "third-party"_s)
        return { LoadType::ThirdParty };
    return std::nullopt;
}

std::optional<OptionSet<LoadContext>> readLoadContext(StringView name)
{
    if (name == "top-frame"_s)
        return { LoadContext::TopFrame };
    if (name == "child-frame"_s)
        return { LoadContext::ChildFrame };
    return std::nullopt;
}

std::optional<RequestMethod> readRequestMethod(StringView name)
{
    if (equalIgnoringASCIICase(name, "get"_s))
        return RequestMethod::Get;
    if (equalIgnoringASCIICase(name, "head"_s))
        return RequestMethod::Head;
    if (equalIgnoringASCIICase(name, "options"_s))
        return RequestMethod::Options;
    if (equalIgnoringASCIICase(name, "trace"_s))
        return RequestMethod::Trace;
    if (equalIgnoringASCIICase(name, "put"_s))
        return RequestMethod::Put;
    if (equalIgnoringASCIICase(name, "delete"_s))
        return RequestMethod::Delete;
    if (equalIgnoringASCIICase(name, "post"_s))
        return RequestMethod::Post;
    if (equalIgnoringASCIICase(name, "patch"_s))
        return RequestMethod::Patch;
    if (equalIgnoringASCIICase(name, "connect"_s))
        return RequestMethod::Connect;
    return std::nullopt;
}

bool ResourceLoadInfo::isThirdParty() const
{
    return !RegistrableDomain(mainDocumentURL).matches(resourceURL);
}

ResourceFlags ResourceLoadInfo::getResourceFlags() const
{
    ResourceFlags flags = 0;
    ASSERT(!type.isEmpty());
    flags |= type.toRaw();
    flags |= isThirdParty() ? static_cast<ResourceFlags>(LoadType::ThirdParty) : static_cast<ResourceFlags>(LoadType::FirstParty);
    flags |= mainFrameContext ? static_cast<ResourceFlags>(LoadContext::TopFrame) : static_cast<ResourceFlags>(LoadContext::ChildFrame);
    flags |= static_cast<ResourceFlags>(requestMethod);
    return flags;
}

ASCIILiteral resourceTypeToString(OptionSet<ResourceType> resourceTypes)
{
    switch (*resourceTypes.begin()) {
    case ResourceType::TopDocument:
        return "top-document"_s;
    case ResourceType::ChildDocument:
        return "child-document"_s;
    case ResourceType::Image:
        return "image"_s;
    case ResourceType::StyleSheet:
        return "style-sheet"_s;
    case ResourceType::Script:
        return "script"_s;
    case ResourceType::Font:
        return "font"_s;
    case ResourceType::WebSocket:
        return "websocket"_s;
    case ResourceType::Fetch:
        return "fetch"_s;
    case ResourceType::SVGDocument:
        return "svg-document"_s;
    case ResourceType::Media:
        return "media"_s;
    case ResourceType::Popup:
        return "popup"_s;
    case ResourceType::Ping:
        return "ping"_s;
    case ResourceType::CSPReport:
        return "csp-report"_s;
    case ResourceType::Other:
        return "other"_s;
    default:
        return "raw"_s;
    }
}

ASCIILiteral resourceTypeToStringForMatchedRule(OptionSet<ResourceType> resourceTypes)
{
    switch (*resourceTypes.begin()) {
    case ResourceType::TopDocument:
        return "main_frame"_s;
    case ResourceType::ChildDocument:
        return "sub_frame"_s;
    case ResourceType::Image:
        return "image"_s;
    case ResourceType::StyleSheet:
        return "stylesheet"_s;
    case ResourceType::Script:
        return "script"_s;
    case ResourceType::Font:
        return "font"_s;
    case ResourceType::WebSocket:
        return "websocket"_s;
    case ResourceType::Fetch:
        return "xmlhttprequest"_s;
    case ResourceType::Media:
        return "media"_s;
    case ResourceType::Ping:
        return "ping"_s;
    case ResourceType::CSPReport:
        return "csp_report"_s;
    case ResourceType::SVGDocument:
    case ResourceType::Popup:
    case ResourceType::Other:
    default:
        return "other"_s;
    }
}

} // namespace WebCore::ContentExtensions

#endif // ENABLE(CONTENT_EXTENSIONS)
