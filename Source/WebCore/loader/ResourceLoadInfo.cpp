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

#include "ContentExtensionActions.h"
#include "SecurityOrigin.h"

#if ENABLE(CONTENT_EXTENSIONS)

namespace WebCore {
namespace ContentExtensions {

ResourceType toResourceType(CachedResource::Type type)
{
    switch (type) {
    case CachedResource::Type::LinkPrefetch:
    case CachedResource::Type::MainResource:
        return ResourceType::Document;
    case CachedResource::Type::SVGDocumentResource:
        return ResourceType::SVGDocument;
    case CachedResource::Type::ImageResource:
        return ResourceType::Image;
    case CachedResource::Type::CSSStyleSheet:
#if ENABLE(XSLT)
    case CachedResource::Type::XSLStyleSheet:
#endif
        return ResourceType::StyleSheet;

    case CachedResource::Type::Script:
        return ResourceType::Script;

    case CachedResource::Type::FontResource:
    case CachedResource::Type::SVGFontResource:
        return ResourceType::Font;

    case CachedResource::Type::MediaResource:
        return ResourceType::Media;

    case CachedResource::Type::Beacon:
    case CachedResource::Type::Ping:
    case CachedResource::Type::Icon:
    case CachedResource::Type::RawResource:
        return ResourceType::Raw;

    case CachedResource::Type::TextTrackResource:
        return ResourceType::Media;

#if ENABLE(APPLICATION_MANIFEST)
    case CachedResource::Type::ApplicationManifest:
        return ResourceType::Raw;
#endif
    };
    return ResourceType::Raw;
}

uint16_t readResourceType(const String& name)
{
    if (name == "document")
        return static_cast<uint16_t>(ResourceType::Document);
    if (name == "image")
        return static_cast<uint16_t>(ResourceType::Image);
    if (name == "style-sheet")
        return static_cast<uint16_t>(ResourceType::StyleSheet);
    if (name == "script")
        return static_cast<uint16_t>(ResourceType::Script);
    if (name == "font")
        return static_cast<uint16_t>(ResourceType::Font);
    if (name == "raw")
        return static_cast<uint16_t>(ResourceType::Raw);
    if (name == "svg-document")
        return static_cast<uint16_t>(ResourceType::SVGDocument);
    if (name == "media")
        return static_cast<uint16_t>(ResourceType::Media);
    if (name == "popup")
        return static_cast<uint16_t>(ResourceType::Popup);
    if (name == "ping")
        return static_cast<uint16_t>(ResourceType::Ping);
    return static_cast<uint16_t>(ResourceType::Invalid);
}

uint16_t readLoadType(const String& name)
{
    if (name == "first-party")
        return static_cast<uint16_t>(LoadType::FirstParty);
    if (name == "third-party")
        return static_cast<uint16_t>(LoadType::ThirdParty);
    return static_cast<uint16_t>(LoadType::Invalid);
}

bool ResourceLoadInfo::isThirdParty() const
{
    Ref<SecurityOrigin> mainDocumentSecurityOrigin = SecurityOrigin::create(mainDocumentURL);
    Ref<SecurityOrigin> resourceSecurityOrigin = SecurityOrigin::create(resourceURL);

    return !mainDocumentSecurityOrigin->canAccess(resourceSecurityOrigin.get());
}
    
ResourceFlags ResourceLoadInfo::getResourceFlags() const
{
    ResourceFlags flags = 0;
    ASSERT(type != ResourceType::Invalid);
    flags |= type.toRaw();
    flags |= isThirdParty() ? static_cast<ResourceFlags>(LoadType::ThirdParty) : static_cast<ResourceFlags>(LoadType::FirstParty);
    return flags;
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
