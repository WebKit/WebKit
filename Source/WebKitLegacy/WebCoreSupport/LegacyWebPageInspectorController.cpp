/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "LegacyWebPageInspectorController.h"

#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorTarget.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameInspectorController.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>
#include <wtf/Compiler.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakRef.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

namespace {

class FrontendChannel final : public Inspector::FrontendChannel {
    WTF_MAKE_TZONE_ALLOCATED(FrontendChannel);
    WTF_MAKE_NONCOPYABLE(FrontendChannel);
public:
    using MessageHandler = Function<void(const String& targetID, const String& message)>;

    FrontendChannel(String&& targetID, Inspector::FrontendChannel::ConnectionType connectionType, MessageHandler& handler)
        : m_targetID(WTF::move(targetID))
        , m_connectionType(connectionType)
        , m_handler(handler)
    {
    }

    ~FrontendChannel() = default;

    ConnectionType connectionType() const override { return m_connectionType; }

    void sendMessageToFrontend(const String& message) override
    {
        m_handler(m_targetID, message);
    }

private:
    String m_targetID;
    Inspector::FrontendChannel::ConnectionType m_connectionType;
    MessageHandler& m_handler;
};

class PageTarget final : public Inspector::InspectorTarget {
    WTF_MAKE_TZONE_ALLOCATED(PageTarget);
    WTF_MAKE_NONCOPYABLE(PageTarget);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageTarget);
public:
    static String identifier(const WebCore::Page& page)
    {
        return makeString("page-"_s, page.identifier()->toUInt64());
    }

    PageTarget(WebCore::Page& page, FrontendChannel::MessageHandler&& handler)
        : m_page(page)
        , m_handler(WTF::move(handler))
    {
    }

    Inspector::InspectorTargetType type() const final
    {
        return Inspector::InspectorTargetType::Page;
    }

    String identifier() const final
    {
        return identifier(Ref { m_page.get() });
    }

    void connect(Inspector::FrontendChannel::ConnectionType connectionType) override
    {
        if (m_channel)
            return;

        m_channel = makeUnique<FrontendChannel>(identifier(), connectionType, m_handler);
        protect(protect(m_page)->inspectorController())->connectFrontend(*m_channel);
    }

    void disconnect() override
    {
        if (!m_channel)
            return;

        protect(protect(m_page)->inspectorController())->disconnectFrontend(*m_channel);
        m_channel = nullptr;
    }

    void sendMessageToTargetBackend(const String& message) override
    {
        protect(protect(m_page)->inspectorController())->dispatchMessageFromFrontend(message);
    }

private:
    WeakRef<WebCore::Page> m_page;
    std::unique_ptr<FrontendChannel> m_channel;
    FrontendChannel::MessageHandler m_handler;
};

class FrameTarget final : public Inspector::InspectorTarget {
    WTF_MAKE_TZONE_ALLOCATED(FrameTarget);
    WTF_MAKE_NONCOPYABLE(FrameTarget);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameTarget);
public:
    static String identifier(const WebCore::LocalFrame& frame)
    {
        return makeString("frame-"_s, frame.frameID().toUInt64());
    }

    FrameTarget(WebCore::LocalFrame& frame, FrontendChannel::MessageHandler&& handler)
        : m_frame(frame)
        , m_handler(WTF::move(handler))
    {
    }

    Inspector::InspectorTargetType type() const final
    {
        return Inspector::InspectorTargetType::Frame;
    }

    String identifier() const final
    {
        return identifier(Ref { m_frame.get() });
    }

    void connect(Inspector::FrontendChannel::ConnectionType connectionType) override
    {
        if (m_channel)
            return;

        m_channel = makeUnique<FrontendChannel>(identifier(), connectionType, m_handler);
        protect(Ref { m_frame.get() }->inspectorController())->connectFrontend(*m_channel);
    }

    void disconnect() override
    {
        if (!m_channel)
            return;

        protect(Ref { m_frame.get() }->inspectorController())->disconnectFrontend(*m_channel);
        m_channel = nullptr;
    }

    void sendMessageToTargetBackend(const String& message) override
    {
        protect(Ref { m_frame.get() }->inspectorController())->dispatchMessageFromFrontend(message);
    }

private:
    WeakRef<WebCore::LocalFrame> m_frame;
    std::unique_ptr<FrontendChannel> m_channel;
    FrontendChannel::MessageHandler m_handler;
};

class EmptyBrowserAgent final : public Inspector::InspectorAgentBase, public Inspector::BrowserBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(EmptyBrowserAgent);
    WTF_MAKE_TZONE_ALLOCATED(EmptyBrowserAgent);
public:
    explicit EmptyBrowserAgent(Inspector::BackendDispatcher& backendDispatcher)
        : Inspector::InspectorAgentBase("Browser"_s)
        , m_backendDispatcher(Inspector::BrowserBackendDispatcher::create(backendDispatcher, this))
    {
    }

    // InspectorAgentBase
    void didCreateFrontendAndBackend() final { }
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) final { }

    // BrowserBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> enable() final { return { }; }
    Inspector::Protocol::ErrorStringOr<void> disable() final { return { }; }

private:
    const Ref<Inspector::BrowserBackendDispatcher> m_backendDispatcher;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrontendChannel);
WTF_MAKE_TZONE_ALLOCATED_IMPL(PageTarget);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameTarget);
WTF_MAKE_TZONE_ALLOCATED_IMPL(EmptyBrowserAgent);

} // namespace

WTF_MAKE_TZONE_ALLOCATED_IMPL(LegacyWebPageInspectorController);

Ref<LegacyWebPageInspectorController> LegacyWebPageInspectorController::create(WebCore::Page& page)
{
    return adoptRef(*new LegacyWebPageInspectorController(page));
}

LegacyWebPageInspectorController::LegacyWebPageInspectorController(WebCore::Page& page)
    : m_frontendRouter(Inspector::FrontendRouter::create())
    , m_backendDispatcher(Inspector::BackendDispatcher::create(m_frontendRouter.copyRef()))
{
    UniqueRef targetAgent = makeUniqueRef<Inspector::InspectorTargetAgent>(m_frontendRouter, m_backendDispatcher);
    m_targetAgent = targetAgent.ptr();
    m_agents.append(WTF::move(targetAgent));

    m_agents.append(makeUniqueRef<EmptyBrowserAgent>(m_backendDispatcher));

    // In WK1, the Page object persists for the entire lifetime of the WebView and is never recreated during navigation.
    // (Unlike WK2, there is no Process Swap On Navigation (PSON) that would require recreating the PageTarget.)
    addTarget(makeUnique<PageTarget>(page, [weakThis = WeakPtr { *this }](const String& targetID, const String& message) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sendMessageToInspectorFrontend(targetID, message);
    }));
}

void LegacyWebPageInspectorController::frameCreated(WebCore::LocalFrame& frame)
{
    addTarget(makeUnique<FrameTarget>(frame, [weakThis = WeakPtr { *this }](const String& targetID, const String& message) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sendMessageToInspectorFrontend(targetID, message);
    }));
}

void LegacyWebPageInspectorController::willDestroyFrame(const WebCore::LocalFrame& frame)
{
    removeTarget(FrameTarget::identifier(frame));
}

void LegacyWebPageInspectorController::willDestroyPage(const WebCore::Page& page)
{
    removeTarget(PageTarget::identifier(page));

    disconnectAllFrontends();
    m_agents.discardValues();
}

void LegacyWebPageInspectorController::addTarget(std::unique_ptr<Inspector::InspectorTarget>&& target)
{
    protect(targetAgent())->targetCreated(*target);
    m_targets.set(target->identifier(), WTF::move(target));
}

void LegacyWebPageInspectorController::removeTarget(const String& targetID)
{
    auto it = m_targets.find(targetID);
    if (it == m_targets.end())
        return;

    protect(targetAgent())->targetDestroyed(protect(*it->value));
    m_targets.remove(it);
}

void LegacyWebPageInspectorController::connectFrontend(Inspector::FrontendChannel& frontendChannel)
{
    bool connectingFirstFrontend = !m_frontendRouter->hasFrontends();
    m_frontendRouter->connectFrontend(frontendChannel);
    if (connectingFirstFrontend)
        m_agents.didCreateFrontendAndBackend();
}

void LegacyWebPageInspectorController::disconnectFrontend(Inspector::FrontendChannel& frontendChannel)
{
    m_frontendRouter->disconnectFrontend(frontendChannel);
    bool disconnectingLastFrontend = !m_frontendRouter->hasFrontends();
    if (disconnectingLastFrontend)
        m_agents.willDestroyFrontendAndBackend(Inspector::DisconnectReason::InspectorDestroyed);
}

bool LegacyWebPageInspectorController::hasLocalFrontend() const
{
    return m_frontendRouter->hasLocalFrontend();
}

void LegacyWebPageInspectorController::disconnectAllFrontends()
{
    if (!m_frontendRouter->hasFrontends())
        return;

    m_agents.willDestroyFrontendAndBackend(Inspector::DisconnectReason::InspectedTargetDestroyed);
    m_frontendRouter->disconnectAllFrontends();
}

void LegacyWebPageInspectorController::dispatchMessageFromFrontend(const String& message)
{
    callOnMainThread([backendDispatcher = Ref { m_backendDispatcher }, message = message.isolatedCopy()] {
        backendDispatcher->dispatch(message);
    });
}

void LegacyWebPageInspectorController::sendMessageToInspectorFrontend(const String& targetID, const String& message)
{
    protect(targetAgent())->sendMessageFromTargetToFrontend(targetID, message);
}
