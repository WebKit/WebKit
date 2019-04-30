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

#include "Document.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
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
    ASSERT(m_pages.isEmpty());
}

void UserContentProvider::addPage(Page& page)
{
    ASSERT(!m_pages.contains(&page));

    m_pages.add(&page);
}

void UserContentProvider::removePage(Page& page)
{
    ASSERT(m_pages.contains(&page));

    m_pages.remove(&page);
}

void UserContentProvider::registerForUserMessageHandlerInvalidation(UserContentProviderInvalidationClient& invalidationClient)
{
    ASSERT(!m_userMessageHandlerInvalidationClients.contains(&invalidationClient));

    m_userMessageHandlerInvalidationClients.add(&invalidationClient);
}

void UserContentProvider::unregisterForUserMessageHandlerInvalidation(UserContentProviderInvalidationClient& invalidationClient)
{
    ASSERT(m_userMessageHandlerInvalidationClients.contains(&invalidationClient));

    m_userMessageHandlerInvalidationClients.remove(&invalidationClient);
}

void UserContentProvider::invalidateAllRegisteredUserMessageHandlerInvalidationClients()
{
    for (auto& client : m_userMessageHandlerInvalidationClients)
        client->didInvalidate(*this);
}

void UserContentProvider::invalidateInjectedStyleSheetCacheInAllFramesInAllPages()
{
    for (auto& page : m_pages)
        page->invalidateInjectedStyleSheetCacheInAllFrames();
}

#if ENABLE(CONTENT_EXTENSIONS)
static bool contentExtensionsEnabled(const DocumentLoader& documentLoader)
{
    if (auto frame = documentLoader.frame()) {
        if (frame->isMainFrame())
            return documentLoader.userContentExtensionsEnabled();
        if (auto mainDocumentLoader = frame->mainFrame().loader().documentLoader())
            return mainDocumentLoader->userContentExtensionsEnabled();
    }

    return true;
}
    
ContentRuleListResults UserContentProvider::processContentRuleListsForLoad(const URL& url, OptionSet<ContentExtensions::ResourceType> resourceType, DocumentLoader& initiatingDocumentLoader)
{
    if (!contentExtensionsEnabled(initiatingDocumentLoader))
        return { };

    return userContentExtensionBackend().processContentRuleListsForLoad(url, resourceType, initiatingDocumentLoader);
}

Vector<ContentExtensions::ActionsFromContentRuleList> UserContentProvider::actionsForResourceLoad(const ContentExtensions::ResourceLoadInfo& resourceLoadInfo, DocumentLoader& initiatingDocumentLoader)
{
    if (!contentExtensionsEnabled(initiatingDocumentLoader))
        return { };

    return userContentExtensionBackend().actionsForResourceLoad(resourceLoadInfo);
}

void UserContentProvider::forEachContentExtension(const WTF::Function<void(const String&, ContentExtensions::ContentExtension&)>& apply, DocumentLoader& initiatingDocumentLoader)
{
    if (!contentExtensionsEnabled(initiatingDocumentLoader))
        return;

    userContentExtensionBackend().forEach(apply);
}

#endif

} // namespace WebCore
