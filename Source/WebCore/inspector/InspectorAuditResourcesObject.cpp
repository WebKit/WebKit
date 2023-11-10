/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "InspectorAuditResourcesObject.h"

#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedRawResource.h"
#include "CachedResource.h"
#include "CachedSVGDocument.h"
#include "Document.h"
#include "FrameDestructionObserverInlines.h"
#include "InspectorPageAgent.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace Inspector;

#define ERROR_IF_NO_ACTIVE_AUDIT() \
    if (!m_auditAgent.hasActiveAudit()) \
        return Exception { ExceptionCode::NotAllowedError, "Cannot be called outside of a Web Inspector Audit"_s };

InspectorAuditResourcesObject::InspectorAuditResourcesObject(InspectorAuditAgent& auditAgent)
    : m_auditAgent(auditAgent)
{
}

InspectorAuditResourcesObject::~InspectorAuditResourcesObject()
{
    for (auto* cachedResource : m_resources.values())
        cachedResource->removeClient(clientForResource(*cachedResource));
}

ExceptionOr<Vector<InspectorAuditResourcesObject::Resource>> InspectorAuditResourcesObject::getResources(Document& document)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    Vector<Resource> resources;

    auto* frame = document.frame();
    if (!frame)
        return Exception { ExceptionCode::NotAllowedError, "Cannot be called with a detached document"_s };

    for (auto* cachedResource : InspectorPageAgent::cachedResourcesForFrame(frame)) {
        Resource resource;
        resource.url = cachedResource->url().string();
        resource.mimeType = cachedResource->mimeType();

        bool exists = false;
        for (const auto& entry : m_resources) {
            if (entry.value == cachedResource) {
                resource.id = entry.key;
                exists = true;
                break;
            }
        }
        if (!exists) {
            cachedResource->addClient(clientForResource(*cachedResource));

            resource.id = String::number(m_resources.size() + 1);
            m_resources.add(resource.id, cachedResource);
        }

        resources.append(WTFMove(resource));
    }

    return resources;
}

ExceptionOr<InspectorAuditResourcesObject::ResourceContent> InspectorAuditResourcesObject::getResourceContent(Document& document, const String& id)
{
    ERROR_IF_NO_ACTIVE_AUDIT();

    auto* frame = document.frame();
    if (!frame)
        return Exception { ExceptionCode::NotAllowedError, "Cannot be called with a detached document"_s };

    auto* cachedResource = m_resources.get(id);
    if (!cachedResource)
        return Exception { ExceptionCode::NotFoundError, makeString("Unknown identifier "_s, id) };

    Protocol::ErrorString errorString;
    ResourceContent resourceContent;
    InspectorPageAgent::resourceContent(errorString, frame, cachedResource->url(), &resourceContent.data, &resourceContent.base64Encoded);
    if (!errorString.isEmpty())
        return Exception { ExceptionCode::NotFoundError, errorString };

    return resourceContent;
}

CachedResourceClient& InspectorAuditResourcesObject::clientForResource(const CachedResource& cachedResource)
{
    if (is<CachedCSSStyleSheet>(cachedResource))
        return m_cachedStyleSheetClient;

    if (is<CachedFont>(cachedResource))
        return m_cachedFontClient;

    if (is<CachedImage>(cachedResource))
        return m_cachedImageClient;

    if (is<CachedRawResource>(cachedResource))
        return m_cachedRawResourceClient;

    if (is<CachedSVGDocument>(cachedResource))
        return m_cachedSVGDocumentClient;

    return m_cachedResourceClient;
}

} // namespace WebCore
