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

#pragma once

#include <functional>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "ContentExtensionActions.h"
#include "ContentExtensionsBackend.h"
#endif

namespace WebCore {

class DOMWrapperWorld;
class DocumentLoader;
class Page;
class ResourceRequest;
class UserMessageHandlerDescriptor;
class UserScript;
class UserStyleSheet;

enum class ResourceType : uint16_t;

struct ResourceLoadInfo;

namespace ContentExtensions {
class ContentExtensionsBackend;
struct Action;
}

class UserContentProvider;

class UserContentProviderInvalidationClient {
public:
    virtual ~UserContentProviderInvalidationClient()
    {
    }
    
    virtual void didInvalidate(UserContentProvider&) = 0;
};

class UserContentProvider : public RefCounted<UserContentProvider> {
public:
    WEBCORE_EXPORT UserContentProvider();
    WEBCORE_EXPORT virtual ~UserContentProvider();

    virtual void forEachUserScript(Function<void(DOMWrapperWorld&, const UserScript&)>&&) const = 0;
    virtual void forEachUserStyleSheet(Function<void(const UserStyleSheet&)>&&) const = 0;
#if ENABLE(USER_MESSAGE_HANDLERS)
    virtual void forEachUserMessageHandler(Function<void(const UserMessageHandlerDescriptor&)>&&) const = 0;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    virtual ContentExtensions::ContentExtensionsBackend& userContentExtensionBackend() = 0;
#endif

    void registerForUserMessageHandlerInvalidation(UserContentProviderInvalidationClient&);
    void unregisterForUserMessageHandlerInvalidation(UserContentProviderInvalidationClient&);

    void addPage(Page&);
    void removePage(Page&);

#if ENABLE(CONTENT_EXTENSIONS)
    // FIXME: These don't really belong here. They should probably bundled up in the ContentExtensionsBackend
    // which should always exist.
    ContentExtensions::BlockedStatus processContentExtensionRulesForLoad(const URL&, ResourceType, DocumentLoader& initiatingDocumentLoader);
    std::pair<Vector<ContentExtensions::Action>, Vector<String>> actionsForResourceLoad(const ResourceLoadInfo&, DocumentLoader& initiatingDocumentLoader);
    WEBCORE_EXPORT void forEachContentExtension(const WTF::Function<void(const String&, ContentExtensions::ContentExtension&)>&, DocumentLoader& initiatingDocumentLoader);
#endif

protected:
    WEBCORE_EXPORT void invalidateAllRegisteredUserMessageHandlerInvalidationClients();
    WEBCORE_EXPORT void invalidateInjectedStyleSheetCacheInAllFramesInAllPages();

private:
    HashSet<Page*> m_pages;
    HashSet<UserContentProviderInvalidationClient*> m_userMessageHandlerInvalidationClients;
};

} // namespace WebCore
