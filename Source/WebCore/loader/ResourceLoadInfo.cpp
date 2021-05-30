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

namespace WebCore {
namespace ContentExtensions {

OptionSet<ResourceType> toResourceType(CachedResource::Type type, ResourceRequestBase::Requester requester)
{
    switch (type) {
    case CachedResource::Type::LinkPrefetch:
    case CachedResource::Type::MainResource:
        return { ResourceType::Document };
    case CachedResource::Type::SVGDocumentResource:
        return { ResourceType::SVGDocument };
    case CachedResource::Type::ImageResource:
        return { ResourceType::Image };
    case CachedResource::Type::CSSStyleSheet:
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
        return { ResourceType::StyleSheet };

    case CachedResource::Type::Script:
        return { ResourceType::Script };

    case CachedResource::Type::FontResource:
    case CachedResource::Type::SVGFontResource:
        return { ResourceType::Font };

    case CachedResource::Type::MediaResource:
        return { ResourceType::Media };

    case CachedResource::Type::RawResource:
        if (requester == ResourceRequestBase::Requester::XHR
            || requester == ResourceRequestBase::Requester::Fetch)
            return { ResourceType::Fetch };
        FALLTHROUGH;
    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::Icon:
#if ENABLE(MODEL_ELEMENT)
    case CachedResource::Type::ModelResource:
#endif
#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
#endif
        return { ResourceType::Other };

    case CachedResource::Type::TextTrackResource:
        return { ResourceType::Media };

    };
    ASSERT_NOT_REACHED();
    return { };
}

std::optional<OptionSet<ResourceType>> readResourceType(StringView name)
{
    if (name == "document")
        return { ResourceType::Document };
    if (name == "image")
        return { ResourceType::Image };
    if (name == "style-sheet")
        return { ResourceType::StyleSheet };
    if (name == "script")
        return { ResourceType::Script };
    if (name == "font")
        return { ResourceType::Font };
    if (name == "raw")
        return {{ ResourceType::Fetch, ResourceType::WebSocket, ResourceType::Other, ResourceType::Ping }};
    if (name == "websocket")
        return { ResourceType::WebSocket };
    if (name == "fetch")
        return { ResourceType::Fetch };
    if (name == "other")
        return {{ ResourceType::Other, ResourceType::Ping }};
    if (name == "svg-document")
        return { ResourceType::SVGDocument };
    if (name == "media")
        return { ResourceType::Media };
    if (name == "popup")
        return { ResourceType::Popup };
    if (name == "ping")
        return { ResourceType::Ping };
    return std::nullopt;
}

std::optional<OptionSet<LoadType>> readLoadType(StringView name)
{
    if (name == "first-party")
        return { LoadType::FirstParty };
    if (name == "third-party")
        return { LoadType::ThirdParty };
    return std::nullopt;
}

std::optional<OptionSet<LoadContext>> readLoadContext(StringView name)
{
    if (name == "top-frame")
        return { LoadContext::TopFrame };
    if (name == "child-frame")
        return { LoadContext::ChildFrame };
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
    return flags;
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
