/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "EnvironmentMapLoader.h"

#if ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)

#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "CrossOriginAccessControl.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentResourceLoader.h"
#include "Element.h"
#include "ElementInlines.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTTPStatusCodes.h"
#include "LegacySchemeRegistry.h"
#include "NodeDocument.h"
#include "SecurityOrigin.h"
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(SPATIAL_PORTAL)
#include "CSSEnvironmentMapRule.h"
#include "StyleDocumentScope.h"
#include "StyleEnvironmentMap.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include "StyleScopeRuleSets.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(EnvironmentMapLoader);

#if ENABLE(SPATIAL_PORTAL)

std::optional<URL> resolvedEnvironmentMapURL(const Element& element, const Style::EnvironmentMap& environmentMap)
{
    auto* name = environmentMap.tryName();
    if (!name || name->value.isEmpty())
        return std::nullopt;

    AtomString ruleName { name->value };

    auto ruleForName = [&](const Style::Scope& scope) -> RefPtr<const StyleRuleEnvironmentMap> {
        RefPtr resolver = scope.resolverIfExists();
        if (!resolver)
            return nullptr;
        return resolver->ruleSets().authorStyle().environmentMapRuleForName(ruleName);
    };

    const Style::Scope& elementScope = Style::Scope::forNode(element);
    RefPtr rule = ruleForName(elementScope);

    if (!rule) {
        const Style::Scope& documentScope = element.document().styleScope();
        if (&documentScope != &elementScope)
            rule = ruleForName(documentScope);
    }

    if (!rule)
        return std::nullopt;

    ASSERT(rule->isUsable());
    return rule->src().resolved;
}

#endif // ENABLE(SPATIAL_PORTAL)

static CachedResourceRequest createEnvironmentMapResourceRequest(Element& element, const URL& resourceURL)
{
    ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
    options.destination = FetchOptions::Destination::Environmentmap;
    options.sameOriginDataURLFlag = SameOriginDataURLFlag::Set;

    auto crossOriginAttribute = parseCORSSettingsAttribute(element.attributeWithoutSynchronization(HTMLNames::crossoriginAttr));
    Ref document = element.document();
    if (crossOriginAttribute.isNull()) {
        Ref documentOrigin = document->securityOrigin();
        if (LegacySchemeRegistry::shouldTreatURLSchemeAsCORSEnabled(documentOrigin->protocol()) || documentOrigin->protocol() != resourceURL.protocol())
            crossOriginAttribute = "anonymous"_s;
    }
    auto request = createPotentialAccessControlRequest(ResourceRequest { URL { resourceURL } }, WTF::move(options), document, crossOriginAttribute);
    request.setInitiator(element);

    return request;
}

EnvironmentMapLoader::~EnvironmentMapLoader()
{
    clearResource();
}

void EnvironmentMapLoader::load(Element& element, const URL& url, LoadCompletionHandler&& completionHandler)
{
    cancel();

    m_completionHandler = WTF::move(completionHandler);

    auto resource = protect(element.document().cachedResourceLoader())->requestEnvironmentMapResource(createEnvironmentMapResourceRequest(element, url));
    if (!resource.has_value()) {
        complete(nullptr);
        return;
    }

    m_data.empty();
    m_resource = resource.value();
    m_resource->addClient(*this);
}

void EnvironmentMapLoader::cancel()
{
    clearResource();
    m_data.reset();
    m_completionHandler = { };
}

void EnvironmentMapLoader::clearResource()
{
    if (!m_resource)
        return;

    m_resource->removeClient(*this);
    m_resource = nullptr;
}

void EnvironmentMapLoader::complete(RefPtr<SharedBuffer>&& data)
{
    Ref protectedThis { *this };
    clearResource();

    if (auto completionHandler = std::exchange(m_completionHandler, { }))
        completionHandler(WTF::move(data));
}

void EnvironmentMapLoader::dataReceived(CachedResource&, const SharedBuffer& buffer)
{
    m_data.append(buffer);
}

void EnvironmentMapLoader::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    int status = resource.response().httpStatusCode();
    if (resource.loadFailedOrCanceled() || (status && !isHttpOkStatus(status))) {
        m_data.reset();
        complete(nullptr);
        return;
    }

    complete(m_data.takeBufferAsContiguous());
}

} // namespace WebCore

#endif // ENABLE(MODEL_ELEMENT_ENVIRONMENT_MAP)
