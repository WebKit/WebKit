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
#include <WebCore/ScriptController.h>
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

PassRefPtr<WebUserContentController> WebUserContentController::getOrCreate(uint64_t identifier)
{
    auto& userContentControllerPtr = userContentControllers().add(identifier, nullptr).iterator->value;
    if (userContentControllerPtr)
        return userContentControllerPtr;

    RefPtr<WebUserContentController> userContentController = adoptRef(new WebUserContentController(identifier));
    userContentControllerPtr = userContentController.get();

    return userContentController.release();
}

WebUserContentController::WebUserContentController(uint64_t identifier)
    : m_identifier(identifier)
    , m_userContentController(UserContentController::create())
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

void WebUserContentController::removeUserContentWorld(uint64_t worldIdentifier)
{
    ASSERT(worldIdentifier);
    ASSERT(worldIdentifier != 1);

    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove a UserContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier);
        return;
    }

    it->value.second--;
    
    if (!it->value.second)
        worldMap().remove(it);
}

void WebUserContentController::addUserScripts(const Vector<std::pair<uint64_t, WebCore::UserScript>>& userScripts)
{
    for (const auto& userScriptWorldPair : userScripts) {
        auto it = worldMap().find(userScriptWorldPair.first);
        if (it == worldMap().end()) {
            WTFLogAlways("Trying to add a UserScript to a UserContentWorld (id=%" PRIu64 ") that does not exist.", userScriptWorldPair.first);
            continue;
        }

        m_userContentController->addUserScript(it->value.first->coreWorld(), std::make_unique<WebCore::UserScript>(userScriptWorldPair.second));
    }
}

void WebUserContentController::removeUserScript(uint64_t worldIdentifier, const String& urlString)
{
    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove a UserScript from a UserContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier);
        return;
    }

    m_userContentController->removeUserScript(it->value.first->coreWorld(), URL(URL(), urlString));
}

void WebUserContentController::removeAllUserScripts(uint64_t worldIdentifier)
{
    auto it = worldMap().find(worldIdentifier);
    if (it == worldMap().end()) {
        WTFLogAlways("Trying to remove all UserScripts from a UserContentWorld (id=%" PRIu64 ") that does not exist.", worldIdentifier);
        return;
    }

    m_userContentController->removeUserScripts(it->value.first->coreWorld());
}

void WebUserContentController::addUserStyleSheets(const Vector<WebCore::UserStyleSheet>& userStyleSheets)
{
    for (const auto& userStyleSheet : userStyleSheets) {
        m_userContentController->addUserStyleSheet(mainThreadNormalWorld(),
            std::make_unique<WebCore::UserStyleSheet>(userStyleSheet), InjectInExistingDocuments);
    }
}

void WebUserContentController::removeUserStyleSheet(const String& urlString)
{
    m_userContentController->removeUserStyleSheet(mainThreadNormalWorld(), URL(URL(), urlString));
}

void WebUserContentController::removeAllUserStyleSheets()
{
    m_userContentController->removeUserStyleSheets(mainThreadNormalWorld());
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
        m_userContentController->addUserMessageHandlerDescriptor(descriptor->descriptor());
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
    
    m_userContentController->removeUserMessageHandlerDescriptor(it->value->descriptor());
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
        m_userContentController->addUserContentExtension(userContentExtension.first, compiledContentExtension);
    }
}

void WebUserContentController::removeUserContentExtension(const String& name)
{
    m_userContentController->removeUserContentExtension(name);
}

void WebUserContentController::removeAllUserContentExtensions()
{
    m_userContentController->removeAllUserContentExtensions();
}
#endif

} // namespace WebKit
