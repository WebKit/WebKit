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

#include "ContentWorldShared.h"
#include "DataReference.h"
#include "FrameInfoData.h"
#include "InjectUserScriptImmediately.h"
#include "InjectedBundleScriptWorld.h"
#include "WebCompiledContentRuleList.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebUserContentControllerMessages.h"
#include "WebUserContentControllerProxyMessages.h"
#include <WebCore/DOMWrapperWorld.h>
#include <WebCore/Frame.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/SerializedScriptValue.h>
#include <WebCore/UserStyleSheet.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include <WebCore/UserMessageHandler.h>
#include <WebCore/UserMessageHandlerDescriptor.h>
#endif

namespace WebKit {
using namespace WebCore;

static HashMap<UserContentControllerIdentifier, WebUserContentController*>& userContentControllers()
{
    static NeverDestroyed<HashMap<UserContentControllerIdentifier, WebUserContentController*>> userContentControllers;

    return userContentControllers;
}

typedef HashMap<ContentWorldIdentifier, std::pair<RefPtr<InjectedBundleScriptWorld>, unsigned>> WorldMap;

static WorldMap& worldMap()
{
    static NeverDestroyed<WorldMap> map(std::initializer_list<WorldMap::KeyValuePairType> { { pageContentWorldIdentifier(), std::make_pair(&InjectedBundleScriptWorld::normalWorld(), 1) } });

    return map;
}

Ref<WebUserContentController> WebUserContentController::getOrCreate(UserContentControllerIdentifier identifier)
{
    auto& userContentControllerPtr = userContentControllers().add(identifier, nullptr).iterator->value;
    if (userContentControllerPtr)
        return *userContentControllerPtr;

    RefPtr<WebUserContentController> userContentController = adoptRef(new WebUserContentController(identifier));
    userContentControllerPtr = userContentController.get();

    return userContentController.releaseNonNull();
}

WebUserContentController::WebUserContentController(UserContentControllerIdentifier identifier)
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

InjectedBundleScriptWorld* WebUserContentController::worldForIdentifier(ContentWorldIdentifier identifier)
{
    auto iterator = worldMap().find(identifier);
    return iterator == worldMap().end() ? nullptr : iterator->value.first.get();
}

InjectedBundleScriptWorld* WebUserContentController::addContentWorld(const std::pair<ContentWorldIdentifier, String>& world)
{
    ASSERT(world.first);
    if (world.first == pageContentWorldIdentifier())
        return nullptr;
    
    auto addResult = worldMap().ensure(world.first, [&] {
#if PLATFORM(GTK) || PLATFORM(WPE)
        // The GLib API doesn't allow to create script worlds from the UI process. We need to
        // use the existing world created by the web extension if any. The world name is used
        // as the identifier.
        if (auto* existingWorld = InjectedBundleScriptWorld::find(world.second))
            return std::make_pair(Ref<InjectedBundleScriptWorld>(*existingWorld), 1);
#endif
        return std::make_pair(InjectedBundleScriptWorld::create(world.second, InjectedBundleScriptWorld::Type::User), 1);
    });
    
    if (addResult.isNewEntry)
        return addResult.iterator->value.first.get();
    return nullptr;
}

void WebUserContentController::addContentWorlds(const Vector<std::pair<ContentWorldIdentifier, String>>& worlds)
{
    for (auto& world : worlds)
        addContentWorld(world);
}

void WebUserContentController::removeContentWorlds(const Vector<ContentWorldIdentifier>& worldIdentifiers)
{
    for (auto& worldIdentifier : worldIdentifiers) {
        ASSERT(worldIdentifier);
        ASSERT(worldIdentifier != pageContentWorldIdentifier());

        auto it = worldMap().find(worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to remove a ContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier.toUInt64());
            return;
        }

        it->value.second--;
        
        if (!it->value.second)
            worldMap().remove(it);
    }
}

void WebUserContentController::addUserScripts(Vector<WebUserScriptData>&& userScripts, InjectUserScriptImmediately immediately)
{
    for (const auto& userScriptData : userScripts) {
        auto it = worldMap().find(userScriptData.worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to add a UserScript to a ContentWorld (id=%" PRIu64 ") that does not exist.", userScriptData.worldIdentifier.toUInt64());
            continue;
        }

        UserScript script = userScriptData.userScript;
        addUserScriptInternal(*it->value.first, userScriptData.identifier, WTFMove(script), immediately);
    }
}

void WebUserContentController::removeUserScript(ContentWorldIdentifier worldIdentifier, uint64_t userScriptIdentifier)
{
    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove a UserScript from a ContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier.toUInt64());
        return;
    }

    removeUserScriptInternal(*it->value.first, userScriptIdentifier);
}

void WebUserContentController::removeAllUserScripts(const Vector<ContentWorldIdentifier>& worldIdentifiers)
{
    for (auto& worldIdentifier : worldIdentifiers) {
        auto it = worldMap().find(worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to remove all UserScripts from a ContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier.toUInt64());
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
            WTFLogAlways("Trying to add a UserStyleSheet to a ContentWorld (id=%" PRIu64 ") that does not exist.", userStyleSheetData.worldIdentifier.toUInt64());
            continue;
        }
        
        UserStyleSheet sheet = userStyleSheetData.userStyleSheet;
        addUserStyleSheetInternal(*it->value.first, userStyleSheetData.identifier, WTFMove(sheet));
    }

    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void WebUserContentController::removeUserStyleSheet(ContentWorldIdentifier worldIdentifier, uint64_t userStyleSheetIdentifier)
{
    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove a UserStyleSheet from a ContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier.toUInt64());
        return;
    }

    removeUserStyleSheetInternal(*it->value.first, userStyleSheetIdentifier);
}

void WebUserContentController::removeAllUserStyleSheets(const Vector<ContentWorldIdentifier>& worldIdentifiers)
{
    bool sheetsChanged = false;
    for (auto& worldIdentifier : worldIdentifiers) {
        auto it = worldMap().find(worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to remove all UserStyleSheets from a ContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier.toUInt64());
            return;
        }

        if (m_userStyleSheets.remove(it->value.first.get()))
            sheetsChanged = true;
    }

    if (sheetsChanged)
        invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

#if ENABLE(USER_MESSAGE_HANDLERS)
class WebUserMessageHandlerDescriptorProxy : public WebCore::UserMessageHandlerDescriptor {
public:
    static Ref<WebUserMessageHandlerDescriptorProxy> create(WebUserContentController* controller, const String& name, InjectedBundleScriptWorld& world, uint64_t identifier)
    {
        return adoptRef(*new WebUserMessageHandlerDescriptorProxy(controller, name, world, identifier));
    }

    virtual ~WebUserMessageHandlerDescriptorProxy()
    {
    }

    uint64_t identifier() { return m_identifier; }

private:
    WebUserMessageHandlerDescriptorProxy(WebUserContentController* controller, const String& name, InjectedBundleScriptWorld& world, uint64_t identifier)
        : WebCore::UserMessageHandlerDescriptor(name, world.coreWorld())
        , m_controller(controller)
        , m_identifier(identifier)
    {
    }

    // WebCore::UserMessageHandlerDescriptor
    void didPostMessage(WebCore::UserMessageHandler& handler, WebCore::SerializedScriptValue* value, WTF::Function<void(SerializedScriptValue*, const String&)>&& completionHandler) override
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

        auto messageReplyHandler = [completionHandler = WTFMove(completionHandler)](const IPC::DataReference& resultValue, const String& errorMessage) {
            if (!errorMessage.isNull()) {
                completionHandler(nullptr, errorMessage);
                return;
            }

            auto value = SerializedScriptValue::createFromWireBytes(resultValue.vector());
            completionHandler(value.ptr(), { });
        };

        WebProcess::singleton().parentProcessConnection()->sendWithAsyncReply(Messages::WebUserContentControllerProxy::DidPostMessage(webPage->webPageProxyIdentifier(), webFrame->info(), m_identifier, IPC::DataReference(value->data())), WTFMove(messageReplyHandler), m_controller->identifier());
    }

    RefPtr<WebUserContentController> m_controller;
    uint64_t m_identifier;
};
#endif

void WebUserContentController::addUserScriptMessageHandlers(const Vector<WebScriptMessageHandlerData>& scriptMessageHandlers)
{
#if ENABLE(USER_MESSAGE_HANDLERS)
    for (auto& handler : scriptMessageHandlers) {
        auto it = worldMap().find(handler.worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to add a UserScriptMessageHandler to a ContentWorld (id=%" PRIu64 ") that does not exist.", handler.worldIdentifier.toUInt64());
            continue;
        }

        addUserScriptMessageHandlerInternal(*it->value.first, handler.identifier, handler.name);
    }
#else
    UNUSED_PARAM(scriptMessageHandlers);
#endif
}

void WebUserContentController::removeUserScriptMessageHandler(ContentWorldIdentifier worldIdentifier, uint64_t userScriptMessageHandlerIdentifier)
{
#if ENABLE(USER_MESSAGE_HANDLERS)
    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove a UserScriptMessageHandler from a ContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier.toUInt64());
        return;
    }

    removeUserScriptMessageHandlerInternal(*it->value.first, userScriptMessageHandlerIdentifier);
#else
    UNUSED_PARAM(worldIdentifier);
    UNUSED_PARAM(userScriptMessageHandlerIdentifier);
#endif
}

void WebUserContentController::removeAllUserScriptMessageHandlers()
{
#if ENABLE(USER_MESSAGE_HANDLERS)
    if (m_userMessageHandlers.isEmpty())
        return;

    m_userMessageHandlers.clear();
    invalidateAllRegisteredUserMessageHandlerInvalidationClients();
#endif
}

void WebUserContentController::removeAllUserScriptMessageHandlersForWorlds(const Vector<ContentWorldIdentifier>& worldIdentifiers)
{
#if ENABLE(USER_MESSAGE_HANDLERS)
    bool userMessageHandlersChanged = false;
    for (auto& worldIdentifier : worldIdentifiers) {
        auto it = worldMap().find(worldIdentifier);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to remove all UserScriptMessageHandler from a ContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier.toUInt64());
            return;
        }

        if (m_userMessageHandlers.remove(it->value.first.get()))
            userMessageHandlersChanged = true;
    }

    if (userMessageHandlersChanged)
        invalidateAllRegisteredUserMessageHandlerInvalidationClients();
#else
    UNUSED_PARAM(worldIdentifiers);
#endif
}

#if ENABLE(USER_MESSAGE_HANDLERS)
void WebUserContentController::addUserScriptMessageHandlerInternal(InjectedBundleScriptWorld& world, uint64_t userScriptMessageHandlerIdentifier, const String& name)
{
    auto& messageHandlersInWorld = m_userMessageHandlers.ensure(&world, [] { return Vector<std::pair<uint64_t, RefPtr<WebUserMessageHandlerDescriptorProxy>>> { }; }).iterator->value;
    if (messageHandlersInWorld.findMatching([&](auto& pair) { return pair.first ==  userScriptMessageHandlerIdentifier; }) != notFound)
        return;
    messageHandlersInWorld.append(std::make_pair(userScriptMessageHandlerIdentifier, WebUserMessageHandlerDescriptorProxy::create(this, name, world, userScriptMessageHandlerIdentifier)));
}

void WebUserContentController::removeUserScriptMessageHandlerInternal(InjectedBundleScriptWorld& world, uint64_t userScriptMessageHandlerIdentifier)
{
    auto it = m_userMessageHandlers.find(&world);
    if (it == m_userMessageHandlers.end())
        return;

    auto protectedThis = makeRef(*this);

    auto& userMessageHandlers = it->value;
    bool userMessageHandlersChanged = userMessageHandlers.removeFirstMatching([userScriptMessageHandlerIdentifier](auto& pair) {
        return pair.first ==  userScriptMessageHandlerIdentifier;
    });

    if (!userMessageHandlersChanged)
        return;

    if (userMessageHandlers.isEmpty())
        m_userMessageHandlers.remove(it);

    invalidateAllRegisteredUserMessageHandlerInvalidationClients();
}
#endif

#if ENABLE(CONTENT_EXTENSIONS)
void WebUserContentController::addContentRuleLists(Vector<std::pair<String, WebCompiledContentRuleListData>>&& contentRuleLists)
{
    for (auto&& contentRuleList : contentRuleLists) {
        auto compiledContentRuleList = WebCompiledContentRuleList::create(WTFMove(contentRuleList.second));

        m_contentExtensionBackend.addContentExtension(contentRuleList.first, WTFMove(compiledContentRuleList));
    }
}

void WebUserContentController::removeContentRuleList(const String& name)
{
    m_contentExtensionBackend.removeContentExtension(name);
}

void WebUserContentController::removeAllContentRuleLists()
{
    m_contentExtensionBackend.removeAllContentExtensions();
}
#endif

void WebUserContentController::addUserScriptInternal(InjectedBundleScriptWorld& world, const std::optional<uint64_t>& userScriptIdentifier, UserScript&& userScript, InjectUserScriptImmediately immediately)
{
    if (immediately == InjectUserScriptImmediately::Yes) {
        Page::forEachPage([&] (auto& page) {
            if (&page.userContentProvider() != this)
                return;

            auto& mainFrame = page.mainFrame();
            if (userScript.injectedFrames() == UserContentInjectedFrames::InjectInTopFrameOnly) {
                mainFrame.injectUserScriptImmediately(world.coreWorld(), userScript);
                return;
            }

            for (auto* frame = &mainFrame; frame; frame = frame->tree().traverseNext(&mainFrame))
                frame->injectUserScriptImmediately(world.coreWorld(), userScript);
        });
    }

    auto& scriptsInWorld = m_userScripts.ensure(&world, [] { return Vector<std::pair<std::optional<uint64_t>, WebCore::UserScript>>(); }).iterator->value;
    if (userScriptIdentifier && scriptsInWorld.findMatching([&](auto& pair) { return pair.first == userScriptIdentifier; }) != notFound)
        return;

    scriptsInWorld.append(std::make_pair(userScriptIdentifier, WTFMove(userScript)));
}

void WebUserContentController::addUserScript(InjectedBundleScriptWorld& world, UserScript&& userScript)
{
    addUserScriptInternal(world, std::nullopt, WTFMove(userScript), InjectUserScriptImmediately::No);
}

void WebUserContentController::removeUserScriptWithURL(InjectedBundleScriptWorld& world, const URL& url)
{
    auto it = m_userScripts.find(&world);
    if (it == m_userScripts.end())
        return;

    auto& scripts = it->value;
    scripts.removeAllMatching([&](auto& pair) {
        return pair.second.url() == url;
    });

    if (scripts.isEmpty())
        m_userScripts.remove(it);
}

void WebUserContentController::removeUserScriptInternal(InjectedBundleScriptWorld& world, uint64_t userScriptIdentifier)
{
    auto it = m_userScripts.find(&world);
    if (it == m_userScripts.end())
        return;

    auto& scripts = it->value;
    scripts.removeFirstMatching([userScriptIdentifier](auto& pair) {
        return pair.first == userScriptIdentifier;
    });

    if (scripts.isEmpty())
        m_userScripts.remove(it);
}

void WebUserContentController::removeUserScripts(InjectedBundleScriptWorld& world)
{
    m_userScripts.remove(&world);
}

void WebUserContentController::addUserStyleSheetInternal(InjectedBundleScriptWorld& world, const std::optional<uint64_t>& userStyleSheetIdentifier, UserStyleSheet&& userStyleSheet)
{
    auto& styleSheetsInWorld = m_userStyleSheets.ensure(&world, [] { return Vector<std::pair<std::optional<uint64_t>, WebCore::UserStyleSheet>>(); }).iterator->value;
    if (userStyleSheetIdentifier && styleSheetsInWorld.findMatching([&](auto& pair) { return pair.first == userStyleSheetIdentifier; }) != notFound)
        return;

    if (auto pageID = userStyleSheet.pageID()) {
        if (auto* webPage = WebProcess::singleton().webPage(*pageID)) {
            if (auto* page = webPage->corePage())
                page->injectUserStyleSheet(userStyleSheet);
        }
    }

    styleSheetsInWorld.append(std::make_pair(userStyleSheetIdentifier, WTFMove(userStyleSheet)));
}

void WebUserContentController::addUserStyleSheet(InjectedBundleScriptWorld& world, UserStyleSheet&& userStyleSheet)
{
    addUserStyleSheetInternal(world, std::nullopt, WTFMove(userStyleSheet));
    invalidateInjectedStyleSheetCacheInAllFramesInAllPages();
}

void WebUserContentController::removeUserStyleSheetWithURL(InjectedBundleScriptWorld& world, const URL& url)
{
    auto it = m_userStyleSheets.find(&world);
    if (it == m_userStyleSheets.end())
        return;

    auto& stylesheets = it->value;
    bool sheetsChanged = stylesheets.removeAllMatching([&](auto& pair) {
        return pair.second.url() == url;
    });

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

    bool sheetsChanged = stylesheets.removeFirstMatching([&](auto& pair) {
        if (pair.first != userStyleSheetIdentifier)
            return false;

        auto& userStyleSheet = pair.second;
        if (auto pageID = userStyleSheet.pageID()) {
            if (auto* webPage = WebProcess::singleton().webPage(*pageID)) {
                if (auto* page = webPage->corePage())
                    page->removeInjectedUserStyleSheet(userStyleSheet);
            }
        }
        return true;
    });

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

void WebUserContentController::forEachUserScript(Function<void(WebCore::DOMWrapperWorld&, const WebCore::UserScript&)>&& functor) const
{
    for (const auto& worldAndUserScriptVector : m_userScripts) {
        auto& world = worldAndUserScriptVector.key->coreWorld();
        for (const auto& identifierUserScriptPair : worldAndUserScriptVector.value)
            functor(world, identifierUserScriptPair.second);
    }
}

void WebUserContentController::forEachUserStyleSheet(Function<void(const WebCore::UserStyleSheet&)>&& functor) const
{
    for (auto& styleSheetVector : m_userStyleSheets.values()) {
        for (const auto& identifierUserStyleSheetPair : styleSheetVector)
            functor(identifierUserStyleSheetPair.second);
    }
}

#if ENABLE(USER_MESSAGE_HANDLERS)
void WebUserContentController::forEachUserMessageHandler(Function<void(const WebCore::UserMessageHandlerDescriptor&)>&& functor) const
{
    for (auto& userMessageHandlerVector : m_userMessageHandlers.values()) {
        for (auto& pair : userMessageHandlerVector)
            functor(*pair.second.get());
    }
}
#endif

} // namespace WebKit
