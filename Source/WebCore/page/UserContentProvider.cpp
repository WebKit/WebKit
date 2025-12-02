/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "UserContentProvider.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "Page.h"

#if ENABLE(CONTENT_EXTENSIONS)
#include "ContentExtensionCompiler.h"
#include "ContentExtensionsBackend.h"
#include "ContentRuleListResults.h"
#endif

namespace WebCore {

UserContentProvider::UserContentProvider()
{
}

UserContentProvider::~UserContentProvider()
{
    ASSERT(m_pages.isEmptyIgnoringNullReferences());
}

void UserContentProvider::addPage(Page& page)
{
    ASSERT(!m_pages.contains(page));

    m_pages.add(page);
}

void UserContentProvider::removePage(Page& page)
{
    ASSERT(m_pages.contains(page));

    m_pages.remove(page);
}

void UserContentProvider::registerForUserMessageHandlerInvalidation(UserContentProviderInvalidationClient& invalidationClient)
{
    ASSERT(!m_userMessageHandlerInvalidationClients.contains(invalidationClient));

    m_userMessageHandlerInvalidationClients.add(invalidationClient);
}

void UserContentProvider::unregisterForUserMessageHandlerInvalidation(UserContentProviderInvalidationClient& invalidationClient)
{
    ASSERT(m_userMessageHandlerInvalidationClients.contains(invalidationClient));

    m_userMessageHandlerInvalidationClients.remove(invalidationClient);
}

void UserContentProvider::invalidateAllRegisteredUserMessageHandlerInvalidationClients()
{
    m_userMessageHandlerInvalidationClients.forEach([&](auto& client) {
        client.didInvalidate(*this);
    });
}

void UserContentProvider::invalidateInjectedStyleSheetCacheInAllFramesInAllPages()
{
    for (auto& page : m_pages)
        page.invalidateInjectedStyleSheetCacheInAllFrames();
}

#if ENABLE(CONTENT_EXTENSIONS)
static DocumentLoader* mainDocumentLoader(DocumentLoader& loader)
{
    if (auto frame = loader.frame()) {
        if (frame->isMainFrame())
            return &loader;

        auto* localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
        if (localFrame)
            return localFrame->loader().documentLoader();
    }
    return nullptr;
}

static ContentExtensions::ContentExtensionsBackend::RuleListFilter ruleListFilter(DocumentLoader& documentLoader)
{
    RefPtr mainLoader = mainDocumentLoader(documentLoader);
    if (!mainLoader) {
        return [](const String&) {
            return ContentExtensions::ContentExtensionsBackend::ShouldSkipRuleList::No;
        };
    }

    RefPtr policySourceLoader = mainLoader;
    if (!mainLoader->request().url().hasSpecialScheme() && documentLoader.request().url().protocolIsInHTTPFamily())
        policySourceLoader = documentLoader;

    auto& exceptions = policySourceLoader->contentExtensionEnablement().second;
    switch (policySourceLoader->contentExtensionEnablement().first) {
    case ContentExtensionDefaultEnablement::Disabled:
        return [&](auto& identifier) {
            return exceptions.contains(identifier)
                ? ContentExtensions::ContentExtensionsBackend::ShouldSkipRuleList::No
                : ContentExtensions::ContentExtensionsBackend::ShouldSkipRuleList::Yes;
        };
    case ContentExtensionDefaultEnablement::Enabled:
        return [&](auto& identifier) {
            return exceptions.contains(identifier)
                ? ContentExtensions::ContentExtensionsBackend::ShouldSkipRuleList::Yes
                : ContentExtensions::ContentExtensionsBackend::ShouldSkipRuleList::No;
        };
    }
    ASSERT_NOT_REACHED();
    return { };
}

static void applyLinkDecorationFilteringIfNeeded(ContentRuleListResults& results, Page& page, const URL& url, const DocumentLoader& initiatingDocumentLoader)
{
    if (RefPtr frame = initiatingDocumentLoader.frame(); !frame || !frame->isMainFrame())
        return;

    if (auto adjustedURL = page.chrome().client().applyLinkDecorationFiltering(url, LinkDecorationFilteringTrigger::Navigation); adjustedURL != url)
        results.summary.redirectActions.append({ { ContentExtensions::RedirectAction::URLAction { adjustedURL.string() } }, adjustedURL });
}

ContentRuleListResults UserContentProvider::processContentRuleListsForLoad(Page& page, const URL& url, OptionSet<ContentExtensions::ResourceType> resourceType, DocumentLoader& initiatingDocumentLoader, const URL& redirectFrom) const
{
    auto results = userContentExtensionBackend().processContentRuleListsForLoad(page, url, resourceType, initiatingDocumentLoader, redirectFrom, ruleListFilter(initiatingDocumentLoader));

    if (resourceType.containsAny({ ContentExtensions::ResourceType::TopDocument, ContentExtensions::ResourceType::ChildDocument }))
        applyLinkDecorationFilteringIfNeeded(results, page, url, initiatingDocumentLoader);

    return results;
}
#endif // ENABLE(CONTENT_EXTENSIONS)

} // namespace WebCore
