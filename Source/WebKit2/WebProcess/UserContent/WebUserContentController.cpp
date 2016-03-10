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

#include "config.h"
#include "WebUserContentController.h"

#include "DataReference.h"
#include "InjectedBundleScriptWorld.h"
#include "WebCompiledContentExtension.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebUserContentControllerMessages.h"
#include "WebUserContentControllerProxyMessages.h"
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/UserStyleSheet.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include <WebCore/UserMessageHandler.h>
#include <WebCore/UserMessageHandlerDescriptor.h>
#endif

using namespace WebCore;

namespace WebKit {

static HashMap<uint64_t, WebUserContentController*>& userContentControllers()
{
    static NeverDestroyed<HashMap<uint64_t, WebUserContentController*>> userContentControllers;

    return userContentControllers;
}

typedef HashMap<uint64_t, std::pair<RefPtr<InjectedBundleScriptWorld>, unsigned>> WorldMap;

static WorldMap& worldMap()
{
    static NeverDestroyed<WorldMap> map(std::initializer_list<WorldMap::KeyValuePairType> { { 1, std::make_pair(InjectedBundleScriptWorld::normalWorld(), 1) } });

    return map;
}

Ref<WebUserContentController> WebUserContentController::getOrCreate(uint64_t identifier)
{
    auto& userContentControllerPtr = userContentControllers().add(identifier, nullptr).iterator->value;
    if (userContentControllerPtr)
        return *userContentControllerPtr;

    RefPtr<WebUserContentController> userContentController = adoptRef(new WebUserContentController(identifier));
    userContentControllerPtr = userContentController.get();

    return userContentController.releaseNonNull();
}

WebUserContentController::WebUserContentController(uint64_t identifier)
    : m_identifier(identifier)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebUserContentController::messageReceiverName(), m_identifier, *this);
}

WebUserContentController::~WebUserContentController()
{
    ASSERT(userContentControllers().contains(m_identifier));

    WebProcess::singleton().removeMessageReceiver(Messages::WebUserContentController::messageReceiverName(), m_identifier);

    userContentControllers().remove(m_identifier);
}

void WebUserContentController::addUserContentWorlds(const Vector<std::pair<uint64_t, String>>& worlds)
{
    for (auto& world : worlds) {
        ASSERT(world.first);
        ASSERT(world.first != 1);

        worldMap().ensure(world.first, [&] { return std::make_pair(InjectedBundleScriptWorld::create(world.second), 1); });
    }
}

void WebUserContentController::removeUserContentWorlds(const Vector<uint64_t>& worldIdentifiers)
{
    for (auto& worldIdentifier : worldIdentifiers) {
        ASSERT(worldIdentifier);
        ASSERT(worldIdentifier != 1);

        auto it = worldMap().find(worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to remove a UserContentWorld (id=%" PRIu64 ") that is does not exist.", worldIdentifier);
            return;
        }

        it->value.second--;
        
        if (!it->value.second)
            worldMap().remove(it);
    }
}

void WebUserContentController::addUserScripts(const Vector<WebUserScriptData>& userScripts)
{
    for (const auto& userScriptData : userScripts) {
        auto it = worldMap().find(userScriptData.worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to add a UserScript to a UserContentWorld (id=%" PRIu64 ") that does not exist.", userScriptData.worldIdentifier);
            continue;
        }

        UserScript script = userScriptData.userScript;
        addUserScriptInternal(*it->value.first, userScriptData.identifier, WTFMove(script));
    }
}

void WebUserContentController::removeUserScript(uint64_t worldIdentifier, uint64_t userScriptIdentifier)
{
    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove a UserScript from a UserContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier);
        return;
    }

    removeUserScriptInternal(*it->value.first, userScriptIdentifier);
}

void WebUserContentController::removeAllUserScripts(const Vector<uint64_t>& worldIdentifiers)
{
    for (auto& worldIdentifier : worldIdentifiers) {
        auto it = worldMap().find(worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to remove all UserScripts from a UserContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier);
            return;
        }

        removeUserScripts(*it->value.first);
    }
}

void WebUserContentController::addUserStyleSheets(const Vector<WebUserStyleSheetData>& userStyleSheets)
{
    for (const auto& userStyleSheetData : userStyleSheets) {
        auto it = worldMap().find(userStyleSheetData.worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to add a UserStyleSheet to a UserContentWorld (id=%" PRIu64 ") that does not exist.", userStyleSheetData.worldIdentifier);
            continue;
        }
        
        UserStyleSheet sheet = userStyleSheetData.userStyleSheet;
        addUserStyleSheetInternal(*it->value.first, userStyleSheetData.identifier, WTFMove(sheet));
    }
}

void WebUserContentController::removeUserStyleSheet(uint64_t worldIdentifier, uint64_t userStyleSheetIdentifier)
{
    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove a UserStyleSheet from a UserContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier);
        return;
    }

    removeUserStyleSheetInternal(*it->value.first, userStyleSheetIdentifier);
}

void WebUserContentController::removeAllUserStyleSheets(const Vector<uint64_t>& worldIdentifiers)
{
    for (auto& worldIdentifier : worldIdentifiers) {
        auto it = worldMap().find(worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to remove all UserStyleSheets from a UserContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier);
            return;
        }

        removeUserStyleSheets(*it->value.first);
    }
}

#if ENABLE(USER_MESSAGE_HANDLERS)
class WebUserMessageHandlerDescriptorProxy : public RefCounted<WebUserMessageHandlerDescriptorProxy>, public WebCore::UserMessageHandlerDescriptor::Client {
public:
    static PassRefPtr<WebUserMessageHandlerDescriptorProxy> create(WebUserContentController* controller, const String& name, uint64_t identifier)
    {
        return adoptRef(new WebUserMessageHandlerDescriptorProxy(controller, name, identifier));
    }

    virtual ~WebUserMessageHandlerDescriptorProxy()
    {
        m_descriptor->invalidateClient();
    }

    // WebCore::UserMessageHandlerDescriptor::Client
    virtual void didPostMessage(WebCore::UserMessageHandler& handler, WebCore::SerializedScriptValue* value)
    {
        WebCore::Frame* frame = handler.frame();
        if (!frame)
            return;
    
        WebFrame* webFrame = WebFrame::fromCoreFrame(*frame);
        if (!webFrame)
            return;

        WebPage* webPage = webFrame->page();
        if (!webPage)
            return;

        WebProcess::singleton().parentProcessConnection()->send(Messages::WebUserContentControllerProxy::DidPostMessage(webPage->pageID(), webFrame->frameID(), SecurityOriginData::fromFrame(frame), m_identifier, IPC::DataReference(value->data())), m_controller->identifier());
    }

    WebCore::UserMessageHandlerDescriptor& descriptor() { return *m_descriptor; }
    uint64_t identifier() { return m_identifier; }

private:
    WebUserMessageHandlerDescriptorProxy(WebUserContentController* controller, const String& name, uint64_t identifier)
        : m_controller(controller)
        , m_descriptor(UserMessageHandlerDescriptor::create(name, mainThreadNormalWorld(), *this))
        , m_identifier(identifier)
    {
    }

    RefPtr<WebUserContentController> m_controller;
    RefPtr<WebCore::UserMessageHandlerDescriptor> m_descriptor;
    uint64_t m_identifier;
};
#endif

void WebUserContentController::addUserScriptMessageHandlers(const Vector<WebScriptMessageHandlerHandle>& scriptMessageHandlers)
{
#if ENABLE(USER_MESSAGE_HANDLERS)
    for (auto& handle : scriptMessageHandlers) {
        RefPtr<WebUserMessageHandlerDescriptorProxy> descriptor = WebUserMessageHandlerDescriptorProxy::create(this, handle.name, handle.identifier);

        m_userMessageHandlerDescriptors.add(descriptor->identifier(), descriptor);

        auto& coreDescriptor = descriptor->descriptor();
        m_userMessageHandlerDescriptorsMap.add(std::make_pair(coreDescriptor.name(), &coreDescriptor.world()), &coreDescriptor);
    }
#else
    UNUSED_PARAM(scriptMessageHandlers);
#endif
}

void WebUserContentController::removeUserScriptMessageHandler(uint64_t identifier)
{
#if ENABLE(USER_MESSAGE_HANDLERS)
    auto it = m_userMessageHandlerDescriptors.find(identifier);
    ASSERT(it != m_userMessageHandlerDescriptors.end());

    auto& coreDescriptor = it->value->descriptor();
    m_userMessageHandlerDescriptorsMap.remove(std::make_pair(coreDescriptor.name(), &coreDescriptor.world()));

    m_userMessageHandlerDescriptors.remove(it);
#else
    UNUSED_PARAM(identifier);
#endif
}

#if ENABLE(CONTENT_EXTENSIONS)
void WebUserContentController::addUserContentExtensions(const Vector<std::pair<String, WebCompiledContentExtensionData>>& userContentExtensions)
{
    for (const auto& userContentExtension : userContentExtensions) {
        WebCompiledContentExtensionData contentExtensionData = userContentExtension.second;
        RefPtr<WebCompiledContentExtension> compiledContentExtension = WebCompiledContentExtension::create(WTFMove(contentExtensionData));

        m_contentExtensionBackend.addContentExtension(userContentExtension.first, WTFMove(compiledContentExtension));
    }
}

void WebUserContentController::removeUserContentExtension(const String& name)
{
    m_contentExtensionBackend.removeContentExtension(name);
}

void WebUserContentController::removeAllUserContentExtensions()
{
    m_contentExtensionBackend.removeAllContentExtensions();
}
#endif



void WebUserContentController::addUserScriptInternal(InjectedBundleScriptWorld& world, uint64_t userScriptIdentifier, UserScript&& userScript)
{
    auto& scriptsInWorld = m_userScripts.ensure(&world, [] { return Vector<std::pair<uint64_t, WebCore::UserScript>>(); }).iterator->value;
    scriptsInWorld.append(std::make_pair(userScriptIdentifier, WTFMove(userScript)));
}

void WebUserContentController::addUserScript(InjectedBundleScriptWorld& world, UserScript&& userScript)
{
    addUserScriptInternal(world, 0, WTFMove(userScript));
}

void WebUserContentController::removeUserScriptWithURL(InjectedBundleScriptWorld& world, const URL& url)
{
    auto it = m_userScripts.find(&world);
    if (it == m_userScripts.end())
        return;

    auto& scripts = it->value;
    for (int i = scripts.size() - 1; i >= 0; --i) {
        if (scripts[i].second.url() == url)
            scripts.remove(i);
    }

    if (scripts.isEmpty())
        m_userScripts.remove(it);
}

void WebUserContentController::removeUserScriptInternal(InjectedBundleScriptWorld& world, uint64_t userScriptIdentifier)
{
    auto it = m_userScripts.find(&world);
    if (it == m_userScripts.end())
        return;

    auto& scripts = it->value;
    for (int i = scripts.size() - 1; i >= 0; --i) {
        if (scripts[i].first == userScriptIdentifier)
            scripts.remove(i);
    }

    if (scripts.isEmpty())
        m_userScripts.remove(it);
}

void WebUserContentController::removeUserScripts(InjectedBundleScriptWorld& world)
{
    m_userScripts.remove(&world);
}

void WebUserContentController::addUserStyleSheetInternal(InjectedBundleScriptWorld& world, uint64_t userStyleSheetIdentifier, UserStyleSheet&& userStyleSheet)
{
    auto& styleSheetsInWorld = m_userStyleSheets.ensure(&world, [] { return Vector<std::pair<uint64_t, WebCore::UserStyleSheet>>(); }).iterator->value;
    styleSheetsInWorld.append(std::make_pair(userStyleSheetIdentifier, WTFMove(userStyleSheet)));

    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void WebUserContentController::addUserStyleSheet(InjectedBundleScriptWorld& world, UserStyleSheet&& userStyleSheet)
{
    addUserStyleSheetInternal(world, 0, WTFMove(userStyleSheet));
}

void WebUserContentController::removeUserStyleSheetWithURL(InjectedBundleScriptWorld& world, const URL& url)
{
    auto it = m_userStyleSheets.find(&world);
    if (it == m_userStyleSheets.end())
        return;

    auto& stylesheets = it->value;

    bool sheetsChanged = false;
    for (int i = stylesheets.size() - 1; i >= 0; --i) {
        if (stylesheets[i].second.url() == url) {
            stylesheets.remove(i);
            sheetsChanged = true;
        }
    }

    if (!sheetsChanged)
        return;

    if (stylesheets.isEmpty())
        m_userStyleSheets.remove(it);

    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void WebUserContentController::removeUserStyleSheetInternal(InjectedBundleScriptWorld& world, uint64_t userStyleSheetIdentifier)
{
    auto it = m_userStyleSheets.find(&world);
    if (it == m_userStyleSheets.end())
        return;

    auto& stylesheets = it->value;

    bool sheetsChanged = false;
    for (int i = stylesheets.size() - 1; i >= 0; --i) {
        if (stylesheets[i].first == userStyleSheetIdentifier) {
            stylesheets.remove(i);
            sheetsChanged = true;
        }
    }

    if (!sheetsChanged)
        return;

    if (stylesheets.isEmpty())
        m_userStyleSheets.remove(it);

    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void WebUserContentController::removeUserStyleSheets(InjectedBundleScriptWorld& world)
{
    if (!m_userStyleSheets.remove(&world))
        return;

    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void WebUserContentController::removeAllUserContent()
{
    m_userScripts.clear();

    if (!m_userStyleSheets.isEmpty()) {
        m_userStyleSheets.clear();
        invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
    }
}

void WebUserContentController::forEachUserScript(const std::function<void(WebCore::DOMWrapperWorld&, const WebCore::UserScript&)>& functor) const
{
    for (const auto& worldAndUserScriptVector : m_userScripts) {
        auto& world = worldAndUserScriptVector.key->coreWorld();
        for (const auto& identifierUserScriptPair : worldAndUserScriptVector.value)
            functor(world, identifierUserScriptPair.second);
    }
}

void WebUserContentController::forEachUserStyleSheet(const std::function<void(const WebCore::UserStyleSheet&)>& functor) const
{
    for (auto& styleSheetVector : m_userStyleSheets.values()) {
        for (const auto& identifierUserStyleSheetPair : styleSheetVector)
            functor(identifierUserStyleSheetPair.second);
    }
}

} // namespace WebKit
