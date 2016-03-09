/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef UserContentController_h
#define UserContentController_h

#include "UserScriptTypes.h"
#include "UserStyleSheetTypes.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include "UserMessageHandlerDescriptorTypes.h"
#endif

#if ENABLE(CONTENT_EXTENSIONS)
#include "ContentExtensionActions.h"
#endif

namespace WebCore {

class DOMWrapperWorld;
class DocumentLoader;
class Page;
class ResourceRequest;
class StyleSheetContents;
class URL;
class UserScript;
class UserStyleSheet;
class UserMessageHandlerDescriptor;

enum class ResourceType : uint16_t;

struct ResourceLoadInfo;

namespace ContentExtensions {
class CompiledContentExtension;
class ContentExtensionsBackend;
struct Action;
}

class UserContentController : public RefCounted<UserContentController> {
public:
    WEBCORE_EXPORT static Ref<UserContentController> create();
    WEBCORE_EXPORT ~UserContentController();

    void addPage(Page&);
    void removePage(Page&);

    const UserScriptMap* userScripts() const { return m_userScripts.get(); }

    WEBCORE_EXPORT void addUserScript(DOMWrapperWorld&, std::unique_ptr<UserScript>);
    WEBCORE_EXPORT void removeUserScript(DOMWrapperWorld&, const URL&);
    WEBCORE_EXPORT void removeUserScripts(DOMWrapperWorld&);

    const UserStyleSheetMap* userStyleSheets() const { return m_userStyleSheets.get(); }

    WEBCORE_EXPORT void addUserStyleSheet(DOMWrapperWorld&, std::unique_ptr<UserStyleSheet>, UserStyleInjectionTime);
    WEBCORE_EXPORT void removeUserStyleSheet(DOMWrapperWorld&, const URL&);
    WEBCORE_EXPORT void removeUserStyleSheets(DOMWrapperWorld&);

    WEBCORE_EXPORT void removeAllUserContent();

#if ENABLE(USER_MESSAGE_HANDLERS)
    const UserMessageHandlerDescriptorMap* userMessageHandlerDescriptors() const { return m_userMessageHandlerDescriptors.get(); }

    WEBCORE_EXPORT void addUserMessageHandlerDescriptor(UserMessageHandlerDescriptor&);
    WEBCORE_EXPORT void removeUserMessageHandlerDescriptor(UserMessageHandlerDescriptor&);
#endif

#if ENABLE(CONTENT_EXTENSIONS)
    WEBCORE_EXPORT void addUserContentExtension(const String& name, RefPtr<ContentExtensions::CompiledContentExtension>);
    WEBCORE_EXPORT void removeUserContentExtension(const String& name);
    WEBCORE_EXPORT void removeAllUserContentExtensions();

    ContentExtensions::BlockedStatus processContentExtensionRulesForLoad(ResourceRequest&, ResourceType, DocumentLoader& initiatingDocumentLoader);
    Vector<ContentExtensions::Action> actionsForResourceLoad(const ResourceLoadInfo&, DocumentLoader& initiatingDocumentLoader);
#endif

private:
    UserContentController();

    void invalidateInjectedStyleSheetCacheInAllFramesInAllPages();

    HashSet<Page*> m_pages;

    std::unique_ptr<UserScriptMap> m_userScripts;
    std::unique_ptr<UserStyleSheetMap> m_userStyleSheets;
#if ENABLE(USER_MESSAGE_HANDLERS)
    std::unique_ptr<UserMessageHandlerDescriptorMap> m_userMessageHandlerDescriptors;
#endif
#if ENABLE(CONTENT_EXTENSIONS)
    std::unique_ptr<ContentExtensions::ContentExtensionsBackend> m_contentExtensionBackend;
#endif
};

} // namespace WebCore

#endif // UserContentController_h
